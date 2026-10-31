# Mods

Each game's desktop build loads N64Recomp mods (`.nrm` files), made for it as for any N64 recompilation built on
N64ModernRuntime: a mod replaces or hooks the game's functions by their names in the game's decompilation. This page
says how a player installs one and how to make one. Mods a player adds don't load on a game's page in a browser: the
page has no Mods option, since the runtime compiles a mod's code to machine code as it loads it, and a page can't run
machine code it makes.

## Installing a mod

In the launcher, before Start Game, open Mods and press Install Mods, or drop the `.nrm` onto the window. Or put the
file in the game's `mods` folder (Open Mods Folder there opens it), in the folder the game keeps its settings and
saves in:

| Game          | Windows                                      | Linux                                     |
|---------------|----------------------------------------------|-------------------------------------------|
| Mario Party   | `%LOCALAPPDATA%\MarioPartyRecompiled\mods`   | `~/.config/MarioPartyRecompiled/mods`     |
| Mario Party 2 | `%LOCALAPPDATA%\MarioParty2Recompiled\mods`  | `~/.config/MarioParty2Recompiled/mods`    |
| Mario Party 3 | `%LOCALAPPDATA%\MarioParty3Recompiled\mods`  | `~/.config/MarioParty3Recompiled/mods`    |

With a `portable.txt` in the folder the game is started from, the game keeps them in that folder instead. Turn a mod
on in the Mods list: the mods turned on load when the game starts.

## Making a mod

A mod is C compiled for the N64's MIPS, linked into an ELF, and packed with its manifest by N64Recomp's
`RecompModTool`. The layout is Majora's Mask: Recompiled's
[MMRecompModTemplate](https://github.com/Zelda64Recomp/MMRecompModTemplate): its `include/modding.h` (the
`RECOMP_PATCH`, `RECOMP_FORCE_PATCH`, `RECOMP_HOOK`, `RECOMP_HOOK_RETURN`, `RECOMP_IMPORT` and `RECOMP_EXPORT` a mod
is written with), its `mod.ld` and its `mod.toml`, without its Majora's Mask parts (`mm-decomp`, `Zelda64RecompSyms`
and the headers for them). What each game gives a mod:

| Game          | `game_id`     | Its symbols (the game's repo)                                              | Its decompilation                                                |
|---------------|---------------|----------------------------------------------------------------------------|------------------------------------------------------------------|
| Mario Party   | `marioparty`  | `syms/marioparty.us.syms.toml`, `syms/marioparty.us.datasyms.toml`         | [mariopartyrd/marioparty](https://github.com/mariopartyrd/marioparty)   |
| Mario Party 2 | `marioparty2` | `syms/marioparty2.us.syms.toml`, `syms/marioparty2.us.datasyms.toml`       | [mariopartyrd/marioparty2](https://github.com/mariopartyrd/marioparty2) |
| Mario Party 3 | `marioparty3` | `syms/marioparty3.us.syms.toml`, `syms/marioparty3.us.datasyms.toml`       | [mariopartyrd/marioparty3](https://github.com/mariopartyrd/marioparty3) |

1. In `mod.toml`, `game_id` is the game's, `minimum_recomp_version` the release the mod needs (a game's version is
   its `version_string` in `src/main/main.cpp`), `func_reference_syms_file` the game's `syms.toml` and
   `data_reference_syms_files` its `datasyms.toml`. Each game plays only the USA ROM, so there is one of each.
2. Write the mod against the game's decompilation, the copy its repo pins in `lib/` (`lib/marioparty`,
   `lib/marioparty2`, `lib/marioparty3`): a function or a variable is named as the game's symbols name it, the
   decompilation's own names, with an overlay's (a board, a minigame, a menu) ending in its overlay's name
   (`func_80102A04_36DBA4_BootLogos`).
3. Compile it with a clang that has a MIPS backend, with the flags the game's `patches/Makefile` uses (its `CFLAGS`,
   and its `CPPFLAGS` with the decompilation's `include/` and `src/` on the path) and `-ffunction-sections`, and link
   it with ld.lld and `mod.ld` as the template's `Makefile` does.
4. Build `RecompModTool` from the N64Recomp the game pins (`lib/N64ModernRuntime/N64Recomp`, the CMake target
   `RecompModTool`), so the mod is written in the format the game's runtime reads, and run
   `RecompModTool mod.toml build`: it writes `build/<mod_filename>.nrm`.

What a mod can reach:

- The game's code, through `RECOMP_PATCH` (the mod's function in place of the game's) and `RECOMP_HOOK` and
  `RECOMP_HOOK_RETURN` (the mod's function called as the game's starts or returns), in the game's main code and in
  every overlay.
- Not a function the game's own patches already replace (its `patches/*.c` and this repo's `patches/`: the process
  scheduler, a camera's view, a board's background, the controllers' setup): `RECOMP_PATCH` on one of those stops
  the mod from loading. `RECOMP_FORCE_PATCH` replaces it anyway, and the patch's fix goes with it (a board drawn at
  4:3, the scheduler's processes no longer on their own threads), so a mod that does has to carry that fix itself.
  Hooks on those functions work. A function the game hooks in its `us.toml` loses those hooks when a mod replaces
  it, since the mod's code runs in place of the whole recompiled function, so prefer a hook of the mod's own there.
- The runtime's functions, imported from `"*"` with `RECOMP_IMPORT` (memory, `recomp_alloc` and `recomp_free`, and
  the mod's own options, `recomp_get_config_u32` and the rest), and other mods' exports. The games export nothing of
  their own to mods.

### An example

This mod for Mario Party 2 goes from the boot logos to the game's debug menu (overlay 0) in place of the title screen
(overlay 0x62), by replacing the boot logos' last step with the decompilation's own code
(`src/overlays/ovl_57_BootLogos/36D9D0.c`) with the overlay changed. It needs only `include/modding.h`, so it declares
what it uses rather than including the decompilation's headers. Built as above and installed, it boots Mario Party 2's
desktop build into the debug menu's minigame list.

`mod.toml`, beside the game's two symbol files:

```toml
[manifest]
id = "mp2_boot_to_debug"
version = "1.0.0"
display_name = "Boot to the debug menu"
description = "Goes from the boot logos to the game's debug menu in place of the title screen."
short_description = "Boot to the debug menu."
authors = [ "Your name" ]
game_id = "marioparty2"
minimum_recomp_version = "0.1.0"
dependencies = []
optional_dependencies = []
native_libraries = []

[inputs]
elf_path = "build/mod.elf"
mod_filename = "mp2_boot_to_debug"
func_reference_syms_file = "marioparty2.us.syms.toml"
data_reference_syms_files = [ "marioparty2.us.datasyms.toml" ]
additional_files = []
```

`src/mod.c`:

```c
#include "modding.h"

typedef int s32;
typedef short s16;
typedef unsigned short u16;

extern s32 func_8008F618_90218(void);
extern void omOvlGotoEx(s32 overlay, s16 event, u16 stat);
extern s32 omOvlCallEx(s32 overlay, s16 event, s16 stat);
extern void omOvlHisChg(s16 level, s32 overlay, s16 event, s16 stat);
extern s32 D_80102D50_36DEF0_BootLogos;

RECOMP_PATCH void func_80102A04_36DBA4_BootLogos(void) {
    if (func_8008F618_90218() == 0) {
        if (D_80102D50_36DEF0_BootLogos != 0) {
            omOvlGotoEx(0x00, 1, 0x193);
            return;
        }
        omOvlCallEx(0x00, 1, 0x193);
        omOvlHisChg(1, 0x00, 0, 0x192);
    }
}
```
