# Mario Party engine

> **Made with AI.** Almost all of the code, documentation and tests in this repo were written by AI agents
> (Claude), directed and play-tested by a person. None of it is offered to the upstream projects it builds on, which
> don't take AI-written code.

The code the static recompilations of the N64 Mario Party games share, written once:
[Mario Party: Recompiled](https://github.com/n64hub/mario-party-recomp),
[Mario Party 2: Recompiled](https://github.com/n64hub/mario-party-2-recomp) and
[Mario Party 3: Recompiled](https://github.com/n64hub/mario-party-3-recomp). The three games are built on Hudson's
engine, and their decompilations name the same code differently, so what is here is written against a short header of
each game's names. Each game has this repo as a submodule, `lib/mario-party-engine`, pinned at a commit.

It holds no ROM and nothing made from one, and builds only as part of a game.

## What is here

- `src/process_threads.cpp` and `patches/process.c`: the engine's process scheduler on native threads, one per live
  process, run one at a time, on the desktop and in a browser alike ([docs/processes.md](docs/processes.md)).
- `tools/gen_syms.py`: writes a game's `syms/` (the symbols N64Recomp reads) from its decompilation's ELF; the game's
  `tools/gen_syms.sh` runs it.

Anything else the games share comes here when a change needs it in more than one of them.

## How a game uses it

- `CMakeLists.txt` compiles `lib/mario-party-engine/src/process_threads.cpp` into the game.
- `patches/Makefile` compiles `lib/mario-party-engine/patches/*.c` with the game's own patches, against the game's
  decompilation's headers and its `patches/patches.h`.
- The game's `patches/engine_names.h` includes the decompilation's headers the patches need, declares what they leave
  out, and names what the engine's patches use:

  | Name             | What it is                       | Mario Party       | Mario Party 2         | Mario Party 3            |
  |------------------|----------------------------------|-------------------|-----------------------|--------------------------|
  | `PRC_SYS_INIT`   | the scheduler's init             | `HuPrcInit`       | `HuPrcInit`           | `HuPrcSysInit`           |
  | `PRC_CHILD_WAIT` | a process waits for its children | `HuPrcChildWatch` | `HuPrcChildWatch`     | `HuPrcChildWait`         |
  | `PRC_END`        | a process ends itself            | `HuPrcEnd`        | `HuPrcEnd`            | `HuPrcExit`              |
  | `PRC_UNLINK`     | takes a process off the list     | `HuUnlinkPrc`     | `HuUnlinkPrc`         | `UnlinkProcess_0004F7C8` |
  | `PRC_HEAP_FREE`  | frees an ended process's heap    | `HuMemDirectFree` | `HuMemMemoryFreePerm` | `HuMemMemoryFreePerm`    |
  | `PRC_TOP`        | the list's first process         | `processtop`      | `processtop`          | `top_process`            |
  | `PRC_CURRENT`    | the process running              | `processcur`      | `processcur`          | `current_process`        |
  | `PRC_COUNT`      | how many are on the list         | `processcnt`      | `processcnt`          | `process_count`          |
  | `PRC_SLEEP_LOC`  | where one last slept (optional)  |                   |                       | `sPrcSleepLoc`           |

  `HuPrcCall`, `HuPrcSleep`, `HuPrcTerminate`, `HuPrcCurrentGet`, `Process` and its fields, and the `EXEC_PROCESS_*`
  values have the same names in every game.
- The game's `patches/syms.ld` has the natives (`docs/processes.md`), and its `patches/survey.c` a
  `survey_tick`, which the scheduler calls once a frame.

## Changing it

A change is a commit to `main`, pushed as a fast-forward: the games pin its commits. A change reaches a game when the
game's `lib/mario-party-engine` moves onto it, by a pull request there that builds the game's page and plays it, so a
change that every game needs moves all three.

## License

GPL-3.0, as the recompilations it serves.
