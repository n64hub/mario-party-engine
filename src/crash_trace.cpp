// A crash's trace on a Linux or macOS build: a fault (SIGSEGV, SIGBUS, SIGILL, SIGFPE) or an abort prints, to stderr,
// a "Crash:" line naming the signal, the address it faulted at and the thread it hit, then the native stack's return
// addresses, each with the module it is in and its offset there, and then dies of the same signal as it would have.
//
// Without it a fault ends the game with no word at all: a Linux build of Mario Party 3 died of SIGSEGV on Chilly
// Waters once in nine runs with nothing in stderr or stdout, and nothing said where. The addresses are the program's
// own, so a trace is read with the same build's binary, which a Release build keeps its symbols in (it isn't stripped):
//   addr2line -f -C -e MarioParty3Recompiled 0x<offset>
// for each frame's offset, or `nm -C --defined-only` sorted for a frame in a function addr2line has no line for.
// tools/frame_dump.mjs (the dumps' and soaks' runner) fails a run at once on a "Crash:" line in stderr and keeps the log.
//
// It installs itself as the program starts (a static object's constructor), for each of those signals no one handles
// yet, so a game only compiles this file. Each thread that faults runs the handler on its own stack, the one that
// started the program on a stack of its own (sigaltstack) so that an overflow of that one stack is traced too. The
// handler writes with write(2) alone and formats its numbers itself; backtrace() is called once at start, so that the
// handler's call doesn't load libgcc. Nothing happens in a browser or on Windows.

#if (defined(__linux__) || defined(__APPLE__)) && !defined(__EMSCRIPTEN__)

#include <csignal>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

namespace {

constexpr int signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };

void put(const char* s) {
    size_t n = strlen(s);
    while (n > 0) {
        ssize_t w = write(STDERR_FILENO, s, n);
        if (w <= 0) {
            return;
        }
        s += w;
        n -= static_cast<size_t>(w);
    }
}

void put_hex(uintptr_t v) {
    char buf[2 + 2 * sizeof(v) + 1];
    char* p = buf + sizeof(buf) - 1;
    *p = '\0';
    do {
        *--p = "0123456789abcdef"[v & 0xF];
        v >>= 4;
    } while (v != 0);
    *--p = 'x';
    *--p = '0';
    put(p);
}

void put_dec(long v) {
    char buf[24];
    char* p = buf + sizeof(buf) - 1;
    *p = '\0';
    bool neg = v < 0;
    unsigned long u = neg ? 0UL - static_cast<unsigned long>(v) : static_cast<unsigned long>(v);
    do {
        *--p = static_cast<char>('0' + u % 10);
        u /= 10;
    } while (u != 0);
    if (neg) {
        *--p = '-';
    }
    put(p);
}

const char* signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS: return "SIGBUS";
        case SIGILL: return "SIGILL";
        case SIGFPE: return "SIGFPE";
        case SIGABRT: return "SIGABRT";
        default: return "signal";
    }
}

volatile sig_atomic_t in_handler = 0;

void on_crash(int sig, siginfo_t* info, void*) {
    // The default action, for what follows: the signal again as this one returns (a fault re-runs the instruction
    // that faulted) or as it raises it (an abort, or a signal sent).
    signal(sig, SIG_DFL);
    if (in_handler) {
        return; // a fault in the trace itself: die of it
    }
    in_handler = 1;

    put("Crash: ");
    put(signal_name(sig));
    put(" (signal ");
    put_dec(sig);
    put(", code ");
    put_dec(info ? info->si_code : 0);
    put(")");
    if (sig != SIGABRT && info) {
        put(" at address ");
        put_hex(reinterpret_cast<uintptr_t>(info->si_addr));
    }
#if defined(__linux__)
    char name[17] = {};
    prctl(PR_GET_NAME, name, 0, 0, 0);
    put(" in thread \"");
    put(name);
    put("\" (tid ");
    put_dec(static_cast<long>(syscall(SYS_gettid)));
    put(")");
#endif
    put("\n");

    void* frames[64];
    int count = backtrace(frames, 64);
    put("Crash: native stack, ");
    put_dec(count);
    put(" frames (module(+offset) [address], the innermost first; addr2line -f -C -e <module> <offset>):\n");
    backtrace_symbols_fd(frames, count, STDERR_FILENO);

    // The modules the frames are in, and where each is loaded, for a frame backtrace_symbols names no module for.
    // dladdr only reads the loader's tables.
    for (int i = 0; i < count; i++) {
        Dl_info dl;
        if (dladdr(frames[i], &dl) == 0 || dl.dli_fname == nullptr) {
            continue;
        }
        bool seen = false;
        for (int j = 0; j < i && !seen; j++) {
            Dl_info prev;
            seen = dladdr(frames[j], &prev) != 0 && prev.dli_fbase == dl.dli_fbase;
        }
        if (seen) {
            continue;
        }
        put("Crash: module ");
        put(dl.dli_fname);
        put(" at ");
        put_hex(reinterpret_cast<uintptr_t>(dl.dli_fbase));
        put("\n");
    }

    if (info == nullptr || info->si_code <= 0 || sig == SIGABRT) {
        raise(sig); // sent, not a fault: returning would go on as if nothing happened
    }
}

struct CrashTrace {
    CrashTrace() {
        void* warm[1];
        backtrace(warm, 1);

        static char alt_stack[64 * 1024];
        stack_t ss{};
        ss.ss_sp = alt_stack;
        ss.ss_size = sizeof(alt_stack);
        ss.ss_flags = 0;
        sigaltstack(&ss, nullptr);

        for (int sig : signals) {
            struct sigaction old{};
            if (sigaction(sig, nullptr, &old) != 0 || (old.sa_flags & SA_SIGINFO) || old.sa_handler != SIG_DFL) {
                continue; // someone handles it already (a debugger's or a sanitizer's own)
            }
            struct sigaction sa{};
            sa.sa_sigaction = on_crash;
            sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
            sigemptyset(&sa.sa_mask);
            sigaction(sig, &sa, nullptr);
        }
    }
};

CrashTrace crash_trace;

} // namespace

#endif
