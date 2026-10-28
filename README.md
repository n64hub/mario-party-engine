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
- `patches/camera_view.c`: a camera's view past 4:3, its scissor to the window's edges and a board's pan moved from
  its viewport into its projection, so the 3D widens without stretching and pans with the board at the display's rate.
  A game's patches of the two functions that start a camera's view call it.
- `patches/board_back.c`: a board's background past 4:3, the tiles a game's patch picks drawn as quads at their
  places on the map, kept to the picture's aspect and moved by an interpolated scroll. A map no wider than 4:3 (a
  board's opening scene) keeps black at the window's sides, and the camera's view, the HUD and a screen wipe stay
  inside 4:3 with it.
- `src/hud.cpp`: a board's HUD past 4:3, the four players' panels drawn at the window's left and right edges, through
  hooks in the sprite manager, whose code is the decompilations' asm.
- `src/wipe.cpp`: a screen wipe past 4:3, its colour (an iris's black around the shape, a fade) drawn out to the
  window's left and right edges, through hooks in the wipe module, whose code is the decompilations' asm.
  `src/display_list.h` is what it and `src/hud.cpp` write into a game's display list with.
- `patches/pad.c`: the controllers' setup, with the lock the controllers' reads take made before the reads start,
  not after: a read between the two waited on the lock for good at boot, and the game with it.
- `src/aspect_ratio.cpp`: the aspect ratio the picture is drawn at, the window's with Aspect Ratio Expand, for a
  patch that has to know how far past 4:3 the window shows (Mario Party's board camera, kept where the wider picture
  stays on the map).
- `tools/gen_syms.py`: writes a game's `syms/` (the symbols N64Recomp reads) from its decompilation's ELF; the game's
  `tools/gen_syms.sh` runs it.

Anything else the games share comes here when a change needs it in more than one of them.

## How a game uses it

- `CMakeLists.txt` compiles `lib/mario-party-engine/src/process_threads.cpp`, `src/hud.cpp` and `src/wipe.cpp` into
  the game, with RT64's `include/` on its path, and `src/aspect_ratio.cpp` if its patches call `mp_aspect_ratio`.
- `patches/Makefile` compiles `lib/mario-party-engine/patches/*.c` with the game's own patches, against the game's
  decompilation's headers, its `patches/patches.h` and RT64's `include/` (the extended GBI the camera and background
  draw with).
- The game's `patches/engine_names.h` includes the decompilation's headers the patches need, declares what they leave
  out, and names what the engine's patches use:

  | Name               | What it is                       | Mario Party         | Mario Party 2         | Mario Party 3            |
  |--------------------|----------------------------------|---------------------|-----------------------|--------------------------|
  | `PRC_SYS_INIT`     | the scheduler's init             | `HuPrcInit`         | `HuPrcInit`           | `HuPrcSysInit`           |
  | `PRC_CHILD_WAIT`   | a process waits for its children | `HuPrcChildWatch`   | `HuPrcChildWatch`     | `HuPrcChildWait`         |
  | `PRC_END`          | a process ends itself            | `HuPrcEnd`          | `HuPrcEnd`            | `HuPrcExit`              |
  | `PRC_UNLINK`       | takes a process off the list     | `HuUnlinkPrc`       | `HuUnlinkPrc`         | `UnlinkProcess_0004F7C8` |
  | `PRC_HEAP_FREE`    | frees an ended process's heap    | `HuMemDirectFree`   | `HuMemMemoryFreePerm` | `HuMemMemoryFreePerm`    |
  | `PRC_TOP`          | the list's first process         | `processtop`        | `processtop`          | `top_process`            |
  | `PRC_CURRENT`      | the process running              | `processcur`        | `processcur`          | `current_process`        |
  | `PRC_COUNT`        | how many are on the list         | `processcnt`        | `processcnt`          | `process_count`          |
  | `PRC_SLEEP_LOC`    | where one last slept (optional)  |                     |                       | `sPrcSleepLoc`           |
  | `PAD_INIT`         | the controllers' setup           | `func_800138DC`     | `func_800169AC_175AC` | `InitController`         |
  | `PAD_INIT_SI`      | its request, finding them        | `func_80013770`     | `func_80016840_17440` | `_InitController`        |
  | `PAD_REQUEST`      | hands the SI thread a function   | `RequestSIFunction` | `func_8007EE0C_7FA0C` | `RequestSIFunction`      |
  | `PAD_REQUEST_T`    | the request it takes             | `unkMesg`           | `unkfunc_8007EE0C`    | `unkMesg`                |
  | `PAD_AUTO_READ`    | starts the reads at each retrace | `func_80014174`     | `func_80017244_17E44` | `SetAutoReadController`  |
  | `PAD_AUTO_READING` | whether they are started         | `D_800D12CC`        | `D_800D819C_D8D9C`    | `D_800ABF9C_main`        |
  | `PAD_STICK_LIMIT`  | the sticks' limits               | `func_80013AEC`     | `func_80016BBC_177BC` | `SetStickValueLimit`     |
  | `PAD_LOCK`         | the lock the reads take          | `D_800D12D0`        | `D_800D81A0_D8DA0`    | `D_800ABFA0_main`        |
  | `PAD_LOCK_MESG`    | its message                      | `D_800D12E8`        | `D_800D81B8_D8DB8`    | `D_800ABFB8_main`        |
  | `PAD_CONTROLLERS`  | how many controllers there are   | `D_800F3778`        | `gNumOfControllers`   | `gNumOfControllers`      |

  `HuPrcCall`, `HuPrcSleep`, `HuPrcTerminate`, `HuPrcCurrentGet`, `Process` and its fields, and the `EXEC_PROCESS_*`
  values have the same names in every game.
- The game's `patches/syms.ld` has the natives (`docs/processes.md`, and `src/hud.cpp`'s `recomp_hud_4x3`), and its
  `patches/survey.c` a `survey_tick`, which the scheduler calls once a frame.
- The game's `us.toml` hooks the sprite manager and its board's panels for `src/hud.cpp`, and the wipe module for
  `src/wipe.cpp` (their headers say which functions).

## Changing it

A change is a commit to `main`, pushed as a fast-forward: the games pin its commits. A change reaches a game when the
game's `lib/mario-party-engine` moves onto it, by a pull request there that builds the game's page and plays it, so a
change that every game needs moves all three.

## License

GPL-3.0, as the recompilations it serves.
