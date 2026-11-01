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
// - mp_hud_item_at where the board carries the item out of a player's panel (to over the character as it is used, a
//   short way out, or back in), before each move, with the item's sprite (a member of the panel's group) and where it
//   keeps its place in the group (two floats, x and y). Its place at the first carry since the panel's group was set
//   is where it rests in the panel, and wherever it is drawn away from there, carried or put somewhere by other code,
//   it eases into the 4:3 middle over its first CARRY pixels, further than the panel if it must: the board carries it
//   to its 4:3 places, over a character standing in the middle of the screen. Back at rest it is the panel's again.
//   A game whose hook says where each sprite is on the screen (mp_hud_sprite_at) has every item the board has moved
//   aligned by that instead: as the panel whose box (its place, a third of the screen wide and PANEL_H tall) it is
//   nearest, eased into the 4:3 middle over the first CARRY pixels out of that box. So an item flown from one panel to
//   another (Mario Party 3's Barter Box, its items put in the other player's panel) leaves the one panel where it is
//   drawn and lands in the other where that is drawn, rather than at its 4:3 place, which is inside it.
// - mp_hud_hand_read where the game reads the picture of the hand that carries a player's item, and mp_hud_hand_group
//   where it next sets a group's picture, the hand's group: that player's hand is drawn with its panel and eased as
//   far as the item, so it holds the item from where the panel is drawn to over the character. A game's hook calls
//   mp_hud_hand_read only for the player whose turn it is, whose item it is: another player's hand (a board's event
//   at their panel) stays where the game puts it, with the sprites it comes with.
// - mp_hud_group_killed where the sprite manager kills a group (HuSprGrpKill), and mp_hud_reset where it starts again
//   with no groups (its init, which forgets them without killing them), so a group the board's panels or hands used
//   is forgotten when the board frees them, before the memory goes to another group.
// - mp_hud_sprite_start and mp_hud_sprite_end around the function that draws one sprite into the display list: for a
//   panel's sprite, RT64's rectangle alignment to the window's edge (gEXSetRectAlign) and every scissor until the end
//   aligned to the window's edges (gEXSetScissorAlign, then a scissor of the whole screen): the sprite pass's and a
//   sprite's own scissors stop at 4:3's edges, which would cut off a panel drawn out past them. After the sprite,
//   both go back to RT64's own and the scissor to the whole screen, the sprite pass's.
// - mp_hud_sprite_shift, from a game's hook where the function that draws a sprite reads its x for the matrix it
//   draws a sprite through (two triangles, which RT64's rectangle alignment doesn't move: Mario Party 3's items in a
//   panel's item select), how far to move that x: as far in the screen's pixels as the alignment moves the
//   sprite's rectangles, so it is drawn where they would be. 0 for a sprite that isn't aligned. RT64 draws the sprite
//   pass's 2D through a projection it fits to the window, 4:3 in its middle, so a triangle moved past 4:3 shows there.
// - mp_hud_sprite_at, from a game's hook just before mp_hud_sprite_start, where the sprite is on the screen (its x and
//   y, in the screen's pixels), for its items as mp_hud_item_at says.
// - mp_hud_sprite_width, from a game's hook just before mp_hud_sprite_start, how wide the sprite is on the screen (its
//   frame's width by its scale; a game whose hook doesn't say has every sprite drawn as RT64 draws it). One at least as
//   wide as the screen is a picture behind the scene, and its rectangles are drawn stretched over the window
//   (gEXSetRectAspect G_EX_ASPECT_STRETCH) until the sprite's end. RT64 stretches a rectangle by itself only while it
//   covers the whole width of the screen's scissor, so such a picture moved a few pixels (Mario Party 2's Abandon
//   Ship's sky following the camera in its finish shot, from 0 to -6) dropped into the 4:3 middle, with the frame's
//   clear beside it, while the scene's 3D went on to the window's edges. In a 4:3 picture it is kept to 4:3
//   (G_EX_ASPECT_ADJUST): RT64 by itself would stretch the rectangles of one that covers the scissor's whole width,
//   only its middle showing between the black at the window's sides and what stands on it no longer on it (Mario
//   Party 2's Bumper Balloon Cars' sand under the cars).
// - recomp_hud_widest, from the engine's patches/board_back.c, the widest the picture is drawn. In a 4:3 picture (a
//   board's opening scene's map is no wider than 4:3, with black at the window's sides) the panels stay where the game
//   puts them, inside 4:3 with the scene, rather than out over the black. In a picture drawn no wider than an aspect
//   past 4:3, in a window wider still, they go to its edges rather than the window's, aligned from the window's centre.
//   src/wipe.cpp asks it too (picture_is_4x3, picture_short_half).
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
    // How far in from its corner the game moves a panel before it is drawn wholly in the 4:3 middle: as far as Mario
    // Party 2's 1 VS 3 teams move the lower right panel, 8 in and 2 up (a 2 VS 2 game moves the lower panels 16).
    // Further, that panel stopped half eased, out of line with the panels of its column.
    constexpr float EASE = 8.0f;
    // How far the board carries a panel's item from its rest before it is drawn wholly in the 4:3 middle: less than
    // Mario Party 2's shortest carry to or from over the character, 61 pixels (the upper left panel's, as an item is
    // put back), so the item and its hand are there at its 4:3 place over the character. A short carry, 32, stops part
    // way.
    constexpr float CARRY = 48.0f;
    // How tall a panel is, from its place: Mario Party 3's, whose items rest 14 to 24 pixels below it.
    constexpr float PANEL_H = 40.0f;
    // How many of the items the board has moved a panel keeps: Mario Party 3's Barter Box flies three at once.
    constexpr int MOVED = 3;

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
        // Its place when its first sprite was last drawn, once a frame, and how far it was last eased in.
        float last_x, last_y;
        float in;
        // The group of the hand that carries its item, 0 for none, and the item's sprite, 0 until the board first
        // carries it, with where the group keeps its place and its place at rest, and the item's alignment's fraction of
        // the window when it was last drawn where a game's hook said (NAN until then).
        gpr hand;
        gpr item;
        gpr item_place;
        float rest_x, rest_y;
        float item_fraction;
        // The last MOVED items the board has moved, 0 for none.
        gpr moved[MOVED];
        int moved_next;
    };

    Panel panels[PANELS];
    // The player whose hand the game read last, until it sets the hand's group's picture: -1 for none.
    int hand_read = -1;
    // Whether the sprite being drawn started an alignment, and how far that moves its rectangles, in the screen's pixels.
    bool sprite_aligned = false;
    float sprite_shift = 0.0f;
    // How wide the next sprite is on the screen, from mp_hud_sprite_width, and whether the one being drawn is a picture
    // as wide as the screen, with an aspect of its own until its end.
    float sprite_width = 0.0f;
    bool sprite_picture = false;
    // Where the next sprite is on the screen, from mp_hud_sprite_at, and whether a game's hook said.
    float sprite_x = 0.0f, sprite_y = 0.0f;
    bool sprite_at = false;
    // The widest the picture is drawn, 0 for the window's width, and whether that is 4:3.
    float picture_widest = 0.0f;
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

    // How far panel's item is carried toward the 4:3 middle, 0 to 1, by how far it is from its rest.
    float carried(uint8_t* rdram, const Panel& panel) {
        if (panel.item == 0) {
            return 0.0f;
        }
        float x = mem_f32(rdram, panel.item_place) - panel.rest_x;
        float y = mem_f32(rdram, panel.item_place + 4) - panel.rest_y;
        return std::fmin(std::sqrt(x * x + y * y) / CARRY, 1.0f);
    }

    // The alignment's fraction of the window for panel eased in this far: from its edge (0 or 1) to the middle (half).
    float panel_fraction(const Panel& panel, float in) {
        return 0.5f + panel.side * 0.5f * (1.0f - in);
    }

    // The alignment's fraction of the window for an item drawn at x, y on the screen: the fraction of the panel whose
    // box it is nearest, eased into the middle over the first CARRY pixels out of that box.
    float item_fraction(uint8_t* rdram, float x, float y) {
        float nearest = INFINITY;
        float fraction = 0.5f;
        for (const Panel& panel : panels) {
            if (panel.group == 0) {
                continue;
            }
            float left = mem_f32(rdram, panel.place);
            float top = mem_f32(rdram, panel.place + 4);
            float out_x = std::fmax(std::fmax(left - x, x - (left + PANEL_HALF * 2)), 0.0f);
            float out_y = std::fmax(std::fmax(top - y, y - (top + PANEL_H)), 0.0f);
            float out = std::sqrt(out_x * out_x + out_y * out_y);
            if (out < nearest) {
                nearest = out;
                fraction = panel_fraction(panel, panel.in);
            }
        }
        return 0.5f + (fraction - 0.5f) * (1.0f - std::fmin(nearest / CARRY, 1.0f));
    }

    bool was_moved(const Panel& panel, gpr sprite) {
        for (gpr item : panel.moved) {
            if (item == sprite) {
                return true;
            }
        }
        return false;
    }

    void forget_items(Panel& panel) {
        panel.item = 0;
        panel.item_fraction = NAN;
        std::memset(panel.moved, 0, sizeof(panel.moved));
    }

    // Whether the sprite group at group, its members' addresses at members bytes into it, has this sprite as a member.
    bool has_member(uint8_t* rdram, gpr group, int32_t members, gpr sprite) {
        uint16_t count = MEM_HU(GROUP_COUNT, group);
        for (uint16_t m = 0; m < count; m++) {
            if ((gpr)(int32_t)MEM_W(members + m * 4, group) == sprite) {
                return true;
            }
        }
        return false;
    }

    // Which panel's group has this sprite as a member, -1 for none, or else which panel's hand's group (hand true).
    int panel_of(uint8_t* rdram, gpr sprite, bool& hand) {
        hand = false;
        for (int i = 0; i < PANELS; i++) {
            const Panel& panel = panels[i];
            if ((panel.group != 0) && has_member(rdram, panel.group, panel.members, sprite)) {
                return i;
            }
        }
        for (int i = 0; i < PANELS; i++) {
            const Panel& panel = panels[i];
            if ((panel.group != 0) && (panel.hand != 0) && has_member(rdram, panel.hand, panel.members, sprite)) {
                hand = true;
                return i;
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
    if (panel.group != group) {
        forget_items(panel);
        panel.in = 0.0f;
    }
    panel.group = group;
    panel.members = members;
    panel.place = place;
    panel.side = (middle < SCREEN_W / 3) ? -1 : (middle > SCREEN_W * 2 / 3) ? 1 : 0;
    panel.corner = ((middle < 0) || (middle > SCREEN_W)) ? Corner::Stop : Corner::Next;
    panel.last_x = panel.last_y = NAN;
}

// mp_hud_item_at(player, sprite, place): the board is about to move player's item, the sprite at sprite, which keeps
// its place, x and y, in the floats at place.
extern "C" void mp_hud_item_at(uint8_t* rdram, int32_t player, gpr sprite, gpr place) {
    if ((player < 0) || (player >= PANELS) || (panels[player].group == 0) || (panels[player].item == sprite)) {
        return;
    }
    Panel& panel = panels[player];
    if (!was_moved(panel, sprite)) {
        panel.moved[panel.moved_next] = sprite;
        panel.moved_next = (panel.moved_next + 1) % MOVED;
    }
    panel.item = sprite;
    panel.item_fraction = NAN;
    panel.item_place = place;
    panel.rest_x = mem_f32(rdram, place);
    panel.rest_y = mem_f32(rdram, place + 4);
}

extern "C" void mp_hud_hand_read(uint8_t* rdram, int32_t player) {
    hand_read = ((player >= 0) && (player < PANELS)) ? player : -1;
}

extern "C" void mp_hud_hand_group(uint8_t* rdram, gpr group) {
    if (hand_read < 0) {
        return;
    }
    // A new hand holds no item yet: the last one's fraction may be where it was put away, over the character.
    panels[hand_read].hand = group;
    panels[hand_read].item_fraction = NAN;
    hand_read = -1;
}

extern "C" void mp_hud_group_killed(uint8_t* rdram, gpr group) {
    for (Panel& panel : panels) {
        if (panel.group == group) {
            panel.group = 0;
            forget_items(panel);
        }
        if (panel.hand == group) {
            panel.hand = 0;
        }
    }
}

extern "C" void mp_hud_reset(uint8_t* rdram) {
    for (Panel& panel : panels) {
        panel.group = 0;
        panel.hand = 0;
        forget_items(panel);
    }
    hand_read = -1;
}

// recomp_hud_widest(widest), a native the patches call, its float in f12.
extern "C" void recomp_hud_widest(uint8_t* rdram, recomp_context* ctx) {
    picture_widest = ctx->f12.fl;
    picture_4x3 = (picture_widest > 0.0f) && (picture_widest <= 4.0f / 3.0f);
}

bool mp_display_list::picture_is_4x3() {
    return picture_4x3;
}

float mp_display_list::picture_short_half() {
    if (picture_4x3 || (picture_widest <= 0.0f) || (picture_widest >= picture_aspect())) {
        return 0.0f;
    }
    return SCREEN_H * picture_widest / 2.0f;
}

extern "C" void mp_hud_sprite_width(uint8_t* rdram, float width) {
    sprite_width = width;
}

extern "C" void mp_hud_sprite_at(uint8_t* rdram, float x, float y) {
    sprite_x = x;
    sprite_y = y;
    sprite_at = true;
}

extern "C" void mp_hud_sprite_start(uint8_t* rdram, gpr gfx, gpr sprite) {
    // A picture as wide as the screen: never a panel's, which is a third of it.
    float width = sprite_width;
    sprite_width = 0.0f;
    bool at = sprite_at;
    sprite_at = false;
    sprite_picture = (width >= SCREEN_W);
    if (sprite_picture) {
        GfxCommand commands[2];
        gEXEnable(&commands[0]);
        gEXSetRectAspect(&commands[1], picture_4x3 ? G_EX_ASPECT_ADJUST : G_EX_ASPECT_STRETCH);
        emit(rdram, gfx, commands, 2);
        return;
    }

    // A panel's sprite follows its corner even while the picture is 4:3, and is aligned only when it isn't. Its item,
    // and the hand that carries it, are as far in as the panel or as far as the item is carried, whichever is further:
    // or where a game's hook says it is, an item the board has moved as item_fraction says, and the hand as its item.
    bool hand = false;
    int panel = panel_of(rdram, sprite, hand);
    float fraction = 0.5f;
    if (panel >= 0) {
        Panel& owner = panels[panel];
        float in = eased_in(rdram, owner, sprite);
        if (!hand) {
            owner.in = in;
        }
        if (at && !hand && was_moved(owner, sprite)) {
            fraction = item_fraction(rdram, sprite_x, sprite_y);
            if (sprite == owner.item) {
                owner.item_fraction = fraction;
            }
        } else if (hand && !std::isnan(owner.item_fraction)) {
            fraction = owner.item_fraction;
        } else if (hand || (sprite == owner.item)) {
            fraction = panel_fraction(owner, std::fmax(in, carried(rdram, owner)));
        } else {
            fraction = panel_fraction(owner, in);
        }
    }
    sprite_aligned = !picture_4x3 && (panel >= 0) && ((panels[panel].side != 0) || (fraction != 0.5f));
    if (!sprite_aligned) {
        return;
    }

    // The alignment a fraction of the window from its left edge (0) to its right (G_EX_ORIGIN_RIGHT): from the panel's
    // edge to the middle (half of it) as it is eased in. A rectangle's x is from there less as much of the screen,
    // which RT64 adds back as it does: from the left edge, the 4:3 middle's left edge, or the right edge less the
    // screen's width. In a picture that stops short of the window's edges, the same fraction of the picture, from the
    // window's centre (where a rectangle's x is from the centre).
    float half = picture_short_half();
    uint16_t origin = (uint16_t)std::lround(G_EX_ORIGIN_RIGHT * fraction);
    int32_t offset = -(int32_t)((origin * SCREEN_W * 4) / G_EX_ORIGIN_RIGHT);
    if (half > 0.0f) {
        origin = G_EX_ORIGIN_CENTER;
        offset = (int32_t)std::lround(((fraction - 0.5f) * 2.0f * half - fraction * SCREEN_W) * 4.0f);
    }
    // A rectangle drawn at x is at the window's x origin's fraction of it plus x plus the offset, so in the screen's
    // pixels, from the 4:3 middle's left edge, the window's own left edge less (wide - SCREEN_W) / 2.
    float wide = SCREEN_H * picture_aspect();
    sprite_shift = wide * origin / G_EX_ORIGIN_RIGHT + offset / 4.0f - (wide - SCREEN_W) / 2.0f;
    GfxCommand commands[7];
    gEXEnable(&commands[0]);
    gEXSetRectAlign(&commands[1], origin, origin, offset, 0, offset, 0);
    scissor_to_window(&commands[3]);
    whole_screen(&commands[6]);
    emit(rdram, gfx, commands, 7);
}

extern "C" float mp_hud_sprite_shift(uint8_t* rdram) {
    return sprite_aligned ? sprite_shift : 0.0f;
}

extern "C" void mp_hud_sprite_end(uint8_t* rdram, gpr gfx) {
    if (sprite_picture) {
        sprite_picture = false;
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
    sprite_shift = 0.0f;

    GfxCommand commands[7];
    gEXEnable(&commands[0]);
    gEXSetRectAlign(&commands[1], G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    scissor_own(&commands[3]);
    whole_screen(&commands[6]);
    emit(rdram, gfx, commands, 7);
}
