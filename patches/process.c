#include "patches.h"
#include "engine_names.h"

// Hudson's process scheduler (the decomp's process.c) on native threads (src/process_threads.cpp says how). On a
// console each process has a stack in its heap, and the scheduler and the processes switch with the game's setjmp and
// longjmp: the scheduler (HuPrcCall) longjmps into each process that is due, and a process setjmps where it sleeps and
// longjmps back. Here the four functions that switch call the natives instead, and everything else is the game's as it
// was. The scheduler's loop is the game's, line for line, with recomp_prc_resume where it longjmped into a process,
// and the heap of a process that ended is freed by the scheduler, on its own stack, as on a console.
//
// A process killed while it waits (HuPrcKill sets EXEC_PROCESS_DEAD) is resumed in PRC_END on a console: the scheduler
// makes PRC_END its jmp_buf's function. A new process starts at its jmp_buf's function, which covers one killed before
// it ran, and one that was running checks for it when it wakes (prc_wait). It starts with the registers its jmp_buf
// holds, as the longjmp into it loaded them: the game reads some before it sets them.
//
// The games' decomps name the scheduler's functions and variables differently, so this file uses the names the game's
// own patches/engine_names.h gives them (README.md lists them).

// The natives (src/process_threads.cpp).
s32 recomp_prc_resume(Process* process, jmp_buf* jump);
void recomp_prc_yield(void);
void recomp_prc_exit(void);
void recomp_prc_reset(process_func end);
// The game's survey (its patches/survey.c).
void survey_tick(void);

// What a process's setjmp-then-longjmp to the scheduler was: back to the scheduler until it resumes this process,
// then on into PRC_END if the process was killed meanwhile.
static void prc_wait(Process* process) {
    recomp_prc_yield();
    if (process->exec_mode == EXEC_PROCESS_DEAD) {
        PRC_END();
    }
}

RECOMP_PATCH void PRC_SYS_INIT(void) {
    // @recomp No process outlives the list it was on.
    recomp_prc_reset(PRC_END);
    PRC_COUNT = 0;
    PRC_TOP = NULL;
}

RECOMP_PATCH void PRC_CHILD_WAIT(void) {
    Process* process = HuPrcCurrentGet();
    if (process->oldest_child) {
        process->exec_mode = EXEC_PROCESS_WATCH;
        // @recomp setjmp(&process->prc_jump), longjmp(&process_jmp_buf, 1)
        prc_wait(process);
    }
}

RECOMP_PATCH void HuPrcTerminate(Process* process) {
    if (process->destructor) {
        process->destructor();
    }

    PRC_UNLINK(&PRC_TOP, process);
    PRC_COUNT--;
    // @recomp longjmp(&process_jmp_buf, 2): the scheduler frees the process's heap.
    recomp_prc_exit();
}

RECOMP_PATCH void HuPrcSleep(s32 time) {
    Process* process = HuPrcCurrentGet();
    if (time != 0 && process->exec_mode != EXEC_PROCESS_DEAD) {
        process->exec_mode = EXEC_PROCESS_SLEEPING;
        process->sleep_time = time;
    }

    // @recomp setjmp(&process->prc_jump), longjmp(&process_jmp_buf, 1)
    prc_wait(process);
}

RECOMP_PATCH void HuPrcCall(s32 time) {
    Process* cur_proc_local;
    s32 ret;

    // @recomp A script's survey (the game's patches/survey.c).
    survey_tick();

    PRC_CURRENT = PRC_TOP;
    // @recomp setjmp(&process_jmp_buf), which returns 0 here and what the process longjmped with after each one.
    ret = 0;
    while (1) {
        switch (ret) {
            case 2:
                PRC_HEAP_FREE(PRC_CURRENT->heap);
                PRC_CURRENT = PRC_CURRENT->next;
                break;
            case 1:
                PRC_CURRENT = PRC_CURRENT->next;
                break;
        }

        cur_proc_local = PRC_CURRENT;

        if (cur_proc_local == 0) {
            break;
        }

#ifdef PRC_SLEEP_LOC
        // Where the process last slept, as a word its setjmp left on its stack, kept for a debugger: nothing in the
        // game reads it. Here the jmp_buf keeps the sp the process started with, so it is a word of the process's heap.
        PRC_SLEEP_LOC = PRC_CURRENT->prc_jump.sp[10];
#endif

        if ((cur_proc_local->stat & 0x1)) {
            if (cur_proc_local->exec_mode != 3) {
                ret = 1;
                continue;
            }
        }

        switch (cur_proc_local->exec_mode) {
            case EXEC_PROCESS_SLEEPING:
                if (cur_proc_local->sleep_time > 0 && (cur_proc_local->sleep_time -= time) <= 0) {
                    cur_proc_local->sleep_time = 0;
                    cur_proc_local->exec_mode = EXEC_PROCESS_DEFAULT;
                }

                ret = 1;
                break;

            case EXEC_PROCESS_WATCH:
                if (cur_proc_local->oldest_child != 0) {
                    ret = 1;
                } else {
                    cur_proc_local->exec_mode = EXEC_PROCESS_DEFAULT;
                    ret = 0;
                }

                break;

            case EXEC_PROCESS_DEAD:
                cur_proc_local->prc_jump.func = PRC_END;

            case 0:
                // @recomp longjmp(&cur_proc_local->prc_jump, 1)
                ret = recomp_prc_resume(cur_proc_local, &cur_proc_local->prc_jump);
                break;
        }
    }
}
