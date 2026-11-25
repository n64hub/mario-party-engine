// A board's HUD at the picture's sides: in a picture wider than 4:3, the four players' panels (each a sprite group of
// the sprite manager) drawn at the window's left and right edges, as far in from them as they are from 4:3's on a
// console, the way the PC ports of N64 games that widen the picture draw a HUD. A picture at least as wide as the
// screen (a minigame's sky behind its 3D) is drawn stretched over the whole window wherever the game puts it. So is a
// minigame's corner HUD (each player's name and coins, in Mario Party's Grab Bag, Crane Game and Bash 'n' Cash) at the
// window's sides. A bar across the screen (Bobsled Run's progress bar between its two views) is stretched over the
// window with what is on it. Everything else the sprite manager draws (text windows, menus, a minigame's other sprites, its timer)
// stays in the 4:3 middle, where RT64 draws a rectangle by default. The letters of the game's big messages (a minigame's winner's name and WINS!)
// stay there too, but reach out to the window's edges: they come in from five times their size, wider than the
// screen, and their outer letters were cut off at 4:3's edges.
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
// - mp_hud_side_sprites where a minigame has made its corner HUD, with where it keeps the indexes of the HUD's basic
//   sprites (the decompilation's esprite, each a group of the sprite manager) and how many: each is drawn from the
//   window's edge on the side its x is on, as far in from it as it is from 4:3's, as a panel at its corner. The game
//   puts them at hardcoded 4:3 places, so in a wider window they sat in its middle, the right ones over the scene.
// - mp_hud_bar where a minigame has made a bar across the screen of basic sprites, with where it keeps the indexes of
//   one of each of its pieces' pictures and the rows of the screen over it: every sprite drawn from those pictures is
//   stretched over the window (gEXSetRectAspect G_EX_ASPECT_STRETCH), and every other sprite drawn with its y on those
//   rows is aligned as far across the window as its x is across the screen, at its own size. The game puts the bar at
//   4:3's width, so in a wider window it stopped at 4:3's edges between views going on to the window's, its start and
//   finish with it. A sprite drawn from the bar's pictures under another group is stretched as well. A piece less than
//   BAR_GAP in from the screen's left edge is drawn from it, wider by as far (Bobsled Run's first tile, 8 in), so the
//   bar reaches the window's left edge as it reaches its right.
// - mp_hud_group_killed where the sprite manager kills a group (HuSprGrpKill), and mp_hud_reset where it starts again
//   with no groups (its init, which forgets them without killing them), so a group the board's panels or hands, a
//   message or a corner HUD used is forgotten when it is freed, before the memory goes to another group.
// - mp_hud_sprite_start and mp_hud_sprite_end around the function that draws one sprite into the display list: for a
//   panel's sprite, RT64's rectangle alignment to the window's edge (gEXSetRectAlign) and every scissor until the end
//   aligned to the window's edges (gEXSetScissorAlign, then a scissor of the whole screen): the sprite pass's and a
//   sprite's own scissors stop at 4:3's edges, which would cut off a panel drawn out past them. After the sprite,
//   both go back to RT64's own and the scissor to the whole screen, the sprite pass's.
//   For every sprite, mp_hud_sprite_end also joins its strips. The sprite manager draws a frame a few rows at a time,
//   each strip a texture rectangle of its own rows loaded into TMEM (the whole frame won't fit), and RT64 draws a
//   magnified one at the window's resolution, filtered: each strip's last row was filtered with only itself, so a
//   hard line ran across the picture at each join (Mario Party's sky behind a minigame's rules, 160 by 120 drawn at
//   twice its size, 12 rows at a time: a line every 24 of the screen's 240). Where a strip goes on from the one above
//   it (the next rows of the same texture, as wide and as magnified, a whole number of quarter pixels a texel), the
//   one above stops a texel short, the next starts a texel later, and between them the two rows each side of the join
//   are loaded and drawn over those two texels, filtered as one texture. To make room, its commands drop their syncs
//   (RT64 has no use for them) and each strip's texture image and tile settings that are the same as those already
//   set. A sprite whose commands would still not fit in their own place is left as it was.
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
// - mp_hud_message_group where the game's message module makes the sprite group of a message's letters (Mario Party's
//   GMesFontCreate), with the group and its members' offset in it: in a picture wider than 4:3 its letters are drawn
//   out to the window's edges, under a scissor aligned to them (as a panel's), in the 4:3 middle still. The game
//   draws such a message from five times its size down to its own (WINS!, after a minigame), its outer letters past
//   the screen's edges, and cuts a texture rectangle off at x 0 in its own code (gSPScisTextureRectangle), so
//   mp_hud_sprite_reach, from a game's hook just after mp_hud_sprite_start, moves a message's sprite drawn as
//   rectangles right by as far as the window reaches past 4:3 (its x, an s16 at the address given, put back by
//   mp_hud_sprite_end), and aligns its rectangles from the screen's centre as far back left (gEXSetRectAlign): it is
//   drawn where it was, and cut off at the window's edge, not 4:3's. mp_hud_sprite_left and mp_hud_sprite_right,
//   from a game's hook where the sprite manager drops a sprite wholly off the screen (its left past 320, its right
//   before 0), say where the window's edges are in the sprite's x, moved or not, so one in the window's sides is drawn.
//   Every other sprite in the 4:3 middle (none of a panel's, a side's or the bar's) is drawn the same way, where the
//   game puts it and cut off at the window's edges: Shell Game's coins, poured from past the screen's top-left corner
//   into the player, were cut off at 4:3's left edge, and those wholly past it not drawn.
// - recomp_hud_widest, from the engine's patches/board_back.c, the widest the picture is drawn. In a 4:3 picture (a
//   board's opening scene's map is no wider than 4:3, with black at the window's sides) the panels stay where the game
//   puts them, inside 4:3 with the scene, rather than out over the black. In a picture drawn no wider than an aspect
//   past 4:3, in a window wider still, they go to its edges rather than the window's, aligned from the window's centre.
//   src/wipe.cpp asks it too (picture_is_4x3, picture_short_half).
//
// RT64 reads extended commands only after a gEXEnable in the same display list, so each start sends one.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "display_list.h"

using namespace mp_display_list;

namespace {
    constexpr int PANELS = 4;
    // A panel is as wide as about a third of the screen: its place (its left edge) plus half that is its middle.
    constexpr int32_t PANEL_HALF = SCREEN_W / 6;
    // A sprite group's count of members, a u16, in every game.
    constexpr int GROUP_COUNT = 0x0A;
    // How many messages' groups are kept: Mario Party's message module keeps up to 16 groups a message, and two
    // messages may be up at once.
    constexpr int MESSAGES = 32;
    // Half the screen, from its left edge to its centre, where a rectangle aligned to the screen's centre is placed from.
    constexpr int32_t HALF = SCREEN_W / 2;
    // The furthest a message's sprite is moved right: a texture rectangle's x (10.2) stops at 1023, and a message's
    // letter is less than 320 wide.
    constexpr int32_t REACH_MOST = SCREEN_W;
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
    // How many groups drawn at a side are kept: a minigame's corner HUD has twenty, five sprites for each player.
    constexpr int SIDES = 32;
    // Mario Party's basic sprites (the decompilation's esprite, D_800ED60C): 0x24 bytes each, with the index of its
    // group in the sprite manager's table an s16 at 4 and its x an s16 at 0x0A.
    constexpr int BASIC_SIZE = 0x24;
    constexpr int BASIC_GROUP = 0x04;
    constexpr int BASIC_X = 0x0A;
    // A sprite of Mario Party's sprite manager: its x and y, s16s at 4 and 6, its scale across, a float at 0x14, and its
    // picture's data (the frames its hook reads its width from), a pointer at 0x4C.
    constexpr int SPRITE_X = 0x04;
    constexpr int SPRITE_Y = 0x06;
    constexpr int SPRITE_SCALE_X = 0x14;
    constexpr int SPRITE_DATA = 0x4C;
    // How many of a bar's pieces are kept: Bobsled Run's has two, its tiles and its finish post.
    constexpr int BAR_PIECES = 4;
    // How far in from the screen's left edge a piece of the bar may start and still be drawn from it: a tile's width.
    constexpr int BAR_GAP = 16;

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
    // The groups drawn at a side of the window (mp_hud_side_sprites), 0 for none, with that side: -1 left, 1 right.
    struct Side {
        gpr group;
        int side;
    };
    Side sides[SIDES];
    int32_t side_members = 0;
    int sides_next = 0;
    // A bar across the screen (mp_hud_bar): its pieces' groups, 0 for none, and their pictures' data, and the rows of the
    // screen over it, from bar_top up to bar_bottom.
    struct BarPiece {
        gpr group;
        gpr data;
    };
    BarPiece bar[BAR_PIECES];
    int32_t bar_top = 0, bar_bottom = 0;
    // The messages' groups, 0 for none, and their members' offset in a group.
    gpr messages[MESSAGES];
    int32_t message_members = 0;
    int messages_next = 0;
    // Whether the sprite being drawn reaches past 4:3 (a message's, or one in the 4:3 middle), in a picture wider than
    // 4:3, how far the window reaches past 4:3 on each side, in the screen's pixels, and where its x is if
    // mp_hud_sprite_reach moved it that far (0 for not).
    bool sprite_reaching = false;
    int32_t sprite_reach = 0;
    gpr sprite_moved = 0;
    // The player whose hand the game read last, until it sets the hand's group's picture: -1 for none.
    int hand_read = -1;
    // Whether the sprite being drawn started an alignment, and how far that moves its rectangles, in the screen's pixels.
    bool sprite_aligned = false;
    float sprite_shift = 0.0f;
    // How wide the next sprite is on the screen, from mp_hud_sprite_width, and whether the one being drawn is a picture
    // as wide as the screen, with an aspect of its own until its end.
    float sprite_width = 0.0f;
    bool sprite_picture = false;
    // Whether the one being drawn is a piece of the bar, stretched as a picture under a scissor of the whole window.
    bool sprite_bar = false;
    // A piece of the bar drawn from the screen's left edge: the sprite, 0 for none, and the x and scale it had.
    gpr bar_moved = 0;
    int16_t bar_x = 0;
    uint32_t bar_scale = 0;
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

    // Whether one of the messages' groups has this sprite as a member.
    bool is_message(uint8_t* rdram, gpr sprite) {
        for (gpr group : messages) {
            if ((group != 0) && has_member(rdram, group, message_members, sprite)) {
                return true;
            }
        }
        return false;
    }

    // The side of the window this sprite's group is drawn at, as mp_hud_side_sprites set: -1 left, 1 right, 0 none.
    int side_of(uint8_t* rdram, gpr sprite) {
        for (const Side& known : sides) {
            if ((known.group != 0) && has_member(rdram, known.group, side_members, sprite)) {
                return known.side;
            }
        }
        return 0;
    }

    // How a sprite is drawn with the bar: as a Piece of it (drawn from one of its pieces' pictures), Over it (drawn with
    // its y on the bar's rows), or Off it, every sprite while there is no bar.
    enum class OnBar {
        Off,
        Piece,
        Over,
    };

    OnBar on_bar(uint8_t* rdram, gpr sprite) {
        bool any = false;
        gpr data = (gpr)(int32_t)MEM_W(SPRITE_DATA, sprite);
        for (const BarPiece& piece : bar) {
            if (piece.group == 0) {
                continue;
            }
            any = true;
            if (piece.data == data) {
                return OnBar::Piece;
            }
        }
        int16_t y = (int16_t)MEM_H(SPRITE_Y, sprite);
        return (any && (y >= bar_top) && (y < bar_bottom)) ? OnBar::Over : OnBar::Off;
    }

    // How far the picture reaches past 4:3 on each side, in the screen's pixels, rounded up: to the window's edges, or
    // to its own in a picture that stops short of them.
    int32_t reach_past_4x3() {
        float half = picture_short_half();
        float wide = (half > 0.0f) ? half * 2.0f : SCREEN_H * picture_aspect();
        int32_t reach = (int32_t)std::ceil((wide - SCREEN_W) / 2.0f);
        return std::min(std::max(reach, 0), REACH_MOST);
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

// mp_hud_message_group(group, members): the sprite group at group, its members' addresses at members bytes into it,
// holds a message's letters.
extern "C" void mp_hud_message_group(uint8_t* rdram, gpr group, int32_t members) {
    message_members = members;
    for (gpr known : messages) {
        if (known == group) {
            return;
        }
    }
    messages[messages_next] = group;
    messages_next = (messages_next + 1) % MESSAGES;
}

// mp_hud_side_sprites(ids, count, basic, groups, members): count of the game's basic sprites, their indexes s16s at
// ids, in its basic sprites at basic (laid out as BASIC_SIZE says), each a group in the sprite manager's table of
// groups' addresses at groups, its members' addresses at members bytes into it. Each is drawn at the side of the
// window its x (where the game has put it) is on, left or right of the screen's centre.
extern "C" void mp_hud_side_sprites(uint8_t* rdram, gpr ids, int32_t count, gpr basic, gpr groups, int32_t members) {
    side_members = members;
    for (int32_t i = 0; i < count; i++) {
        int16_t id = (int16_t)MEM_H(i * 2, ids);
        if (id < 0) {
            continue;
        }
        gpr sprite = basic + id * BASIC_SIZE;
        int16_t index = (int16_t)MEM_H(BASIC_GROUP, sprite);
        int16_t x = (int16_t)MEM_H(BASIC_X, sprite);
        if (index < 0) {
            continue;
        }
        gpr group = (gpr)(int32_t)MEM_W(index * 4, groups);
        if (group == 0) {
            continue;
        }
        Side* slot = nullptr;
        for (Side& known : sides) {
            if (known.group == group) {
                slot = &known;
            }
        }
        if (slot == nullptr) {
            slot = &sides[sides_next];
            sides_next = (sides_next + 1) % SIDES;
        }
        slot->group = group;
        slot->side = (x < HALF) ? -1 : 1;
    }
}

// mp_hud_bar(ids, count, basic, groups, members, top, bottom): count of the game's basic sprites, their indexes at ids
// (as mp_hud_side_sprites reads them), are the pieces of a bar across the screen, whose sprites are over it from row top
// of the screen up to bottom. Every sprite drawn from one of the pieces' pictures (the bar's other tiles, the same
// picture over and over) is stretched with the screen over the window, as a picture, and every other sprite drawn with
// its y over the bar where its x falls on the stretched bar, at its own size: a minigame's progress bar between its
// two views (Mario Party's Bobsled Run), its racers' icons going from its start to its finish. Until the pieces'
// groups are killed.
extern "C" void mp_hud_bar(uint8_t* rdram, gpr ids, int32_t count, gpr basic, gpr groups, int32_t members,
                           int32_t top, int32_t bottom) {
    std::memset(bar, 0, sizeof(bar));
    bar_top = top;
    bar_bottom = bottom;
    for (int32_t i = 0; (i < count) && (i < BAR_PIECES); i++) {
        int16_t id = (int16_t)MEM_H(i * 2, ids);
        if (id < 0) {
            continue;
        }
        int16_t index = (int16_t)MEM_H(BASIC_GROUP, basic + id * BASIC_SIZE);
        if (index < 0) {
            continue;
        }
        gpr group = (gpr)(int32_t)MEM_W(index * 4, groups);
        if ((group == 0) || (MEM_HU(GROUP_COUNT, group) == 0)) {
            continue;
        }
        gpr sprite = (gpr)(int32_t)MEM_W(members, group);
        bar[i].group = group;
        bar[i].data = (gpr)(int32_t)MEM_W(SPRITE_DATA, sprite);
    }
}

extern "C" void mp_hud_group_killed(uint8_t* rdram, gpr group) {
    for (gpr& known : messages) {
        if (known == group) {
            known = 0;
        }
    }
    for (Side& known : sides) {
        if (known.group == group) {
            known.group = 0;
        }
    }
    for (BarPiece& piece : bar) {
        if (piece.group == group) {
            piece.group = 0;
        }
    }
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
    std::memset(messages, 0, sizeof(messages));
    std::memset(sides, 0, sizeof(sides));
    std::memset(bar, 0, sizeof(bar));
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

namespace {
    // Where the sprite being drawn starts in the display list, from mp_hud_sprite_start, for join_strips: 0 for none.
    gpr sprite_start = 0;

    // A display list's command, its two words.
    struct Command {
        uint32_t w0, w1;
    };

    // The most commands a sprite is drawn with that join_strips looks at: a frame of 320 by 240 a few rows at a time
    // is a few hundred.
    constexpr int SPRITE_COMMANDS = 2048;
    // One texel in s10.5, a texture rectangle's coordinates' format.
    constexpr int32_t TEXEL = 32;
    // One texel a pixel in s5.10, a texture rectangle's steps' format.
    constexpr int32_t STEP_ONE = 1 << 10;

    // A texture rectangle of a sprite's (three commands, gSPTextureRectangle): where it is (in quarter pixels), its
    // texture coordinates (s10.5) and steps (s5.10), and what it is drawn from: the texture image, the load tile's
    // settings, its own tile's, the last load (a LoadTile, or 0 words after any other) and its tile's size.
    struct Strip {
        int index;
        int tile;
        int32_t xl, yl, xh, yh;
        int32_t s, t, dsdx, dtdy;
        Command image, load_tile, own_tile, load, size;
    };

    bool same(const Command& a, const Command& b) {
        return (a.w0 == b.w0) && (a.w1 == b.w1);
    }

    // A LoadTile's or SetTileSize's first and last row (its top and bottom, 10.2) in whole texels: -1 for a part one.
    int32_t first_row(const Command& c) {
        return ((c.w0 & 0xFFF) % 4 == 0) ? (int32_t)(c.w0 & 0xFFF) / 4 : -1;
    }

    int32_t last_row(const Command& c) {
        return ((c.w1 & 0xFFF) % 4 == 0) ? (int32_t)(c.w1 & 0xFFF) / 4 : -1;
    }

    // Its left and right together.
    uint32_t columns(const Command& c) {
        return ((c.w0 >> 12) & 0xFFF) | (((c.w1 >> 12) & 0xFFF) << 12);
    }

    // c with its first and last row these.
    Command with_rows(const Command& c, int32_t first, int32_t last) {
        return {(c.w0 & ~0xFFFu) | (uint32_t)(first * 4), (c.w1 & ~0xFFFu) | (uint32_t)(last * 4)};
    }

    // Whether strip is drawn from rows of its texture a LoadTile put in TMEM, its tile those rows and its texture
    // coordinates the texture's, at least four rows, so a join can load four rows in their place.
    bool whole_rows(const Strip& strip) {
        int32_t first = first_row(strip.load);
        int32_t last = last_row(strip.load);
        return ((strip.load.w0 >> 24) == 0xF4) && (strip.image.w0 != 0) && (strip.load_tile.w0 != 0) &&
               (strip.own_tile.w0 != 0) && (first >= 0) && (last >= first + 3) && (first_row(strip.size) == first) &&
               (last_row(strip.size) == last) && (columns(strip.size) == columns(strip.load)) &&
               (strip.t == first * TEXEL);
    }

    // Where strip b goes on from strip a (the next rows of the same texture, drawn right under it as wide and as
    // magnified, a whole number of quarter pixels a texel), how tall one texel is drawn, in quarter pixels: 0 where
    // it doesn't.
    int32_t join_height(const Strip& a, const Strip& b) {
        if ((a.tile != b.tile) || (a.xl != b.xl) || (a.xh != b.xh) || (a.yh != b.yl) || (a.s != b.s) ||
            (a.dsdx != b.dsdx) || (a.dtdy != b.dtdy) || (a.dtdy <= 0) || (a.dtdy >= STEP_ONE) ||
            ((4 * STEP_ONE) % a.dtdy != 0)) {
            return 0;
        }
        int32_t height = (4 * STEP_ONE) / a.dtdy;
        // How far a's texture coordinate goes down it, in s5.10 by quarter pixels: in s10.5, 128 times less.
        int32_t drawn = (a.yh - a.yl) * a.dtdy;
        if (((a.yh - a.yl) <= 2 * height) || ((b.yh - b.yl) <= 2 * height) || (drawn % 128 != 0) ||
            (a.t + drawn / 128 != b.t) || !same(a.image, b.image) || !same(a.load_tile, b.load_tile) ||
            !same(a.own_tile, b.own_tile) || (columns(a.load) != columns(b.load)) || !whole_rows(a) ||
            !whole_rows(b) || (last_row(a.load) + 1 != first_row(b.load))) {
            return 0;
        }
        return height;
    }

    // The sprite's commands from start to *gfx with each join of its strips drawn from one texture: see the header.
    // Left as they are where no strips join, or where the commands that would replace them don't fit in their place.
    void join_strips(uint8_t* rdram, gpr gfx, gpr start) {
        gpr end = (int32_t)MEM_W(0, gfx);
        if ((start == 0) || (end <= start) || ((end - start) / 8 > SPRITE_COMMANDS)) {
            return;
        }
        int count = (end - start) / 8;
        std::vector<Command> commands(count);
        for (int i = 0; i < count; i++) {
            commands[i] = {(uint32_t)MEM_W(i * 8, start), (uint32_t)MEM_W(i * 8 + 4, start)};
        }

        // The texture rectangles, and what each is drawn from.
        std::vector<Strip> strips;
        Command image{}, tiles[8]{}, sizes[8]{}, load{};
        for (int i = 0; i < count; i++) {
            const Command& c = commands[i];
            switch (c.w0 >> 24) {
                case 0xFD:
                    image = c;
                    break;
                case 0xF5:
                    tiles[(c.w1 >> 24) & 7] = c;
                    break;
                case 0xF2:
                    sizes[(c.w1 >> 24) & 7] = c;
                    break;
                case 0xF4:
                    load = c;
                    break;
                case 0xF3:
                case 0xF0:
                    load = {};
                    break;
                case 0xE4:
                    if ((i + 2 < count) && ((commands[i + 1].w0 >> 24) == 0xE1) &&
                        ((commands[i + 2].w0 >> 24) == 0xF1)) {
                        Strip strip;
                        strip.index = i;
                        strip.tile = (c.w1 >> 24) & 7;
                        strip.xh = (c.w0 >> 12) & 0xFFF;
                        strip.yh = c.w0 & 0xFFF;
                        strip.xl = (c.w1 >> 12) & 0xFFF;
                        strip.yl = c.w1 & 0xFFF;
                        strip.s = (int16_t)(commands[i + 1].w1 >> 16);
                        strip.t = (int16_t)(commands[i + 1].w1 & 0xFFFF);
                        strip.dsdx = (int16_t)(commands[i + 2].w1 >> 16);
                        strip.dtdy = (int16_t)(commands[i + 2].w1 & 0xFFFF);
                        strip.image = image;
                        strip.load_tile = tiles[(load.w1 >> 24) & 7];
                        strip.own_tile = tiles[strip.tile];
                        strip.load = load;
                        strip.size = sizes[strip.tile];
                        strips.push_back(strip);
                        i += 2;
                    }
                    break;
            }
        }
        // Each strip's join with the next: one texel's height, 0 for none.
        std::vector<int32_t> joins(strips.size(), 0);
        bool any = false;
        for (size_t k = 0; k + 1 < strips.size(); k++) {
            joins[k] = join_height(strips[k], strips[k + 1]);
            any = any || (joins[k] != 0);
        }
        if (!any) {
            return;
        }

        // The commands again, without the syncs (RT64 has no use for them) or a texture image or tile's settings the
        // same as those already set (each strip sets them again), which makes room for the joins.
        std::vector<Command> out;
        out.reserve(count);
        Command set_image{}, set_tiles[8]{};
        size_t next = 0;
        for (int i = 0; i < count; i++) {
            const Command& c = commands[i];
            uint32_t op = c.w0 >> 24;
            if ((op == 0xE6) || (op == 0xE7) || (op == 0xE8)) {
                continue;
            }
            if (op == 0xFD) {
                if (!same(c, set_image)) {
                    out.push_back(c);
                }
                set_image = c;
                continue;
            }
            if (op == 0xF5) {
                Command& set = set_tiles[(c.w1 >> 24) & 7];
                if (!same(c, set)) {
                    out.push_back(c);
                }
                set = c;
                continue;
            }
            if ((next >= strips.size()) || (strips[next].index != i)) {
                out.push_back(c);
                continue;
            }
            // A strip, a texel short at a join above it and one below. Then its join with the next: the two rows each
            // side of it loaded into its tile, drawn a texel up and down from it.
            const Strip& strip = strips[next];
            int32_t above = (next > 0) ? joins[next - 1] : 0;
            int32_t below = joins[next];
            const Command& coords = commands[i + 1];
            out.push_back({(c.w0 & ~0xFFFu) | (uint32_t)(strip.yh - below), (c.w1 & ~0xFFFu) | (uint32_t)(strip.yl + above)});
            out.push_back({coords.w0, (coords.w1 & 0xFFFF0000u) | (uint16_t)(strip.t + ((above != 0) ? TEXEL : 0))});
            out.push_back(commands[i + 2]);
            if (below != 0) {
                const Strip& under = strips[next + 1];
                int32_t row = under.t / TEXEL;
                out.push_back(with_rows(strip.load, row - 2, row + 1));
                out.push_back(with_rows(strip.size, row - 2, row + 1));
                out.push_back({(c.w0 & ~0xFFFu) | (uint32_t)(under.yl + below), (c.w1 & ~0xFFFu) | (uint32_t)(under.yl - below)});
                out.push_back({coords.w0, (coords.w1 & 0xFFFF0000u) | (uint16_t)(under.t - TEXEL)});
                out.push_back(commands[i + 2]);
            }
            i += 2;
            next++;
        }
        if ((int)out.size() > count) {
            return;
        }
        for (size_t i = 0; i < out.size(); i++) {
            MEM_W(i * 8, start) = (int32_t)out[i].w0;
            MEM_W(i * 8 + 4, start) = (int32_t)out[i].w1;
        }
        MEM_W(0, gfx) = (int32_t)(start + (gpr)out.size() * 8);
    }
}

// A sprite drawn where it is, under a scissor of the whole window, mp_hud_sprite_reach moving its x as far as the window
// reaches past 4:3, so the game's own code doesn't cut it off at 4:3's edges.
static void reach_start(uint8_t* rdram, gpr gfx) {
    sprite_reaching = true;
    sprite_reach = reach_past_4x3();
    GfxCommand commands[5];
    gEXEnable(&commands[0]);
    scissor_to_window(&commands[1]);
    whole_screen(&commands[4]);
    emit(rdram, gfx, commands, 5);
}

extern "C" void mp_hud_sprite_start(uint8_t* rdram, gpr gfx, gpr sprite) {
    sprite_start = (int32_t)MEM_W(0, gfx);
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

    // A message's letter, drawn in the 4:3 middle under a scissor of the whole window.
    if (!picture_4x3 && is_message(rdram, sprite)) {
        reach_start(rdram, gfx);
        return;
    }

    // A piece of the bar, stretched over the window as a picture, under a scissor of the whole window. In a picture that
    // stops short of the window's edges the bar stays 4:3, with what is over it.
    OnBar with_bar = (picture_4x3 || (picture_short_half() > 0.0f)) ? OnBar::Off : on_bar(rdram, sprite);
    if (with_bar == OnBar::Piece) {
        sprite_bar = true;
        // A piece starting near the screen's left edge: from it, as much wider as it was in from it.
        int16_t x = (int16_t)MEM_H(SPRITE_X, sprite);
        if ((x > 0) && (x < BAR_GAP) && (width > 0.0f)) {
            bar_moved = sprite;
            bar_x = x;
            bar_scale = MEM_W(SPRITE_SCALE_X, sprite);
            union {
                uint32_t u;
                float f;
            } scale;
            scale.u = bar_scale;
            scale.f *= (x + width) / width;
            MEM_H(SPRITE_X, sprite) = 0;
            MEM_W(SPRITE_SCALE_X, sprite) = scale.u;
        }
        GfxCommand commands[6];
        gEXEnable(&commands[0]);
        gEXSetRectAspect(&commands[1], G_EX_ASPECT_STRETCH);
        scissor_to_window(&commands[2]);
        whole_screen(&commands[5]);
        emit(rdram, gfx, commands, 6);
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
    // A sprite of a group drawn at a side (a minigame's corner HUD): from that edge of the window, as far in from it as
    // it is from 4:3's.
    int side = (panel < 0) ? side_of(rdram, sprite) : 0;
    if (side != 0) {
        fraction = (side < 0) ? 0.0f : 1.0f;
    }
    // A sprite over the bar: the alignment as far across the window as its x is across the screen, where that x is on
    // the stretched bar.
    bool over_bar = (panel < 0) && (side == 0) && (with_bar == OnBar::Over);
    if (over_bar) {
        fraction = std::clamp((int16_t)MEM_H(SPRITE_X, sprite) / (float)SCREEN_W, 0.0f, 1.0f);
    }
    sprite_aligned = !picture_4x3 && (over_bar || (side != 0) ||
                                      ((panel >= 0) && ((panels[panel].side != 0) || (fraction != 0.5f))));
    if (!sprite_aligned) {
        // Any other sprite in the 4:3 middle (none of a panel's, a side's or the bar's) is drawn where it is, as a
        // message's letter is, and cut off at the window's edges rather than 4:3's: the coins Shell Game's start pours
        // in from the screen's top-left corner, past its left edge, into the player.
        if (!picture_4x3 && (panel < 0) && (with_bar == OnBar::Off)) {
            reach_start(rdram, gfx);
        }
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

// mp_hud_sprite_reach(gfx, x, rects): the sprite mp_hud_sprite_start began has its x, an s16, at x, and is drawn as
// texture rectangles (rects, not 0) or not.
extern "C" void mp_hud_sprite_reach(uint8_t* rdram, gpr gfx, gpr x, int32_t rects) {
    if (!sprite_reaching || (sprite_reach == 0) || (rects == 0)) {
        return;
    }
    MEM_H(0, x) = (int16_t)(MEM_H(0, x) + sprite_reach);
    sprite_moved = x;
    // A rectangle drawn at x is at the window's centre plus x plus the offset: back by as far as it was moved, from
    // the screen's left edge.
    int32_t offset = -(HALF + sprite_reach) * 4;
    GfxCommand commands[3];
    gEXEnable(&commands[0]);
    gEXSetRectAlign(&commands[1], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, offset, 0, offset, 0);
    emit(rdram, gfx, commands, 3);
}

// Where the window's left and right edges are in the x of the sprite being drawn: 0 and 320 but for one reaching past 4:3.
extern "C" float mp_hud_sprite_left(uint8_t* rdram) {
    if (!sprite_reaching) {
        return 0.0f;
    }
    return (sprite_moved != 0) ? 0.0f : (float)-sprite_reach;
}

extern "C" float mp_hud_sprite_right(uint8_t* rdram) {
    if (!sprite_reaching) {
        return (float)SCREEN_W;
    }
    return (float)(SCREEN_W + sprite_reach * ((sprite_moved != 0) ? 2 : 1));
}

extern "C" void mp_hud_sprite_end(uint8_t* rdram, gpr gfx) {
    join_strips(rdram, gfx, sprite_start);
    sprite_start = 0;
    if (sprite_bar) {
        sprite_bar = false;
        if (bar_moved != 0) {
            MEM_H(SPRITE_X, bar_moved) = bar_x;
            MEM_W(SPRITE_SCALE_X, bar_moved) = bar_scale;
            bar_moved = 0;
        }
        GfxCommand commands[6];
        gEXEnable(&commands[0]);
        gEXSetRectAspect(&commands[1], G_EX_ASPECT_AUTO);
        scissor_own(&commands[2]);
        whole_screen(&commands[5]);
        emit(rdram, gfx, commands, 6);
        return;
    }
    if (sprite_picture) {
        sprite_picture = false;
        GfxCommand commands[2];
        gEXEnable(&commands[0]);
        gEXSetRectAspect(&commands[1], G_EX_ASPECT_AUTO);
        emit(rdram, gfx, commands, 2);
        return;
    }
    if (sprite_reaching) {
        sprite_reaching = false;
        GfxCommand commands[7];
        int count = 0;
        gEXEnable(&commands[count++]);
        if (sprite_moved != 0) {
            MEM_H(0, sprite_moved) = (int16_t)(MEM_H(0, sprite_moved) - sprite_reach);
            sprite_moved = 0;
            gEXSetRectAlign(&commands[count], G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
            count += 2;
        }
        scissor_own(&commands[count]);
        count += 3;
        whole_screen(&commands[count++]);
        emit(rdram, gfx, commands, count);
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
