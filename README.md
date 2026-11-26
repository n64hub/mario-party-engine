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
  `src/process_threads.h` declares `mp_prc_started()` for a game's own code: the processes that have started, so a
  game knows when a snapshot of RDRAM holds all of its state (for netplay).
- `patches/camera_view.c`: a camera's view past 4:3, its scissor to the window's edges and a board's pan moved from
  its viewport into its projection, so the 3D widens without stretching and pans with the board at the display's rate.
  A game's patches of the two functions that start a camera's view call it. In a scene drawn at 4:3, a camera's
  scissor 8 pixels inside the screen (Mario Party 3's opening), which a TV's overscan hid, is the whole screen's, so
  its 3D reaches the edges its background picture does. A scene a game's patch names has its 3D cut at the bottom of the
  console's overscan border, where its 3D below it shows what a TV's overscan hid (Mario Party's Mini-Game Stadium
  results, whose coin columns go on down under the podiums to the screen's last line).
- `patches/board_back.c`: a board's background past 4:3, the tiles a game's patch picks drawn as quads at their
  places on the map, kept to the picture's aspect and moved by an interpolated scroll. A map no wider than 4:3 (a
  board's opening scene), or a scene a game's patch says the game draws at 4:3 (board_back_scene_widest), keeps black
  at the window's sides, and the camera's view, the HUD and a screen wipe stay inside 4:3 with it. A scene a game's
  patch says is drawn no wider than an aspect past 4:3, a stage whose 3D ends short of a wide window's edges, is drawn
  to the window's edges up to that width, with black only past it. The wipe between turns, which closes the board
  camera's scissor, takes the map away with the 3D, its black out to the window's edges, and the 3D keeps its width
  as it goes.
- `src/hud.cpp`: a board's HUD past 4:3, the four players' panels drawn at the window's left and right edges (with
  their items, those drawn as triangles too, the hand that carries one out and an item flown to another panel), and a
  picture as wide as the screen (a minigame's sky) stretched over the window wherever the game puts it, through hooks
  in the sprite manager, whose code is the decompilations' asm. The letters of the game's big messages (WINS! after a
  minigame), which come in from five times their size, reach out to the window's edges rather than stop at 4:3's.
  A minigame's corner HUD (each player's name and coins in Mario Party's Grab Bag, Crane Game and Bash 'n' Cash) is
  drawn at the window's sides, as far in from its edges as from 4:3's. A bar across the screen (Bobsled Run's progress
  bar between its two views) is stretched over the window from edge to edge, its racers' icons, start and finish going
  across it with it.
  A sprite drawn magnified a few rows at a time (the sky behind a minigame's rules) is filtered across each join of its
  strips, with no hard line along it.
- `src/wipe.cpp`: a screen wipe past 4:3, its colour (an iris's black around the shape, a fade) drawn out to the
  window's left and right edges, through hooks in the wipe module, whose code is the decompilations' asm. A shape
  wipe's shape (a star, a circle) is made as many times larger as the picture is wider than 4:3, so it sweeps past the
  window's corners where the game sized it for 4:3's, and is drawn where the game put it in the frames between the
  game's, as the colour round it is, where RT64's rect interpolation drew it between two sizes. In a 4:3
  picture it draws black over the window's sides at the end of each frame, over whatever the scene drew there, and a
  fade of the whole screen under a scissor of the whole screen (Mario Party 3's opening draws its flash of white under
  a scissor 8 pixels inside it).
  `src/display_list.h` is what it and `src/hud.cpp` write into a game's display list with.
- `src/model_ids.cpp`: a 3D model's parts paired with their own between the game's frames, each part's matrix tagged
  with an ID of its model and its part (gEXMatrixGroup), through hooks in the model walk and the draw of its queue,
  whose code is the decompilations' asm. RT64 otherwise guesses the pairs among draws that look alike, and paired a
  character's head with its neighbour's in the frames between the game's (Mario Party 3's opening, as the toy box
  comes down). A part's ID holds the camera too, as each camera of a screen split between players draws it again, and
  a copy's number, as a model can draw one part at several of its nodes (Mario Party 1's Coin Block Bash, its
  coins, coin bags and field): under one ID, a copy was drawn in the frames between partway to another copy. A copy
  takes the number of the one the frame before it lies nearest, so a block that breaks takes only its own number with
  it. A part that blinks, moved far and back to its place of the frame before that, is drawn at its own matrix, not
  halfway between (a player the hammer hit in Coin Block Bash, which the game hides by moving them near the eye every
  other frame). A camera that cuts to another shot, told by its view's jump (`mp_model_camera`, which a game's patch calls with the
  camera's view and projection as it sets it up), has its parts drawn at the new shot, not swept partway to it (Mario
  Party 2's opening). So is a part turned more than 75 degrees in a frame, which is the game putting it somewhere,
  not a turn, and with it every part at its place, so what the game draws as one thing in two models stays in one
  piece (Mario Party 3's party setup: its spinning star, and the BONUS dial it swaps in for the star's back). Where
  most of a camera's parts turned in a frame, the camera turned, and every part of it is moved partway in the frames
  between, not put at its new place where it moved far after resting, as RT64 otherwise does: the camera's view is in
  each part's matrix, so as a camera turned from rest the far parts jumped and the near ones didn't (Mario Party 1's
  Mini-Game House, its red door out of its archway as the camera turned to the Pot o' Skills or the table).
  A minigame's own effects, drawn from a table of its own outside the model walk (`mp_effect_push`, `mp_effect_pop`),
  are tagged a slot each, and drawn at their own matrix where a slot is new or handed on to another effect: untagged,
  Coin Block Bash's coins, puffs and shadows were paired with each other as they came and went, a coin drawn huge and
  a shadow stretched into a line across the field in the frames between.
- `patches/pad.c`: the controllers' setup, with the lock the controllers' reads take made before the reads start,
  not after: a read between the two waited on the lock for good at boot, and the game with it.
- `src/aspect_ratio.cpp`: the aspect ratio the picture is drawn at, the window's with Aspect Ratio Expand, for a
  patch that has to know how far past 4:3 the window shows (Mario Party's board camera, kept where the wider picture
  stays on the map), and for `src/hud.cpp`, whether a scene drawn no wider than an aspect past 4:3 stops short of the
  window's edges.
- `docs/mods.md`: installing and making a mod for the three games' desktop builds, what each game gives a mod, and
  an example.
- `src/board_options.cpp`: the board's two options on the General tab, Board Movement and Map Scrolling, each
  Original (as on the console, the default) or Fast: their names, values and wording, the same in every game, and what
  a game's hooks ask of them. Fast board movement walks from space to space in half the time, Fast map scrolling moves
  the map's view twice as fast.
- `src/launcher_background.cpp`: a picture behind the launcher's menu, where RecompFrontend draws a plain dark
  colour, covering the window, with the menu's text outlined so it reads on it. Each game passes its own picture.
- `src/crash_trace.cpp`: a crash's trace in a Linux or macOS build. A fault (SIGSEGV, SIGBUS, SIGILL, SIGFPE) or an
  abort prints a `Crash:` line to stderr with the signal, the address and the thread, then the native stack's frames
  (module and offset, for `addr2line` on the same build's binary), and dies of the same signal. It installs itself
  as the program starts; a game only compiles it. Nothing happens on Windows or in a browser.
- `tools/gen_syms.py`: writes a game's `syms/` (the symbols N64Recomp reads) from its decompilation's ELF; the game's
  `tools/gen_syms.sh` runs it.

Anything else the games share comes here when a change needs it in more than one of them.

## How a game uses it

- `CMakeLists.txt` compiles `lib/mario-party-engine/src/process_threads.cpp`, `src/hud.cpp`, `src/wipe.cpp`,
  `src/aspect_ratio.cpp` (which `src/hud.cpp` asks the picture's width), `src/board_options.cpp`, `src/model_ids.cpp`,
  `src/launcher_background.cpp` and `src/crash_trace.cpp` into the game,
  with RT64's `include/` on its path. The game's `src/game/config.cpp` adds the board's options to its General tab
  (`mp_board_options::add`), and its `src/main/main.cpp` puts its launcher's picture, built in with file_to_c, behind
  the menu (`mp_launcher_background::set`).
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
- The game's `patches/syms.ld` has the natives (`docs/processes.md`, and `src/hud.cpp`'s `recomp_hud_widest`), and its
  `patches/survey.c` a `survey_tick`, which the scheduler calls once a frame.
- The game's `us.toml` hooks the sprite manager (with each sprite's width and place on the screen, from its own layout
  of a sprite), its board's panels, its message module's letters, a minigame's corner HUD and Bobsled Run's bar for `src/hud.cpp`, and
  the wipe module and the end of a frame's last display list for `src/wipe.cpp` (their headers say which functions),
  and where a walk and the map's view take their pace, for `src/board_options.cpp`, and the model walk, its queue and
  the queue's draw for `src/model_ids.cpp` (its header says where), and a minigame's table of effects where it
  draws one (Mario Party 1's Coin Block Bash).

## Changing it

A change is a commit to `main`, pushed as a fast-forward: the games pin its commits. A change reaches a game when the
game's `lib/mario-party-engine` moves onto it, by a pull request there that builds the game's page and plays it, so a
change that every game needs moves all three.

This repo's CI builds the three games with `lib/mario-party-engine` at the commit under test, by calling each game's
own CI on that game's `main` (Windows and Linux, everything but the code generated from the ROM, no link). A change
that needs a game's change too stays red here until the game's change is on its `main`.

## License

GPL-3.0, as the recompilations it serves.
