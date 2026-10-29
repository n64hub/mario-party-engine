// A board's HUD at the picture's sides: in a picture wider than 4:3, the four players' panels (each a sprite group of
// the sprite manager) drawn at the window's left and right edges, as far in from them as they are from 4:3's on a
// console, the way the PC ports of N64 games that widen the picture draw a HUD. A picture at least as wide as the
// screen (a minigame's sky behind its 3D) is drawn stretched over the whole window wherever the game puts it.
// Everything else the sprite manager draws (text windows, menus, a minigame's sprites) stays in the 4:3 middle, where
// RT64 draws a rectangle by default.
//
// A game's hooks (its us.toml) call these natives, since the sprite manager's code is the decompilations' asm:
// - mp_hud_panel_at where the board sets a player's panel's place (MBStatusPosSet and its kin), with the panel's group
//   and where the board keeps the place it moves the panel by (two floats, x and y, in its status of the player): the
//   panel is drawn at the side its place is on, and a panel in the middle third stays in the middle. The board then
//   slides a panel by that place without the call: the place it set is the panel's corner, where the panel is drawn
//   from the window's edge, as it is further out (sliding in from off the screen). Moved in or down from there (a
//   minigame's teams in rows round the VS badge, a little in from the corners and lower), it eases into the 4:3 middle
//   over its first EASE pixels, the alignment a fraction of the window between its edge and its middle, so it slides as
//   it does in 4:3. Its corner is its place the next time it is drawn, or for a place off the screen, where it first
//   stops on the screen.
// - mp_hud_group_killed where the sprite manager kills a group (HuSprGrpKill), and mp_hud_reset where it starts again
//   with no groups (its init, which forgets them without killing them), so a group the board's panels used is
//   forgotten when the board frees them, before the memory goes to another group.
// - mp_hud_sprite_start and mp_hud_sprite_end around the function that draws one sprite into the display list: for a
//   panel's sprite, RT64's rectangle alignment to the window's edge (gEXSetRectAlign) and every scissor until the end
//   aligned to the window's edges (gEXSetScissorAlign, then a scissor of the whole screen): the sprite pass's and a
//   sprite's own scissors stop at 4:3's edges, which would cut off a panel drawn out past them. After the sprite,
//   both go back to RT64's own and the scissor to the whole screen, the sprite pass's.
// - mp_hud_sprite_width, from a game's hook just before mp_hud_sprite_start, how wide the sprite is on the screen (its
//   frame's width by its scale; a game whose hook doesn't say has every sprite drawn as RT64 draws it). One at least as
//   wide as the screen is a picture behind the scene, and its rectangles are drawn stretched over the window
//   (gEXSetRectAspect G_EX_ASPECT_STRETCH) until the sprite's end. RT64 stretches a rectangle by itself only while it
//   covers the whole width of the screen's scissor, so such a picture moved a few pixels (Mario Party 2's Abandon
//   Ship's sky following the camera in its finish shot, from 0 to -6) dropped into the 4:3 middle, with the frame's
//   clear beside it, while the scene's 3D went on to the window's edges. In a 4:3 picture it stays as RT64 draws it.
// - recomp_hud_4x3, from the engine's patches/board_back.c, whether the picture is 4:3: a board's opening scene's map
//   is no wider than 4:3, with black at the window's sides, and the panels stay where the game puts them, inside 4:3
//   with the scene, rather than out over the black. src/wipe.cpp asks it too (picture_is_4x3).
//
// RT64 reads extended commands only after a gEXEnable in the same display list, so each start sends one.

#include <cmath>
#include <cstring>

#include "display_list.h"

using namespace mp_display_list;

namespace {
    constexpr int PANELS = 4;
    // A panel is as wide as about a third of the screen: its place (its left edge) plus half that is its middle.
    constexpr int32_t PANEL_HALF = SCREEN_W / 6;
    // A sprite group's count of members, a u16, in every game.
    constexpr int GROUP_COUNT = 0x0A;
    // How far in from its corner the game moves a panel before it is drawn wholly in the 4:3 middle: as far as a 2 VS 2
    // game moves the lower panels.
    constexpr float EASE = 16.0f;

    enum class Corner {
        // Its place the next time it is drawn.
        Next,
        // Where it first stops on the screen.
        Stop,
        Known,
    };

    struct Panel {
        gpr group;
        int32_t members;
        // Where the board keeps its place, x and y.
        gpr place;
        // The side its place is on: -1 left, 1 right, 0 the middle third.
        int side;
        Corner corner;
        float corner_x, corner_y;
        // Its place when its first sprite was last drawn, once a frame.
        float last_x, last_y;
    };

    Panel panels[PANELS];
    // Whether the sprite being drawn started an alignment.
    bool sprite_aligned = false;
    // How wide the next sprite is on the screen, from mp_hud_sprite_width, and whether the one being drawn is stretched.
    float sprite_width = 0.0f;
    bool sprite_stretched = false;
    // Whether the picture is 4:3.
    bool picture_4x3 = false;

    float mem_f32(uint8_t* rdram, gpr address) {
        uint32_t bits = (uint32_t)MEM_W(0, address);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    // How far panel is from the window's edge toward the 4:3 middle, 0 to 1, by where the game has moved it from its
    // corner, and its corner found as Corner says.
    float eased_in(uint8_t* rdram, Panel& panel, gpr sprite) {
        gpr first = (gpr)(int32_t)MEM_W(panel.members, panel.group);
        float x = mem_f32(rdram, panel.place);
        float y = mem_f32(rdram, panel.place + 4);
        bool stopped = (sprite == first) && (x == panel.last_x) && (y == panel.last_y);
        if ((panel.corner == Corner::Next) || ((panel.corner == Corner::Stop) && stopped && (x >= 0) && (x <= SCREEN_W))) {
            panel.corner = Corner::Known;
            panel.corner_x = x;
            panel.corner_y = y;
        }
        if (sprite == first) {
            panel.last_x = x;
            panel.last_y = y;
        }
        if (panel.corner != Corner::Known) {
            return 0.0f;
        }
        float in = std::fmax((panel.corner_x - x) * panel.side, 0.0f);
        float down = y - panel.corner_y;
        return std::fmin(std::sqrt(in * in + down * down) / EASE, 1.0f);
    }

    // Which panel's group has this sprite as a member, -1 for none.
    int panel_of(uint8_t* rdram, gpr sprite) {
        for (int i = 0; i < PANELS; i++) {
            const Panel& panel = panels[i];
            if (panel.group == 0) {
                continue;
            }
            uint16_t count = MEM_HU(GROUP_COUNT, panel.group);
            for (uint16_t m = 0; m < count; m++) {
                if ((gpr)(int32_t)MEM_W(panel.members + m * 4, panel.group) == sprite) {
                    return i;
                }
            }
        }
        return -1;
    }
}

// mp_hud_panel_at(player, group, members, x, place): player's panel is the sprite group at group, its members'
// addresses at members bytes into it, its left edge set at x, and the board moves it by the floats at place.
extern "C" void mp_hud_panel_at(uint8_t* rdram, int32_t player, gpr group, int32_t members, int32_t x, gpr place) {
    if ((player < 0) || (player >= PANELS)) {
        return;
    }
    Panel& panel = panels[player];
    int32_t middle = x + PANEL_HALF;
    panel.group = group;
    panel.members = members;
    panel.place = place;
    panel.side = (middle < SCREEN_W / 3) ? -1 : (middle > SCREEN_W * 2 / 3) ? 1 : 0;
    panel.corner = ((middle < 0) || (middle > SCREEN_W)) ? Corner::Stop : Corner::Next;
    panel.last_x = panel.last_y = NAN;
}

extern "C" void mp_hud_group_killed(uint8_t* rdram, gpr group) {
    for (Panel& panel : panels) {
        if (panel.group == group) {
            panel.group = 0;
        }
    }
}

extern "C" void mp_hud_reset(uint8_t* rdram) {
    for (Panel& panel : panels) {
        panel.group = 0;
    }
}

// recomp_hud_4x3(picture_4x3), a native the patches call.
extern "C" void recomp_hud_4x3(uint8_t* rdram, recomp_context* ctx) {
    picture_4x3 = (int32_t)ctx->r4 != 0;
}

bool mp_display_list::picture_is_4x3() {
    return picture_4x3;
}

extern "C" void mp_hud_sprite_width(uint8_t* rdram, float width) {
    sprite_width = width;
}

extern "C" void mp_hud_sprite_start(uint8_t* rdram, gpr gfx, gpr sprite) {
    // A picture as wide as the screen: never a panel's, which is a third of it.
    float width = sprite_width;
    sprite_width = 0.0f;
    sprite_stretched = !picture_4x3 && (width >= SCREEN_W);
    if (sprite_stretched) {
        GfxCommand commands[2];
        gEXEnable(&commands[0]);
        gEXSetRectAspect(&commands[1], G_EX_ASPECT_STRETCH);
        emit(rdram, gfx, commands, 2);
        return;
    }

    // A panel's sprite follows its corner even while the picture is 4:3, and is aligned only when it isn't.
    int panel = panel_of(rdram, sprite);
    float in = (panel >= 0) ? eased_in(rdram, panels[panel], sprite) : 0.0f;
    sprite_aligned = !picture_4x3 && (panel >= 0) && (panels[panel].side != 0);
    if (!sprite_aligned) {
        return;
    }

    // The alignment a fraction of the window from its left edge (0) to its right (G_EX_ORIGIN_RIGHT): from the panel's
    // edge to the middle (half of it) as it is eased in. A rectangle's x is from there less as much of the screen,
    // which RT64 adds back as it does: from the left edge, the 4:3 middle's left edge, or the right edge less the
    // screen's width.
    float edge = 0.5f * (1.0f - in);
    uint16_t origin = (uint16_t)std::lround(G_EX_ORIGIN_RIGHT * (0.5f + panels[panel].side * edge));
    int32_t offset = -(int32_t)((origin * SCREEN_W * 4) / G_EX_ORIGIN_RIGHT);
    GfxCommand commands[7];
    gEXEnable(&commands[0]);
    gEXSetRectAlign(&commands[1], origin, origin, offset, 0, offset, 0);
    scissor_to_window(&commands[3]);
    whole_screen(&commands[6]);
    emit(rdram, gfx, commands, 7);
}

extern "C" void mp_hud_sprite_end(uint8_t* rdram, gpr gfx) {
    if (sprite_stretched) {
        sprite_stretched = false;
        GfxCommand commands[2];
        gEXEnable(&commands[0]);
        gEXSetRectAspect(&commands[1], G_EX_ASPECT_AUTO);
        emit(rdram, gfx, commands, 2);
        return;
    }
    if (!sprite_aligned) {
        return;
    }
    sprite_aligned = false;

    GfxCommand commands[7];
    gEXEnable(&commands[0]);
    gEXSetRectAlign(&commands[1], G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    scissor_own(&commands[3]);
    whole_screen(&commands[6]);
    emit(rdram, gfx, commands, 7);
}
