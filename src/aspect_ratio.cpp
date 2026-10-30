// The aspect ratio a game's picture is drawn at, for a patch that has to know how far past 4:3 the window shows: the
// window's when Aspect Ratio is Expand, where RT64 widens the 3D to it, and 4:3 when it is Original or the window is
// narrower. Manual is taken as the window's too: RT64 draws no wider than the window, and a patch that allows for more
// than is drawn only keeps a little more inside it.
//
// A board's camera is one: each game keeps the camera where a 320x240 picture stays on the board's map, and the map
// ends there, so a wider picture showed black past it (Mario Party's patches/background.c). So is a scene drawn no
// wider than an aspect past 4:3 (src/hud.cpp's picture_short_half), which stops short of the window's edges only when
// the window is wider than that.

#include <algorithm>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "recompui/recompui.h"
#include "ultramodern/config.hpp"
#include "display_list.h"

float mp_display_list::picture_aspect() {
    constexpr float ORIGINAL = 4.0f / 3.0f;
    int width = 0;
    int height = 0;
    float aspect = ORIGINAL;

    if (ultramodern::renderer::get_graphics_config().ar_option != ultramodern::renderer::AspectRatio::Original) {
        recompui::get_window_size(width, height);
        if ((width > 0) && (height > 0)) {
            aspect = std::max(static_cast<float>(width) / height, ORIGINAL);
        }
    }
    return aspect;
}

// mp_aspect_ratio(): the picture's width over its height, 4:3 at least.
extern "C" void mp_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, mp_display_list::picture_aspect());
}
