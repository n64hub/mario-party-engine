// The board's options on the General tab, Board Movement and Map Scrolling, the same in every game: their names, values,
// wording and place are here, so they can't drift apart. Original, the default, plays as the console does. Each game's
// us.toml hooks its own code, where a walk and the map's view take their pace, and asks these two whether to change it.
//
// Fast board movement takes a walk from one space to the next in half the frames: Mario Party walks a player at twice
// the speed (38 a frame for 19), Mario Party 2 takes 5 frames a space for 10, and Mario Party 3 reads its own walking
// speed as Fast (5 frames, its Normal's 10), whatever its pause menu holds, which still shows and saves the player's
// own. Fast map scrolling doubles the speed the board's camera follows the stick at in the map's view (8 for 4 in
// Mario Party and on Mario Party 3's boards, 7 for 3.5 in Mario Party 2 and on Mario Party 3's duel boards). The
// players' community cheat codes for the games do the same by rewriting the game's instructions, which a recompilation
// has already turned into C.

#include <string>
#include <vector>

#include "librecomp/config.hpp"
#include "recompui/config.h"
#include "board_options.h"

namespace {
    enum class Pace {
        Original,
        Fast,
        OptionCount
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> pace_options = {
        {Pace::Original, "Original", "Original"},
        {Pace::Fast, "Fast", "Fast"},
    };
    const std::string board_movement_option = "board_movement";
    const std::string map_scrolling_option = "map_scrolling";

    bool fast(const std::string& option) {
        auto value = recompui::config::get_general_config().get_option_value(option);
        return static_cast<Pace>(std::get<uint32_t>(value)) == Pace::Fast;
    }
}

void mp_board_options::add(recomp::config::Config& general) {
    general.add_enum_option(
        board_movement_option,
        "Board Movement",
        "How fast the players walk from space to space on a board."
        "<br/>"
        "<b>Fast:</b> in half the time, as Mario Party 3's own Fast walking speed does, whatever the walking speed in "
        "a game's pause menu says. "
        "<b>Original:</b> as on the console.",
        pace_options,
        Pace::Original
    );
    general.add_enum_option(
        map_scrolling_option,
        "Map Scrolling",
        "How fast the view moves when you look around a board's map."
        "<br/>"
        "<b>Fast:</b> twice as fast. "
        "<b>Original:</b> as on the console.",
        pace_options,
        Pace::Original
    );
}

extern "C" int mp_board_movement_fast(void) {
    return fast(board_movement_option);
}

extern "C" int mp_map_scrolling_fast(void) {
    return fast(map_scrolling_option);
}
