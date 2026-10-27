"""Writes a game's <prefix>.syms.toml and <prefix>.datasyms.toml (syms/marioparty.us, say) from its decomp's US ELF with
N64Recomp's own context dump (--dump-context), the way the MarioKart64RecompSyms and Starfox64RecompSyms repos were
made. Each game's tools/gen_syms.sh runs it.

The ELF is the game's tools/build_decomp.sh's, as the decomp links it. KMC gcc puts every function, static ones too,
in the symbol table, and splat sizes every function of the assembly, so the ELF alone gives N64Recomp each function
with its size. Most are named by their address (splat's func_8001234), the rest as the decomp named them.

N64Recomp reads each section's VRAM from the ELF's section headers and its ROM offset from the load address the
decomp's linker script gives it, which is the ROM's: each decomp builds its ROM uncompressed, byte for byte. Each code
section's bytes are checked against the ROM's there.

A function that starts with a nop (the compiler's, before a mult or div that would follow too closely on the caller's
mflo or mfhi) has that nop sized into the function before it, as padding, and is named and placed one word later.
Mario Party's Crazy Cutter calls one that way, through a pointer to its nop, which the runtime then finds no function
at. So where a section's code takes the address of such a nop (a jal to it, or a lui and an addiu making it), the
function starts there.

Lines after <prefix> go into N64Recomp's configuration as they are: a game's function_sizes, say, for a symbol its
decomp gives no size.

Usage: py -3 -I gen_syms.py <N64Recomp> <elf> <rom> <prefix> [<configuration line>...]
"""
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

# The decomps' entrypoint (asm/entrypoint.s), the ROM header's: the boot code copies 1 MB from ROM 0x1000 there.
ENTRYPOINT = 0x80000400


def elf_sections(path):
    """Each section's bytes in the ELF, by name."""
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 2:
        sys.exit(f"{path}: not a 32-bit big-endian ELF")
    e_shoff, = struct.unpack_from(">I", data, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(">HHH", data, 0x2E)
    headers = [struct.unpack_from(">IIIIIIIIII", data, e_shoff + i * e_shentsize) for i in range(e_shnum)]
    names = headers[e_shstrndx][4]
    return {data[names + h[0]:data.index(b"\0", names + h[0])].decode(): data[h[4]:h[4] + h[5]] for h in headers}


def check_rom_offsets(dump, sections, rom):
    """Each [[section]]'s bytes are the ROM's at its rom."""
    for name, offset in re.findall(r'name = "([^"]+)"\nrom = (0x[0-9A-F]+)', dump):
        code = sections[name]
        if rom[int(offset, 16):int(offset, 16) + len(code)] != code:
            sys.exit(f"{name}: the ROM's bytes at {offset} aren't the ELF's")


def entry_nops(dump, sections):
    """The dump with each function that starts with a nop (above) starting at that nop."""
    moved = []

    def section(match):
        block = match.group(0)
        name, vram = match.group(1), int(match.group(2), 16)
        code = sections[name]
        word = lambda a: struct.unpack_from(">I", code, a - vram)[0]
        funcs = {int(v, 16): int(n, 16) for v, n in re.findall(r"vram = (0x[0-9A-F]+), size = (0x[0-9A-F]+)", block)}
        # The addresses its code calls or makes: jal targets, and lui then addiu from the lui's register.
        made = set()
        for start, size in funcs.items():
            his = {}
            for a in range(start, start + size, 4):
                x = word(a)
                op, rs, rt, imm = x >> 26, (x >> 21) & 31, (x >> 16) & 31, x & 0xFFFF
                if op == 3:
                    made.add((a & 0xF0000000) | (x & 0x3FFFFFF) << 2)
                elif op == 15:
                    his[rt] = imm << 16
                elif op == 9 and rs in his:
                    made.add((his[rs] + imm - (imm & 0x8000) * 2) & 0xFFFFFFFF)
        for start, size in sorted(funcs.items()):
            before = next((s for s, n in funcs.items() if s + n == start), None)
            if before is not None and start - 4 in made and start - 4 not in funcs and word(start - 4) == 0:
                block = block.replace(f"vram = 0x{before:08X}, size = 0x{funcs[before]:X} ",
                                      f"vram = 0x{before:08X}, size = 0x{funcs[before] - 4:X} ")
                block = block.replace(f"vram = 0x{start:08X}, size = 0x{size:X} ",
                                      f"vram = 0x{start - 4:08X}, size = 0x{size + 4:X} ")
                moved.append(f"{name} 0x{start:08X}")
        return block

    dump = re.sub(r'\[\[section\]\]\nname = "([^"]+)"\nrom = 0x[0-9A-F]+\nvram = (0x[0-9A-F]+)\n(?:(?!\[\[section).*\n?)*',
                  section, dump)
    return dump, moved


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__.strip().splitlines()[-1])
    n64recomp, elf, rom, prefix = (Path(a).resolve() for a in sys.argv[1:5])
    with tempfile.TemporaryDirectory() as tmp:
        config = Path(tmp) / "context.toml"
        config.write_text(
            "[input]\n"
            f"entrypoint = 0x{ENTRYPOINT:08X}\n"
            f'elf_path = "{elf.as_posix()}"\n'
            'output_func_path = "out"\n'
            # The decomps' bss sections are named <segment>_bss (splat's linker scripts).
            'bss_section_suffix = "_bss"\n'
            + "".join(line + "\n" for line in sys.argv[5:]))
        run = subprocess.run([str(n64recomp), config.name, "--dump-context"], cwd=tmp, capture_output=True, text=True)
        if run.returncode != 0:
            sys.exit((run.stdout + run.stderr)[-3000:])
        dump = (Path(tmp) / "dump.toml").read_text()
        check_rom_offsets(dump, elf_sections(elf), rom.read_bytes())
        dump, moved = entry_nops(dump, elf_sections(elf))
        print("started at their nop: " + (", ".join(moved) or "none"))
        syms = prefix.with_name(prefix.name + ".syms.toml")
        datasyms = prefix.with_name(prefix.name + ".datasyms.toml")
        prefix.parent.mkdir(exist_ok=True)
        syms.write_text(dump, newline="\n")
        datasyms.write_text((Path(tmp) / "data_dump.toml").read_text(), newline="\n")
    print(f"{dump.count('[[section]]')} sections, {dump.count('{ name = ')} functions; wrote {syms} and {datasyms}")


if __name__ == "__main__":
    main()
