// The board's options, Board Movement and Map Scrolling (board_options.cpp says what each does).

#pragma once

#include "librecomp/config.hpp"

namespace mp_board_options {
    // Adds both to a game's General tab, after its own options.
    void add(recomp::config::Config& general);
}

// What a game's us.toml hooks ask, on the game's thread: 1 when the option is Fast.
extern "C" int mp_board_movement_fast(void);
extern "C" int mp_map_scrolling_fast(void);
