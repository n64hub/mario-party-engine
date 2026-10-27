// A board's HUD at the picture's sides: in a picture wider than 4:3, the four players' panels (each a sprite group of
// the sprite manager) drawn at the window's left and right edges, as far in from them as they are from 4:3's on a
// console, the way the PC ports of N64 games that widen the picture draw a HUD. Everything else the sprite manager
// draws (text windows, menus, a minigame's sprites) stays in the 4:3 middle, where RT64 draws a rectangle by default.
//
// A game's hooks (its us.toml) call these natives, since the sprite manager's code is the decompilations' asm:
// - mp_hud_panel where the board sets a player's panel's place (MBStatusPosSet and its kin), with the panel's group:
//   the panel is drawn at the side its place is on, a panel in the middle third stays in the middle, and a panel the
//   game moves is drawn where it moved to.
// - mp_hud_group_killed where the sprite manager kills a group (HuSprGrpKill), and mp_hud_reset where it starts again
//   with no groups (its init, which forgets them without killing them), so a group the board's panels used is
//   forgotten when the board frees them, before the memory goes to another group.
// - mp_hud_sprite_start and mp_hud_sprite_end around the function that draws one sprite into the display list: for a
//   panel's sprite, RT64's rectangle alignment to the window's edge (gEXSetRectAlign) and every scissor until the end
//   aligned to the window's edges (gEXSetScissorAlign, then a scissor of the whole screen): the sprite pass's and a
//   sprite's own scissors stop at 4:3's edges, which would cut off a panel moved out past them. After the sprite,
//   both go back to RT64's own and the scissor to the whole screen, the sprite pass's.
// - recomp_hud_4x3, from the engine's patches/board_back.c, whether the picture is 4:3: a board's opening scene's map
//   is no wider than 4:3, with black at the window's sides, and the panels stay where the game puts them, inside 4:3
//   with the scene, rather than out over the black.
//
// RT64 reads extended commands only after a gEXEnable in the same display list, so each start sends one.

#include "display_list.h"

using namespace mp_display_list;

namespace {
    constexpr int PANELS = 4;
    // A panel is as wide as about a third of the screen: its place (its left edge) plus half that is its middle.
    constexpr int32_t PANEL_HALF = SCREEN_W / 6;
    // A sprite group's count of members, a u16, in every game.
    constexpr int GROUP_COUNT = 0x0A;

    struct Panel {
        gpr group;
        int32_t members;
        uint16_t origin;
    };

    Panel panels[PANELS];
    // The alignment the sprite being drawn started, G_EX_ORIGIN_NONE for none.
    uint16_t sprite_origin = G_EX_ORIGIN_NONE;
    // Whether the picture is 4:3.
    bool picture_4x3 = false;

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

// mp_hud_panel(player, group, members, x): player's panel is the sprite group at group, its members' addresses at
// members bytes into it, its left edge at x.
extern "C" void mp_hud_panel(uint8_t* rdram, int32_t player, gpr group, int32_t members, int32_t x) {
    if ((player < 0) || (player >= PANELS)) {
        return;
    }
    Panel& panel = panels[player];
    int32_t middle = x + PANEL_HALF;
    panel.group = group;
    panel.members = members;
    if (middle < SCREEN_W / 3) {
        panel.origin = G_EX_ORIGIN_LEFT;
    }
    else if (middle > SCREEN_W * 2 / 3) {
        panel.origin = G_EX_ORIGIN_RIGHT;
    }
    else {
        panel.origin = G_EX_ORIGIN_NONE;
    }
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

extern "C" void mp_hud_sprite_start(uint8_t* rdram, gpr gfx, gpr sprite) {
    int panel = picture_4x3 ? -1 : panel_of(rdram, sprite);
    sprite_origin = (panel < 0) ? G_EX_ORIGIN_NONE : panels[panel].origin;
    if (sprite_origin == G_EX_ORIGIN_NONE) {
        return;
    }

    // A rectangle's x from the window's left edge, or from its right edge less the screen's width.
    int32_t offset = (sprite_origin == G_EX_ORIGIN_RIGHT) ? -SCREEN_W * 4 : 0;
    GfxCommand commands[7];
    gEXEnable(&commands[0]);
    gEXSetRectAlign(&commands[1], sprite_origin, sprite_origin, offset, 0, offset, 0);
    scissor_to_window(&commands[3]);
    whole_screen(&commands[6]);
    emit(rdram, gfx, commands, 7);
}

extern "C" void mp_hud_sprite_end(uint8_t* rdram, gpr gfx) {
    if (sprite_origin == G_EX_ORIGIN_NONE) {
        return;
    }
    sprite_origin = G_EX_ORIGIN_NONE;

    GfxCommand commands[7];
    gEXEnable(&commands[0]);
    gEXSetRectAlign(&commands[1], G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    scissor_own(&commands[3]);
    whole_screen(&commands[6]);
    emit(rdram, gfx, commands, 7);
}
