#include "patches.h"
#include "rt64_extended_gbi.h"
#include "board_back.h"

// A board's background past 4:3, panning with the board's 3D at the display's rate. Each game draws a board's
// background as a map of 64x48 tiles it streams in from the ROM as the camera moves, the tiles under the camera's
// 320x240 as rectangles, before the board's 3D. RT64's Aspect Ratio Expand widens the 3D but can't draw tiles the game
// never drew, and keeps a 4:3 picture's rectangles inside 4:3, so a board drew in a 4:3 box with its 3D spaces and
// characters out past it on black. A game's patch picks the tiles under a wider picture (its own code: each game streams
// them its own way) and draws them through these:
//
// - board_back_begin: a list of the tiles' own, one of a ring of them a frame that the frame's list branches to, as the
//   tiles take several times the commands the game's rectangles do, and a frame's display list buffer has the size the
//   game gives it for what it draws. A scissor to the window's edges, and black behind the tiles for past the map's own
//   edges: the game never clears a board's frame (its tiles cover 4:3), so the window's sides would show what an
//   earlier frame left there.
// - board_back_view: the 320x240 picture's projection, kept to its aspect (G_EX_ASPECT_ADJUST, so that the window's
//   sides show more of the map, not a stretched 4:3), and the scroll as a matrix. RT64 draws frames between the game's
//   at the display's rate by interpolating matrices, and never moves a rectangle, so drawn as rectangles the board
//   stood still between its frames while the 3D spaces on it moved, and slid under them in a pan: the scroll's matrix
//   is interpolated as the 3D camera is, unless the scroll jumped (a cut, not a pan).
// - board_back_quad: a tile, or part of one, as a textured quad at its place on the map.
//
// RT64 reads extended commands only after a gEXEnable in the same display list (it forgets it at each list's start).

#define SCREEN_W 320
#define SCREEN_H 240
// A scroll further than this on the map between two frames is a cut.
#define CUT_X 64
#define CUT_Y 48
#define LISTS 8
#define SCROLL_GROUP 0x4D504247

void guOrtho(Mtx* m, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);
void guMtxF2L(float mf[4][4], Mtx* m);

// A list is drawn while the frames after it are built, so there are more of them than the game's frames in flight.
static Gfx lists[LISTS][BOARD_BACK_COMMANDS];
static Vtx corners[LISTS][BOARD_BACK_CORNERS];
static Mtx scrolls[LISTS];
static s32 list_index;
static Gfx* list;
static Vtx* next_corner;
static Mtx projection;
static Vp viewport = { { { SCREEN_W * 2, SCREEN_H * 2, G_MAXZ / 2, 0 }, { SCREEN_W * 2, SCREEN_H * 2, G_MAXZ / 2, 0 } } };
// The view the frame before drew at, to tell a cut from a pan.
static f32 last_left, last_top, last_scale;
static s32 last_mirrored;

Gfx** board_back_begin(Gfx** game_gfx) {
    list = lists[list_index];
    next_corner = corners[list_index];
    gSPDisplayList((*game_gfx)++, list);

    gEXEnable(list++);
    gEXPushScissor(list++);
    gEXSetScissor(list++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_H);

    gDPPipeSync(list++);
    gEXPushOtherMode(list++);
    gEXPushFillColor(list++);
    gDPSetCycleType(list++, G_CYC_FILL);
    gDPSetRenderMode(list++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(list++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    gEXSetRectAlign(list++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, -SCREEN_W * 4, 0, SCREEN_W * 4, SCREEN_H * 4);
    gDPFillRectangle(list++, 0, 0, 0, 0);
    gEXSetRectAlign(list++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gDPPipeSync(list++);
    gEXPopFillColor(list++);
    gEXPopOtherMode(list++);
    return &list;
}

void board_back_view(Gfx** gfx, f32 left, f32 top, f32 scale, s32 mirrored) {
    Mtx* scroll = &scrolls[list_index];
    f32 m[4][4];
    s32 i, j;

    if (projection.m[0][0] == 0) {
        guOrtho(&projection, 0, SCREEN_W, SCREEN_H, 0, -1, 1, 1);
    }
    gEXPushViewport((*gfx)++);
    gSPViewport((*gfx)++, &viewport);
    gEXPushProjectionMatrix((*gfx)++);
    gEXMatrixGroup((*gfx)++, G_EX_ID_IGNORE, G_EX_INTERPOLATE_SIMPLE, G_EX_PUSH, G_MTX_PROJECTION, G_EX_COMPONENT_SKIP,
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_ASPECT_ADJUST,
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP);
    gSPMatrix((*gfx)++, &projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gEXPushGeometryMode((*gfx)++);
    gSPClearGeometryMode((*gfx)++, G_ZBUFFER | G_CULL_BOTH | G_LIGHTING | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode((*gfx)++, G_SHADE | G_SHADING_SMOOTH);
    gSPTexture((*gfx)++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);

    // The map's (x, y) at (scale * (x - left), scale * (y - top)), or mirrored, at (scale * (320 - (x - left)), ...).
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            m[i][j] = 0;
        }
    }
    m[0][0] = mirrored ? -scale : scale;
    m[1][1] = scale;
    m[2][2] = 1;
    m[3][0] = mirrored ? (scale * (SCREEN_W + left)) : (-scale * left);
    m[3][1] = -scale * top;
    m[3][3] = 1;
    guMtxF2L(m, scroll);
    // A zoom as the game animates one is a pan's few percent a frame.
    if ((mirrored == last_mirrored) && (scale <= last_scale * 1.25f) && (last_scale <= scale * 1.25f) &&
        (left - last_left <= CUT_X) && (last_left - left <= CUT_X) && (top - last_top <= CUT_Y) &&
        (last_top - top <= CUT_Y)) {
        gEXMatrixGroupSimple((*gfx)++, SCROLL_GROUP, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_INTERPOLATE,
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,
                             G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP);
    } else {
        gEXMatrixGroupNoInterpolate((*gfx)++, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    }
    last_left = left;
    last_top = top;
    last_scale = scale;
    last_mirrored = mirrored;
    gSPMatrix((*gfx)++, scroll, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
}

static void corner(Vtx* v, s32 x, s32 y, s32 s, s32 t) {
    v->v.ob[0] = x;
    v->v.ob[1] = y;
    v->v.ob[2] = 0;
    v->v.flag = 0;
    // RT64 reads a texel as a coordinate over 64 at gSPTexture's 0xFFFF.
    v->v.tc[0] = s << 6;
    v->v.tc[1] = t << 6;
    v->v.cn[0] = v->v.cn[1] = v->v.cn[2] = v->v.cn[3] = 0xFF;
}

void board_back_quad(Gfx** gfx, s32 x, s32 y, s32 width, s32 height, s32 s, s32 t) {
    Vtx* v = next_corner;

    if (v + 4 > corners[list_index] + BOARD_BACK_CORNERS) {
        return;
    }
    next_corner += 4;
    corner(&v[0], x, y, s, t);
    corner(&v[1], x + width, y, s + width, t);
    corner(&v[2], x + width, y + height, s + width, t + height);
    corner(&v[3], x, y + height, s, t + height);
    gSPVertex((*gfx)++, v, 4, 0);
    gSP2Triangles((*gfx)++, 0, 1, 2, 0, 0, 2, 3, 0);
}

void board_back_end(Gfx** gfx) {
    gSPPopMatrix((*gfx)++, G_MTX_MODELVIEW);
    gEXPopMatrixGroup((*gfx)++, G_MTX_MODELVIEW);
    gEXPopMatrixGroup((*gfx)++, G_MTX_PROJECTION);
    gEXPopGeometryMode((*gfx)++);
    gEXPopProjectionMatrix((*gfx)++);
    gEXPopViewport((*gfx)++);
    gEXPopScissor((*gfx)++);
    gSPEndDisplayList((*gfx)++);
    list_index = (list_index + 1) % LISTS;
}
