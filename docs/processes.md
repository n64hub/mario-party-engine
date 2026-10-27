# The process scheduler

Hudson's engine, which Mario Party, Mario Party 2 and 3 and Bomberman 64 are built on, runs most of a game as
processes: small functions, each with a stack of its own in the engine's heap, which a scheduler runs in turn once a
frame. A process runs until it sleeps (`HuPrcSleep`), waits for its children (Mario Party's `HuPrcChildWatch`) or ends
(Mario Party's `HuPrcEnd`), and the scheduler (`HuPrcCall`, called from the game's main thread) then goes on to the
next. On a console they switch with the game's own setjmp and longjmp: `HuPrcCall` longjmps into a process, onto its
stack, and the process setjmps where it stops and longjmps back.

Recompiled code can't do that. A function of the game is a C function, so a process's state is on the native stack as
well as in RDRAM, and a longjmp between two of them would leave the native stack behind.

The names here are Mario Party's; the README has each game's.

## How it runs here

Each live process runs on a native thread of its own, and exactly one of them runs at a time, as the console runs one
process at a time:

- `HuPrcCall` is the game's loop, line for line, with `recomp_prc_resume` where it longjmped into a process. The first
  resume of a process starts a thread at its function with its RDRAM stack and the saved registers its jmp_buf holds
  (s0-s7, fp and f20-f31), as the longjmp loaded them; a later one wakes its thread. Either way the scheduler waits
  until the process hands the baton back, then goes on with what the longjmp back would have returned: 1 when the
  process slept, 2 when it ended.
- `HuPrcSleep` and `HuPrcChildWatch` hand the baton back (`recomp_prc_yield`) and wait to be resumed, where they
  setjmped and longjmped.
- `HuPrcTerminate` ends the process (`recomp_prc_exit`), which throws out of the recompiled code to its thread's loop,
  as N64ModernRuntime ends a game thread. The scheduler frees the process's heap, on its own stack, as on a console.
- `HuPrcInit` ends every process's thread (`recomp_prc_reset`), since none outlives the list it was on.
- A process killed while it waits (`HuPrcKill`) is resumed in `HuPrcEnd` on a console, and here too: a process that
  wakes checks for it, and one that never ran starts there.

The engine never setjmps a new process's jmp_buf, so its registers are whatever the process's heap held, and the game
reads some of them before it sets them: Mario Party's warp pipe room, where Toad sends the players off on an
adventure, reads s3 in its first message's wait, and with 0 there it closes and reopens Toad's window over and over.
Started with the jmp_buf's registers, it waits for A as on a console.

The thread a process runs on takes the identity of the game thread that resumed it (N64ModernRuntime's
`thread_self` and the FPU's rounding mode), so what a process does, a blocking `osRecvMesg` included, is that game
thread's doing, as on a console. A process's thread is kept when the process ends and handed the next new one, so a
board's processes cost no thread creation after the first time.

It is the same code on the desktop and on the web, where a page builds with threads and `-fwasm-exceptions` anyway.
Bomberman 64's recompilation and another of Mario Party 3 make each process a coroutine instead (minicoro), which in a
browser needs Asyncify over the whole recompiled game: a bigger, slower page.

The code is `patches/process.c` (the five patches, on the game's names) and `src/process_threads.cpp` (the natives,
with a header that says how they hand the baton over).

## What a game needs

- `src/process_threads.cpp` as it is. Nothing in it is one game's: its natives take a process's key (its address) and
  its jmp_buf, in the engine's setjmp's layout (sp, the function, s0-s7, fp and f20-f31, a word each), and the game's
  `PRC_END` for a process whose function returns.
- `patches/process.c`, on the names its `patches/engine_names.h` gives (README). Each patch is the game's function
  with its setjmp and longjmp swapped for a native. A game whose `Process` names its fields differently would need
  more names there.
- The natives in its `patches/syms.ld`, each at a dummy address that N64Recomp turns into a call of the C function of
  that name: `recomp_prc_resume`, `recomp_prc_yield`, `recomp_prc_exit` and `recomp_prc_reset`.
- The game calls setjmp and longjmp nowhere but in the four functions the patches replace, so its
  `src/game/libultra.cpp`'s setjmp aborts.

Then build the game with `MP_PROCESS_STATS` (below) and look at what it prints. Each game's own `docs/processes.md`
has what its boards keep alive and what a resume costs.

Not the scheduler's, but the same engine's: its swap chain draws each frame in several RSP tasks, the ground first and
the text last, so each game presents at the VI (its `src/main/main.cpp`). RT64's early present shows a framebuffer as
soon as any task that draws into it ends, and the screen flickers between parts of a frame.

## What it costs

Built with `-DMP_PROCESS_STATS=ON` (each game's CMake option), the program prints every 5 seconds how many processes
are alive, how many threads it has made, how many resumes a second the scheduler makes and what each costs beyond the
process's own run (the two handoffs, from the scheduler's thread to the process's and back). On a desktop a resume
costs about 10 us beyond the process, in Chrome about 25 us, under half a millisecond of each of a game's frames.
