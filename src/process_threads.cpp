// Hudson's process scheduler (patches/process.c) on native threads, one per live process, run one at a time: the
// same on the desktop and the web.
//
// The game gives each process its own stack in RDRAM and switches between them with its own setjmp/longjmp, which
// recompiled code can't do: a process's state is on the native stack too. The desktop recomps of Hudson's games
// (Bomberman 64's, Mario Party 3's) make each process a coroutine (minicoro), which in a browser needs Asyncify over
// the whole recompiled game. Here each process instead runs on a thread of its own, and exactly one thread runs at a time:
// the scheduler hands a process the baton (resume) and waits until it hands it back (yield, or exit when it ends), as
// ultramodern runs the game's threads one at a time. The thread a process runs on takes the identity of the game thread
// that resumed it (ultramodern's thread_self, the FPU's rounding mode), so whatever the process does, a blocking
// osRecvMesg included, is that game thread's doing. Nothing here is the game's: the patches pass a process's key (its
// address) and its jmp_buf, in the engine's setjmp's layout, so Mario Party, Mario Party 2 and Mario Party 3 build this
// file as it is.
//
// A new process starts as the scheduler's longjmp into it started it on a console: at its jmp_buf's function, on its
// stack, with s0-s7, fp and f20-f31 as the jmp_buf holds them. The engine never setjmps a new process's jmp_buf, so
// those are whatever its heap held, and the game reads some before it sets them (Mario Party's warp-pipe room reads s3
// in its first message's wait and, with 0 there, closes and reopens Toad's window over and over).
//
// A thread is kept when its process ends and handed the next new process, so a board's dozens of processes cost no
// thread creation after the first time. A process ends by throwing (prc_exit) out of the recompiled code to its
// thread's loop, as ultramodern ends a game thread (the page builds with -fwasm-exceptions).
//
// The natives (patches/process.c says when each is called):
//   recomp_prc_resume(key, jmp_buf)  -> 1 when the process slept, 2 when it ended
//   recomp_prc_yield()                   from a process: back to the scheduler until it is resumed
//   recomp_prc_exit()                    from a process: it has ended; never returns
//   recomp_prc_reset()                   ends every process's thread (the scheduler's init)
//
// Built with MP_PROCESS_STATS (CMake option of that name), it prints every 5 seconds how many processes are alive, how
// many resumes a second the scheduler makes, and what a resume costs beyond the process's own run: two handoffs.

#include <chrono>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "librecomp/overlays.hpp"
#include "ultramodern/ultramodern.hpp"
#include "lightweightsemaphore.h"

// ultramodern's (threads.cpp): which game thread this native thread is.
extern thread_local PTR(OSThread) thread_self;
extern thread_local bool is_game_thread;

namespace {
    constexpr int32_t reason_slept = 1;
    constexpr int32_t reason_ended = 2;

    // The engine's jmp_buf (its setjmp and longjmp): sp, the function, then s0-s7, fp and f20-f31, a word each.
    constexpr int32_t jmp_sp = 0x00;
    constexpr int32_t jmp_func = 0x04;
    constexpr int32_t jmp_regs_at = 0x08;
    constexpr int jmp_regs = 21;

    // Thrown on a process's thread to unwind it when the process ends.
    struct prc_exit {};

#ifdef MP_PROCESS_STATS
    using stats_clock = std::chrono::steady_clock;
#endif

    struct Worker {
        std::thread thread;
        // The scheduler signals wake to run the process, the process signals back when it gives the baton back.
        moodycamel::LightweightSemaphore wake;
        moodycamel::LightweightSemaphore back;
        uint8_t* rdram = nullptr;
        uint32_t key = 0;
        uint32_t entry = 0;
        uint32_t sp = 0;
        // s0-s7, fp and f20-f31 as a new process's jmp_buf holds them.
        uint32_t regs[jmp_regs] = {};
        // The resuming game thread's identity, taken each time the process is resumed.
        PTR(OSThread) owner = NULLPTR;
        uint32_t rounding = 0;
        int32_t reason = 0;
        bool killed = false;
#ifdef MP_PROCESS_STATS
        stats_clock::time_point woken;
#endif
    };

#ifdef MP_PROCESS_STATS
    struct Stats {
        stats_clock::time_point since = stats_clock::now();
        uint64_t resumes = 0;
        stats_clock::duration resuming{};
        stats_clock::duration running{};
        size_t most_alive = 0;
        size_t threads = 0;
    } stats;

    void print_stats(size_t alive) {
        stats.most_alive = std::max(stats.most_alive, alive);
        auto now = stats_clock::now();
        double seconds = std::chrono::duration<double>(now - stats.since).count();
        if (seconds < 5.0) {
            return;
        }
        double overhead_us = std::chrono::duration<double, std::micro>(stats.resuming - stats.running).count();
        printf("[processes] %zu alive (at most %zu), %zu threads, %.0f resumes/s, %.1f us each beyond the process, "
               "%.2f ms/s in all\n", alive, stats.most_alive, stats.threads, stats.resumes / seconds,
               stats.resumes ? overhead_us / stats.resumes : 0.0, overhead_us / 1000.0 / seconds);
        fflush(stdout);
        size_t threads = stats.threads;
        stats = Stats{};
        stats.threads = threads;
    }
#endif

    std::mutex workers_mutex;
    std::vector<Worker*> idle;
    std::unordered_map<uint32_t, Worker*> running;
    // The process this thread runs, null on any other thread.
    thread_local Worker* current = nullptr;
    uint32_t end_vram = 0;

    // The FPU's rounding mode, as the game set it with ctc1 (recomp.h's: the host's FPU natively, a thread_local with
    // Emscripten, which WebAssembly's fesetround can't change).
    uint32_t get_rounding() {
        return get_cop1_cs();
    }

    void set_rounding(uint32_t mode) {
        set_cop1_cs(mode);
    }

    // Takes the identity of the game thread that resumed this process.
    void take_identity(Worker* w) {
        thread_self = w->owner;
        is_game_thread = true;
        set_rounding(w->rounding);
    }

    // Gives the baton back to the scheduler and waits for it again; throws prc_exit if the process was ended
    // meanwhile (recomp_prc_reset).
    void hand_back(Worker* w, int32_t reason) {
#ifdef MP_PROCESS_STATS
        stats.running += stats_clock::now() - w->woken;
#endif
        w->reason = reason;
        w->back.signal();
        w->wake.wait();
        if (w->killed) {
            throw prc_exit{};
        }
#ifdef MP_PROCESS_STATS
        w->woken = stats_clock::now();
#endif
        take_identity(w);
    }

    void run_process(Worker* w) {
        recomp_context ctx{};
        ctx.r29 = static_cast<int32_t>(w->sp);
        gpr* saved[] = { &ctx.r16, &ctx.r17, &ctx.r18, &ctx.r19, &ctx.r20, &ctx.r21, &ctx.r22, &ctx.r23, &ctx.r30 };
        for (int i = 0; i < 9; i++) {
            *saved[i] = static_cast<int32_t>(w->regs[i]);
        }
        // f20-f31 as singles: with the 32-bit FPU's registers an odd one is the high half of the even one before it.
        fpr* pairs[] = { &ctx.f20, &ctx.f22, &ctx.f24, &ctx.f26, &ctx.f28, &ctx.f30 };
        for (int i = 0; i < 6; i++) {
            pairs[i]->u32l = w->regs[9 + 2 * i];
            pairs[i]->u32h = w->regs[10 + 2 * i];
        }
        ctx.mips3_float_mode = 0;
        ctx.f_odd = &ctx.f0.u32h;
        get_function(static_cast<int32_t>(w->entry))(w->rdram, &ctx);
        // A process that returns ends as HuPrcEnd ends it, which a process that returns on a console would not reach
        // (its return address is whatever the jmp_buf held): kill its children, unlink it, run its destructor.
        if (end_vram != 0) {
            get_function(static_cast<int32_t>(end_vram))(w->rdram, &ctx);
        }
        throw prc_exit{};
    }

    void worker_loop(Worker* w) {
        current = w;
        ultramodern::set_native_thread_name("Process");
        ultramodern::set_native_thread_priority(ultramodern::ThreadPriority::High);
        while (true) {
            w->wake.wait();
            if (!w->killed) {
#ifdef MP_PROCESS_STATS
                w->woken = stats_clock::now();
#endif
                take_identity(w);
                try {
                    run_process(w);
                } catch (prc_exit&) {
                }
            }
            {
                std::lock_guard lock{ workers_mutex };
                running.erase(w->key);
                w->killed = false;
                idle.push_back(w);
            }
#ifdef MP_PROCESS_STATS
            stats.running += stats_clock::now() - w->woken;
#endif
            w->reason = reason_ended;
            w->back.signal();
        }
    }

    Worker* take_worker() {
        std::lock_guard lock{ workers_mutex };
        if (!idle.empty()) {
            Worker* w = idle.back();
            idle.pop_back();
            return w;
        }
        Worker* w = new Worker{};
#ifdef MP_PROCESS_STATS
        stats.threads++;
#endif
        w->thread = std::thread{ worker_loop, w };
        w->thread.detach();
        return w;
    }

    // Hands the baton to w and waits until it comes back.
    int32_t run_until_back(Worker* w) {
        w->owner = thread_self;
        w->rounding = get_rounding();
        w->wake.signal();
        w->back.wait();
        return w->reason;
    }
}

extern "C" void recomp_prc_resume(uint8_t* rdram, recomp_context* ctx) {
    uint32_t key = _arg<0, uint32_t>(rdram, ctx);
    int32_t jump = _arg<1, int32_t>(rdram, ctx);

    Worker* w;
    {
        std::lock_guard lock{ workers_mutex };
        auto it = running.find(key);
        w = it == running.end() ? nullptr : it->second;
    }
    if (w == nullptr) {
#ifdef MP_PROCESS_STATS
        if (getenv("MP_PROCESS_LOG") != nullptr) {
            fprintf(stderr, "[processes] start %08X at %08X\n", key, (uint32_t)MEM_W(jmp_func, jump));
        }
#endif
        w = take_worker();
        w->rdram = rdram;
        w->key = key;
        w->entry = MEM_W(jmp_func, jump);
        w->sp = MEM_W(jmp_sp, jump);
        for (int i = 0; i < jmp_regs; i++) {
            w->regs[i] = MEM_W(jmp_regs_at + 4 * i, jump);
        }
        std::lock_guard lock{ workers_mutex };
        running[key] = w;
    }
#ifdef MP_PROCESS_STATS
    auto start = stats_clock::now();
    int32_t reason = run_until_back(w);
    stats.resuming += stats_clock::now() - start;
    stats.resumes++;
    print_stats(running.size());
    _return(ctx, reason);
#else
    _return(ctx, run_until_back(w));
#endif
}

extern "C" void recomp_prc_yield(uint8_t* rdram, recomp_context* ctx) {
    if (current == nullptr) {
        fprintf(stderr, "recomp_prc_yield: not on a process's thread\n");
        std::abort();
    }
    hand_back(current, reason_slept);
}

extern "C" void recomp_prc_exit(uint8_t* rdram, recomp_context* ctx) {
    if (current == nullptr) {
        fprintf(stderr, "recomp_prc_exit: not on a process's thread\n");
        std::abort();
    }
    throw prc_exit{};
}

// recomp_prc_reset(end): ends every process's thread without running any more of it, and keeps end, the engine's
// HuPrcEnd, for a process whose function returns.
extern "C" void recomp_prc_reset(uint8_t* rdram, recomp_context* ctx) {
    end_vram = _arg<0, uint32_t>(rdram, ctx);
    std::vector<Worker*> left;
    {
        std::lock_guard lock{ workers_mutex };
        for (auto& [key, w] : running) {
            left.push_back(w);
        }
    }
    for (Worker* w : left) {
        w->killed = true;
        run_until_back(w);
    }
}
