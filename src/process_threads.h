// What the process scheduler (process_threads.cpp) offers a game's own code.

#pragma once

#include <cstdint>
#include <vector>

// The keys of the processes that have started (have a thread), lowest first, for a snapshot of the game to know which
// processes' state is all in RDRAM: one that hasn't started is only its jmp_buf and stack there, while one that has
// keeps its locals on its thread's native stack. Read-only, and nothing in the engine calls it.
std::vector<uint32_t> mp_prc_started();
