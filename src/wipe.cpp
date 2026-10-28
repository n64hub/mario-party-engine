// A wipe past 4:3: in a picture wider than 4:3, the black (or the wipe's colour) of every game's screen wipe drawn out
// to the window's left and right edges, where the board or minigame beside 4:3 kept showing through a closing wipe.
//
// Each game's wipe is Hudson's one module (in Mario Party 3 WipeExecAlways and its kin), the decompilations' asm. A
// shape wipe (an iris: a circle, a star, a face) draws the shape as a mask texture in a square centred on the screen,
// then the wipe's colour as up to four fill rectangles around the square: one above and one below it from x 0 to 320,
// one each side of it. A fade, the held colour after a wipe closes, and a shape wipe's first frame are one fill
// rectangle of the whole screen. RT64's Aspect Ratio Expand keeps those rectangles inside 4:3 where the frame's other
// draws reach past it (a board's or a minigame's 3D, widened), so the game's colour never reaches past 4:3 there; a
// rectangle as wide as every scissor of the frame (RT64's framebuffer pair's) it stretches to the window's width
// instead. A game's hooks (its us.toml) call these natives:
// - mp_wipe_fill_start and mp_wipe_fill_end around the function that draws the whole-screen rectangle: that rectangle
//   carried out past the window's edges.
// - mp_wipe_sides where a shape wipe has set its colour for the rectangles around the square: two more of them, from
//   past the window's left edge to 4:3's and from 4:3's right edge to past the window's, so the colour fills the
//   window's sides. A square wider than 4:3 (an iris's first frames, a face grown past the screen) is drawn only
//   inside 4:3, as the game clamps it at 0, and the window's sides take the wipe's colour there too: an iris starts 16
//   pixels past 4:3 at most. The rectangles take the wipe's colour and alpha, so a face's wipe that fades fades its
//   sides with it.
//
// Each aligns its rectangles to the screen's centre (gEXSetRectAlign) and draws them under a scissor of its own as wide
// (gEXSetScissor, pushed and popped around them), WIDE past 4:3 on each side. Not to the window's edges: RT64 places
// what is aligned to the window's left or right edge at its HUD's aspect (the runtime's HUD ratio, 16:9 by default),
// short of a wider window's edges, and a scissor's alignment (gEXSetScissorAlign) can't reach left of 0.
//
// A frame whose picture is 4:3 (a board's opening scene, its map no wider than 4:3, with black at the window's sides:
// patches/board_back.c) keeps its wipes inside 4:3 with it. Its draws keep to the screen's scissor, so RT64 would
// stretch the whole-screen rectangle across the window: mp_wipe_fill_start sets its aspect to the screen's
// (G_EX_ASPECT_ADJUST) instead, which puts it on the 4:3 picture where RT64 puts the game's other rectangles
// (aligned to the screen's centre, its edges would be rounded down to the game's pixels, up to 4 of the window's
// pixels left of the picture), and mp_wipe_sides adds nothing. Mario Party 3's opening scene fades in from white,
// and the window's black sides turned grey with it.
//
// In a 4:3 window the rectangles cover nothing past the screen. RT64 reads extended commands only after a gEXEnable in
// the same display list, so each start sends one.

#include "display_list.h"

using namespace mp_display_list;

namespace {
    // How far past 4:3 the wipe's colour reaches on each side, in the 320x240 screen's pixels: a window up to 12 times
    // as wide as it is tall.
    constexpr int32_t WIDE = SCREEN_W * 4;
    // A rectangle aligned to the screen's centre is placed from it, half the screen from its left edge.
    constexpr int32_t HALF = SCREEN_W / 2;
    // gbi.h's G_SC_NON_INTERLACE, a scissor's mode.
    constexpr uint32_t NON_INTERLACE = 0;

    // gDPFillRectangle from (ulx, 0) to (lrx, 240), x in pixels.
    void fill(GfxCommand* command, int32_t ulx, int32_t lrx) {
        command->values.word0 = (0xF6u << 24) | ((uint32_t)(lrx * 4) << 12) | (SCREEN_H * 4);
        command->values.word1 = ((uint32_t)(ulx * 4) << 12);
    }

    // gEXPushScissor, then a scissor from WIDE left of 4:3 to WIDE right of it. Three commands.
    void push_wide_scissor(GfxCommand* commands) {
        gEXPushScissor(&commands[0]);
        gEXSetScissor(&commands[1], NON_INTERLACE, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, -WIDE - HALF, 0, WIDE + HALF,
                      SCREEN_H);
    }

    // gEXSetRectAlign of RT64's own. Two commands.
    void rect_own(GfxCommand* commands) {
        gEXSetRectAlign(commands, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    }

    // Whether mp_wipe_fill_start pushed a scissor, for mp_wipe_fill_end.
    bool fill_wide = false;
}

extern "C" void mp_wipe_fill_start(uint8_t* rdram, gpr gfx) {
    fill_wide = !picture_is_4x3();
    if (!fill_wide) {
        GfxCommand commands[2];
        gEXEnable(&commands[0]);
        gEXSetRectAspect(&commands[1], G_EX_ASPECT_ADJUST);
        emit(rdram, gfx, commands, 2);
        return;
    }

    GfxCommand commands[6];
    gEXEnable(&commands[0]);
    push_wide_scissor(&commands[1]);
    // The game's rectangle from 0 to 320: its left edge WIDE left of 4:3's, its right edge WIDE right of it.
    gEXSetRectAlign(&commands[4], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, (-WIDE - HALF) * 4, 0, (WIDE - HALF) * 4, 0);
    emit(rdram, gfx, commands, 6);
}

extern "C" void mp_wipe_fill_end(uint8_t* rdram, gpr gfx) {
    if (!fill_wide) {
        GfxCommand command;
        gEXSetRectAspect(&command, G_EX_ASPECT_AUTO);
        emit(rdram, gfx, &command, 1);
        return;
    }
    fill_wide = false;

    GfxCommand commands[3];
    rect_own(&commands[0]);
    gEXPopScissor(&commands[2]);
    emit(rdram, gfx, commands, 3);
}

extern "C" void mp_wipe_sides(uint8_t* rdram, gpr gfx) {
    if (picture_is_4x3()) {
        return;
    }

    GfxCommand commands[13];
    gEXEnable(&commands[0]);
    push_wide_scissor(&commands[1]);
    // From WIDE left of 4:3's left edge to it.
    gEXSetRectAlign(&commands[4], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_NONE, (-WIDE - HALF) * 4, 0, 0, 0);
    fill(&commands[6], 0, 0);
    // From 4:3's right edge to WIDE right of it.
    gEXSetRectAlign(&commands[7], G_EX_ORIGIN_NONE, G_EX_ORIGIN_CENTER, 0, 0, (WIDE - HALF) * 4, 0);
    fill(&commands[9], SCREEN_W, SCREEN_W);
    rect_own(&commands[10]);
    gEXPopScissor(&commands[12]);
    emit(rdram, gfx, commands, 13);
}
