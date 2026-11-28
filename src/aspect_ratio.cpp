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
#include <cmath>
#include <cstring>

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

// A camera's cull past 4:3. The engine draws a stage's grid cell only where one of its corners lies in a triangle on
// the ground, seen from above: its apex a little behind the camera, its sides at the camera's yaw plus and minus half
// its field of view, the vertical one, which is how far a 4:3 picture reaches each side once the apex is moved back. A
// wider picture showed black where a cell past 4:3 wasn't drawn (Mario Party 3's Cheep Cheep Chase, its walls, floor and
// water), and a board's coins, gates and characters came and went at 4:3's edges. mp_cull_widen() widens the field of
// view the draw reads by as much as the picture's half-width widens, from atan(tan(fov/2) * 4/3) to atan(tan(fov/2) *
// aspect) each side, so the triangle reaches as far past the window's edges as it did past 4:3's; mp_cull_restore()
// puts it back once the draw is done, as the game writes it once and reads it for other things too. A game's hooks call
// them at the start and end of its draw of a camera's cells, with the address of the field of view the draw reads.
namespace {
    uint32_t cull_fov_address = 0;
    uint32_t cull_fov_word = 0;
    bool cull_fov_saved = false;

    float word_float(uint32_t word) {
        float value;
        std::memcpy(&value, &word, sizeof(value));
        return value;
    }

    uint32_t float_word(float value) {
        uint32_t word;
        std::memcpy(&word, &value, sizeof(word));
        return word;
    }
}

// mp_cull_widen(fov): the field of view of a camera's cull at fov (degrees, a float in RDRAM) widened to the picture's.
extern "C" void mp_cull_widen(uint8_t* rdram, gpr fov_address) {
    constexpr float ORIGINAL = 4.0f / 3.0f;
    constexpr float DEGREES = 180.0f / 3.14159265f;
    constexpr float WIDEST = 170.0f;
    float aspect = mp_display_list::picture_aspect();
    uint32_t word = MEM_W(0, fov_address);
    float fov = word_float(word);

    cull_fov_address = (uint32_t)fov_address;
    cull_fov_word = word;
    cull_fov_saved = true;
    if ((aspect <= ORIGINAL) || !(fov > 0.0f) || (fov >= WIDEST)) {
        return;
    }
    float half_tan = std::tan(fov / 2.0f / DEGREES);
    float widened = 2.0f * (std::atan(half_tan * aspect) - std::atan(half_tan * ORIGINAL)) * DEGREES;
    MEM_W(0, fov_address) = float_word(std::min(fov + widened, WIDEST));
}

// mp_cull_restore(): the field of view mp_cull_widen() widened, as the game wrote it.
extern "C" void mp_cull_restore(uint8_t* rdram) {
    if (cull_fov_saved) {
        MEM_W(0, (gpr)(int32_t)cull_fov_address) = cull_fov_word;
        cull_fov_saved = false;
    }
}
