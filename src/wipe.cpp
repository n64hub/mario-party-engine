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
//   window's sides. The rectangles take the wipe's colour and alpha, so a face's wipe that fades fades its sides with
//   it. mp_wipe_square_sides there, given the square's left and right edges, draws a square wider than 4:3 (below)
//   past it too.
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
// A shape wipe in a 4:3 picture is all inside 4:3 too. Its rectangles above and below the square run from x 0 to 320,
// as wide as every scissor of such a frame, and so does its square at its largest, clamped to the screen, so RT64
// stretched them across the window and left the rectangles beside the square inside 4:3: Mario Party 1's white star
// wipe on a board's opening scene drew white out to the window's edges above and below the star and the picture's
// black bars beside it. mp_wipe_shape_start and mp_wipe_shape_end, around the shape wipe's draws, set their aspect to
// the screen's (G_EX_ASPECT_ADJUST) in a 4:3 picture.
//
// A square wider than 4:3 in a wider picture (Mario Party 1's circle wipe starts at twice the screen's width, its
// circle past the window's corners) the game clamps to the screen at 0, and as wide as the screen RT64 stretched it
// across the window, a shape too wide for the colour around it, which mp_wipe_sides drew over the circle at the
// window's sides: the board cut to 4:3 for the circle's first frame. mp_wipe_square_sides turns the game's texture
// rectangle into no-ops and draws the whole square instead, aligned to the screen's centre under the wide scissor, its
// texture where the game's would have it, and the colour only past the square.
//
// A 4:3 picture's sides are black whatever its scene draws: mp_wipe_picture_sides, where a game ends the last display
// list of its frame (after its wipe and its text), given the list's next command, fills the window black from past its
// left edge to 4:3's and from 4:3's right edge to past its right edge, two fill rectangles of the window's sides alone
// (RT64 draws a fill rectangle as a clear of its area, which no scissor cuts), and returns the list's next command
// after them. RT64 widens the 3D of a camera whose scissor is the whole screen, and draws a rectangle that samples a
// copy of the frame (a framebuffer's tile) across the window, so a scene the game draws at 4:3 drew past 4:3 over the
// black: Mario Party 2's Crazy Cutters its logs and spiked bars, and Mario Party 3's results on the star stage the
// stage's 3D. A picture drawn no wider than an aspect past 4:3 in a window wider still (picture_short_half) is black
// past its own edges the same way, over what its scene, its wipes and its pictures as wide as the screen draw there,
// which are drawn as a wide picture's are.
//
// In a 4:3 window the rectangles cover nothing past the screen. RT64 reads extended commands only after a gEXEnable in
// the same display list, so each start sends one.

#include <cmath>

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
    // gbi.h's (F3DEX2's) commands and other modes the black at a 4:3 picture's sides is drawn with.
    constexpr uint32_t G_RDPPIPESYNC = 0xE7;
    constexpr uint32_t G_SETOTHERMODE_H = 0xE3;
    constexpr uint32_t G_SETOTHERMODE_L = 0xE2;
    constexpr uint32_t G_SETFILLCOLOR = 0xF7;
    constexpr uint32_t CYCLE_TYPE_SHIFT = 20;
    constexpr uint32_t CYCLE_FILL = 3u << CYCLE_TYPE_SHIFT;
    constexpr uint32_t RENDER_MODE_SHIFT = 3;

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
    // Whether mp_wipe_shape_start set the screen's aspect, for mp_wipe_shape_end.
    bool shape_adjusted = false;

    // gbi.h's commands a shape wipe draws its square with, and how far back from the colour's rectangles they are
    // looked for.
    constexpr uint32_t G_NOOP = 0x00;
    constexpr uint32_t G_TEXRECT = 0xE4;
    constexpr uint32_t G_RDPHALF_1 = 0xE1;
    constexpr uint32_t G_RDPHALF_2 = 0xF1;
    constexpr uint32_t G_SETCOMBINE = 0xFC;
    constexpr int LOOK_BACK = 32;

    // The whole square from left (negative) to right in place of the game's texture rectangle, which is the square
    // clamped to the screen from x 0, found back from *gfx with the combine it was drawn with (the one before the
    // colour's) and made no-ops. The texture is at s at x 0 (S10.5, a step of x dsdx of S5.10's 1024 a texel), so at
    // left it is left * dsdx / 32 before it. Seven commands, or none if they aren't there.
    int whole_square(uint8_t* rdram, gpr gfx, int32_t left, int32_t right, GfxCommand* commands) {
        gpr at = (int32_t)MEM_W(0, gfx);
        gpr rect = 0;
        gpr combine = 0;
        gpr colour_combine = 0;
        for (int i = 1; (i <= LOOK_BACK) && (combine == 0); i++) {
            gpr command = at - 8 * i;
            uint32_t op = (uint32_t)MEM_W(0, command) >> 24;
            if ((op == G_SETCOMBINE) && (colour_combine == 0)) {
                colour_combine = command;
            }
            else if ((op == G_SETCOMBINE) && (rect != 0)) {
                combine = command;
            }
            else if ((op == G_TEXRECT) && (colour_combine != 0) && (rect == 0)) {
                rect = command;
            }
        }
        if ((combine == 0) || (((uint32_t)MEM_W(8, rect) >> 24) != G_RDPHALF_1) ||
            (((uint32_t)MEM_W(16, rect) >> 24) != G_RDPHALF_2)) {
            return 0;
        }

        uint32_t rect0 = (uint32_t)MEM_W(0, rect);
        uint32_t rect1 = (uint32_t)MEM_W(4, rect);
        uint32_t coords = (uint32_t)MEM_W(12, rect);
        uint32_t steps = (uint32_t)MEM_W(20, rect);
        int32_t s = (int16_t)(coords >> 16) + left * (int16_t)(steps >> 16) / 32;
        int32_t t = (int16_t)(coords & 0xFFFF);
        for (int i = 0; i < 3; i++) {
            MEM_W(8 * i, rect) = (int32_t)(G_NOOP << 24);
            MEM_W(8 * i + 4, rect) = 0;
        }

        commands[0].values.word0 = (uint32_t)MEM_W(0, combine);
        commands[0].values.word1 = (uint32_t)MEM_W(4, combine);
        // From 0 to its width, placed from the screen's centre at left.
        gEXSetRectAlign(&commands[1], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, (left - HALF) * 4, 0, (left - HALF) * 4,
                        0);
        commands[3].values.word0 = (G_TEXRECT << 24) | ((uint32_t)((right - left) * 4) << 12) | (rect0 & 0xFFF);
        commands[3].values.word1 = rect1 & 0x07000FFF;
        commands[4].values.word0 = G_RDPHALF_1 << 24;
        commands[4].values.word1 = ((uint32_t)(s & 0xFFFF) << 16) | (uint32_t)(t & 0xFFFF);
        commands[5].values.word0 = G_RDPHALF_2 << 24;
        commands[5].values.word1 = steps;
        // Back to the colour's.
        commands[6].values.word0 = (uint32_t)MEM_W(0, colour_combine);
        commands[6].values.word1 = (uint32_t)MEM_W(4, colour_combine);
        return 7;
    }

    // The colour from WIDE left of 4:3 to the square's left edge and from its right edge to WIDE right of 4:3, and a
    // square that reaches past 4:3 drawn whole.
    void sides(uint8_t* rdram, gpr gfx, int32_t left, int32_t right) {
        if (picture_is_4x3()) {
            return;
        }

        GfxCommand commands[20];
        gEXEnable(&commands[0]);
        push_wide_scissor(&commands[1]);
        int count = 4;
        int past = (left < 0) ? whole_square(rdram, gfx, left, right, &commands[count]) : 0;
        count += past;
        if (past == 0) {
            // From WIDE left of 4:3's left edge to it.
            gEXSetRectAlign(&commands[count], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_NONE, (-WIDE - HALF) * 4, 0, 0, 0);
            fill(&commands[count + 2], 0, 0);
            // From 4:3's right edge to WIDE right of it.
            gEXSetRectAlign(&commands[count + 3], G_EX_ORIGIN_NONE, G_EX_ORIGIN_CENTER, 0, 0, (WIDE - HALF) * 4, 0);
            fill(&commands[count + 5], SCREEN_W, SCREEN_W);
        }
        else {
            // The same from the square's edges, placed from the screen's centre as the square is.
            gEXSetRectAlign(&commands[count], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, (-WIDE - HALF) * 4, 0,
                            (left - HALF) * 4, 0);
            fill(&commands[count + 2], 0, 0);
            gEXSetRectAlign(&commands[count + 3], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER,
                            (right - SCREEN_W - HALF) * 4, 0, (WIDE - HALF) * 4, 0);
            fill(&commands[count + 5], SCREEN_W, SCREEN_W);
        }
        count += 6;
        rect_own(&commands[count]);
        gEXPopScissor(&commands[count + 2]);
        emit(rdram, gfx, commands, count + 3);
    }
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

extern "C" void mp_wipe_shape_start(uint8_t* rdram, gpr gfx) {
    shape_adjusted = picture_is_4x3();
    if (!shape_adjusted) {
        return;
    }

    GfxCommand commands[2];
    gEXEnable(&commands[0]);
    gEXSetRectAspect(&commands[1], G_EX_ASPECT_ADJUST);
    emit(rdram, gfx, commands, 2);
}

extern "C" void mp_wipe_shape_end(uint8_t* rdram, gpr gfx) {
    if (!shape_adjusted) {
        return;
    }
    shape_adjusted = false;

    GfxCommand command;
    gEXSetRectAspect(&command, G_EX_ASPECT_AUTO);
    emit(rdram, gfx, &command, 1);
}

extern "C" void mp_wipe_sides(uint8_t* rdram, gpr gfx) {
    sides(rdram, gfx, 0, SCREEN_W);
}

extern "C" void mp_wipe_square_sides(uint8_t* rdram, gpr gfx, gpr left, gpr right) {
    sides(rdram, gfx, (int16_t)left, (int16_t)right);
}

extern "C" gpr mp_wipe_picture_sides(uint8_t* rdram, gpr at) {
    // How far the picture reaches each side of the screen's middle, in the screen's pixels and in quarter pixels.
    float half = picture_is_4x3() ? HALF : picture_short_half();
    if (half <= 0.0f) {
        return at;
    }
    int32_t half4 = (int32_t)std::lround(half * 4.0f);

    GfxCommand commands[22];
    gEXEnable(&commands[0]);
    commands[1].values.word0 = G_RDPPIPESYNC << 24;
    commands[1].values.word1 = 0;
    gEXPushOtherMode(&commands[2]);
    gEXPushFillColor(&commands[3]);
    push_wide_scissor(&commands[4]);
    // Fill mode (gDPSetCycleType), no blending (gDPSetRenderMode), and black of a 16-bit frame (gDPSetFillColor).
    commands[7].values.word0 = (G_SETOTHERMODE_H << 24) | ((32 - CYCLE_TYPE_SHIFT - 2) << 8) | (2 - 1);
    commands[7].values.word1 = CYCLE_FILL;
    commands[8].values.word0 = (G_SETOTHERMODE_L << 24) | ((32 - RENDER_MODE_SHIFT - 29) << 8) | (29 - 1);
    commands[8].values.word1 = 0;
    commands[9].values.word0 = G_SETFILLCOLOR << 24;
    commands[9].values.word1 = 0x00010001;
    // From WIDE left of 4:3's left edge to the picture's: a fill rectangle reaches 3/4 of a pixel past its right edge
    // (RT64 draws it as a clear of its own area, under no scissor), taken off here. RT64 rounds an aligned rectangle's
    // edges down to the game's pixels in the window, which put this one's right edge a pixel short of the picture's: it
    // ends a pixel on.
    gEXSetRectAlign(&commands[10], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, (-WIDE - HALF) * 4, 0, 4 - half4 - 3, 0);
    fill(&commands[12], 0, 0);
    // From the picture's right edge to WIDE right of 4:3's, its left edge a pixel on for the same reason, which put it
    // a pixel inside the picture.
    gEXSetRectAlign(&commands[13], G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, half4 + (1 - SCREEN_W) * 4, 0,
                    (WIDE - HALF) * 4, 0);
    fill(&commands[15], SCREEN_W, SCREEN_W);
    rect_own(&commands[16]);
    commands[18].values.word0 = G_RDPPIPESYNC << 24;
    commands[18].values.word1 = 0;
    gEXPopScissor(&commands[19]);
    gEXPopFillColor(&commands[20]);
    gEXPopOtherMode(&commands[21]);
    for (const GfxCommand& command : commands) {
        MEM_W(0, at) = command.values.word0;
        MEM_W(4, at) = command.values.word1;
        at += 8;
    }
    return at;
}
