#include "patches.h"
#include "rt64_extended_gbi.h"
#include "camera_view.h"
#include "board_back.h"

// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges, its 3D moving with the board's background under it.
//
// - The scissor. Each game sets a camera's scissor before its view is drawn, and RT64 keeps a scissor that stops short
//   of 4:3's edges as far in from the window's, so a board, whose camera keeps the console's overscan border (16, 12,
//   304, 228), drew in a box with black at its sides. A camera whose scissor is the whole screen (its right edge at
//   320, or at 319 as some scenes give it) or that border is drawn to the window's edges (gEXSetScissor, from the left
//   and right): from top to bottom for the border, and between the game's own top and bottom while it animates them, as
//   Mario Party 2's board wipe between turns closes it to a band across the middle. Mario Party 3's wipes it sideways
//   instead, 32 pixels a frame: the left edge goes across to close it, then the right edge to open it again after the
//   camera cuts to the next player. A scissor on those steps with one edge at the border's is widened too, each edge as
//   far across the window as across the border (an origin of that fraction of the window's width), so its 3D keeps its
//   projection and the wipe crosses the window as it crosses the console's screen. RT64 draws a projection's 3D on a
//   viewport as wide as the window only while the scissors of all it draws under that projection, together, span the
//   frame's width, so under a sideways wipe's scissor alone the board's spaces and players were squeezed into 4:3 (to
//   three quarters of their width at 16:9) from the wipe's first step to its last. A triangle of no area, which draws
//   nothing, goes under the open border's scissor before the wipe's is set: the 3D keeps the window's viewport and is
//   still cut at the wipe's edge by its own scissor. RT64 counts a projection or viewport put back as a new one, so
//   after a board's map, which Mario Party 3 draws once the camera's projection and scissor are set and before the
//   characters and the things standing on the board, with a projection and viewport of its own (board_back.c), and
//   then loads the camera's projection again for them without setting its scissor, those were squeezed into 4:3
//   still: the triangle goes again under every widened camera's projection while the last scissor is a sideways
//   wipe's, and after board_back_end has put the camera's back (camera_view_span). A screen split between players (a
//   minigame's halves and quarters, at 160) keeps the game's, and so does every camera while the picture is 4:3
//   (board_back.c: a board's opening scene, whose map is no wider than 4:3, or a scene the game draws at 4:3), so that
//   its 3D, the scene's host and the players among them, keeps inside 4:3 with its background, not out over the black
//   at the window's sides. A scene drawn no wider than an aspect past 4:3 (board_back_scene_widest: a stage whose 3D
//   ends short of a wide window's edges) has the scissor that would reach the window's edges stop that far from the
//   screen's middle (an origin at the window's centre), so its 3D ends there, with black past it. A scissor that closes
//   the border, as a wipe does, is kept for board_back.c (camera_view_wipe): on a console the game draws a board's
//   background under it too, and the wipe takes the board's map away with its 3D.
// - The viewport. A board pans its 3D by moving its camera's viewport, as large as the map, by as much as it scrolls
//   the background, and RT64 widens a viewport about the screen's middle: what the viewport moved by was widened too,
//   so a pan moved the board's spaces and characters a third further than the board under them, and a viewport is
//   never interpolated, so between the game's frames the 3D stood still while the background moved. A widened camera
//   has its viewport kept in the middle here, and what it was moved by goes into its projection (an off-centre
//   projection draws the same picture), which RT64 widens as it widens a 3D view (G_EX_ASPECT_ADJUST, on the
//   projection's group without a group of its own) and interpolates. Any other camera's projection goes back to RT64's
//   own choice (G_EX_ASPECT_AUTO), which keeps the 3D of a view that covers less than the frame at its own aspect. A
//   jump of the viewport, as where the board's camera cuts to another place, isn't interpolated, as the background's
//   scroll isn't (board_back.c).
//
// RT64 reads extended commands only after a gEXEnable in the same display list (it forgets it at each list's start),
// and reads one before as the microcode's own command.

#define SCREEN_W 320
// The whole screen's right edge as some scenes give it, at the last pixel and not past it: Mario Party's Bowl Over,
// Tightrope Treachery, Tug o' War and Chance Time set a camera's scissor to (0, 0, 319, 239), as do Mario Party 2's
// Bowl Over, Chance Time and Rainbow Run and Mario Party 3's Fridged Bridges. The console leaves the screen's last
// column undrawn; RT64 kept such a camera's view inside 4:3, beside views of the whole screen that reach the window's
// edges.
#define SCREEN_LAST (SCREEN_W - 1)
#define SCREEN_H 240
#define WIDEST_4X3 (4.0f / 3.0f)
#define BORDER_LEFT 16
#define BORDER_TOP 12
#define BORDER_RIGHT 304
#define BORDER_BOTTOM 228
// The step of a sideways wipe's edge across the border, from one edge to the other in 9.
#define WIPE_STEP 32
// The projections and viewports of the widened cameras, more than the frames in flight use.
#define SHIFTED 16
// The cameras whose last viewport is kept, to tell a cut from a pan.
#define CAMERAS 16
// A viewport moved further than this between two frames, in its quarter pixels, is a cut.
#define CUT_X (64 * 4)
#define CUT_Y (48 * 4)

void guMtxL2F(float mf[4][4], Mtx* m);
void guMtxF2L(float mf[4][4], Mtx* m);

static Mtx shifted_projections[SHIFTED];
static Vp centred_viewports[SHIFTED];
static s32 shifted_index;
static Vp last_viewports[CAMERAS];
// The last scissor while it closes the border, as gEXSetScissor takes it: its left and right edges' origins, and its
// edges (left, top, right, bottom) in pixels, the left and right from their origins.
static s32 wiping;
static u16 wipe_origins[2];
static s32 wipe_edges[4];
// Whether the last camera's scissor is a sideways wipe's, with a triangle of no area under the open border, and that
// border as span_width takes it.
static s32 spanning;
static u16 span_origins[2];
static s32 span_edges[2];
// The triangle of no area under a sideways wipe's open border: its one corner in front of the camera, on the identity.
static Mtx identity = { { { 0x00010000, 0, 0x00000001, 0 }, { 0, 0x00010000, 0, 0x00000001 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } } };
static Vtx in_front = { { { 0, 0, -1 }, 0, { 0, 0 }, { 0, 0, 0, 0 } } };

// Whether the viewport moved or zoomed further than a pan does since the camera's last frame.
static s32 cut(s32 camera, Vp* viewport) {
    Vp* last;
    s32 dx, dy, scale, last_scale, jumped;

    if ((camera < 0) || (camera >= CAMERAS)) {
        return FALSE;
    }
    last = &last_viewports[camera];
    dx = viewport->vp.vtrans[0] - last->vp.vtrans[0];
    dy = viewport->vp.vtrans[1] - last->vp.vtrans[1];
    scale = viewport->vp.vscale[0];
    last_scale = last->vp.vscale[0];
    jumped = (dx > CUT_X) || (-dx > CUT_X) || (dy > CUT_Y) || (-dy > CUT_Y) || (scale * 4 > last_scale * 5) ||
             (last_scale * 4 > scale * 5);
    *last = *viewport;
    return jumped;
}

// Whether an edge is on a sideways wipe's steps across the border.
static s32 wipe_step(s32 x) {
    return (x >= BORDER_LEFT) && (x <= BORDER_RIGHT) && (((x - BORDER_LEFT) % WIPE_STEP) == 0);
}

// Whether the scissor is the border, or the border in a sideways wipe: one edge at the border's, the other on its steps.
static s32 border_wipe(s32 left, s32 right) {
    return wipe_step(left) && wipe_step(right) && (left <= right) && ((left == BORDER_LEFT) || (right == BORDER_RIGHT));
}

// An edge across the border as the origin as far across the window.
static u16 border_origin(s32 x) {
    return (u16)(((x - BORDER_LEFT) * G_EX_ORIGIN_RIGHT) / (BORDER_RIGHT - BORDER_LEFT));
}

s32 camera_view_widened(s32 left, s32 right) {
    return ((left == 0) && ((right == SCREEN_W) || (right == SCREEN_LAST))) || border_wipe(left, right);
}

s32 camera_view_wipe(u16 origins[2], s32 edges[4]) {
    s32 i, was = wiping;

    wiping = FALSE;
    for (i = 0; i < 2; i++) {
        origins[i] = wipe_origins[i];
    }
    for (i = 0; i < 4; i++) {
        edges[i] = wipe_edges[i];
    }
    return was;
}

// The scissor as camera_view_wipe gives it, if it closes the border: inside it and short of it on a side, sideways or
// to a band.
static void keep_wipe(s32 left, s32 top, s32 right, s32 bottom, u16 left_origin, s32 left_x, u16 right_origin,
                      s32 right_x, s32 wipe_top, s32 wipe_bottom) {
    wiping = (left >= BORDER_LEFT) && (top >= BORDER_TOP) && (right <= BORDER_RIGHT) && (bottom <= BORDER_BOTTOM) &&
             ((left > BORDER_LEFT) || (top > BORDER_TOP) || (right < BORDER_RIGHT) || (bottom < BORDER_BOTTOM));
    wipe_origins[0] = left_origin;
    wipe_origins[1] = right_origin;
    wipe_edges[0] = left_x;
    wipe_edges[1] = wipe_top;
    wipe_edges[2] = right_x;
    wipe_edges[3] = wipe_bottom;
}

// A triangle of no area under the scissor the border has open (its left and right edges' origins and offsets), so that
// the scissors of the projection it is drawn under span the frame's width. The scissor is put back after it.
static void span_width(Gfx** gfx, u16 left_origin, s32 left_x, u16 right_origin, s32 right_x) {
    spanning = TRUE;
    span_origins[0] = left_origin;
    span_origins[1] = right_origin;
    span_edges[0] = left_x;
    span_edges[1] = right_x;
    gEXPushScissor((*gfx)++);
    gEXSetScissor((*gfx)++, G_SC_NON_INTERLACE, left_origin, right_origin, left_x, 0, right_x, SCREEN_H);
    gDPPipeSync((*gfx)++);
    gEXPushOtherMode((*gfx)++);
    gEXPushGeometryMode((*gfx)++);
    gDPSetCycleType((*gfx)++, G_CYC_1CYCLE);
    gSPClearGeometryMode((*gfx)++, G_CULL_BOTH);
    gEXMatrixGroupNoInterpolate((*gfx)++, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gSPMatrix((*gfx)++, &identity, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
    gSPVertex((*gfx)++, &in_front, 1, 0);
    gSP1Triangle((*gfx)++, 0, 0, 0, 0);
    gSPPopMatrix((*gfx)++, G_MTX_MODELVIEW);
    gEXPopMatrixGroup((*gfx)++, G_MTX_MODELVIEW);
    gDPPipeSync((*gfx)++);
    gEXPopGeometryMode((*gfx)++);
    gEXPopOtherMode((*gfx)++);
    gEXPopScissor((*gfx)++);
}

void camera_view_scissor(Gfx** gfx, s32 left, s32 top, s32 right, s32 bottom) {
    u16 left_origin = G_EX_ORIGIN_LEFT;
    u16 right_origin = G_EX_ORIGIN_RIGHT;
    f32 widest = board_back_widest();
    s32 game_top = top, game_bottom = bottom;
    s32 sideways = (left > BORDER_LEFT) || ((left != 0) && (right < BORDER_RIGHT));

    spanning = FALSE;
    // A closed wipe draws nothing, as the game's own scissor does.
    if (camera_view_widened(left, right) && (left < right) && !board_back_4x3()) {
        if ((top == BORDER_TOP) && (bottom == BORDER_BOTTOM)) {
            top = 0;
            bottom = SCREEN_H;
        }
        if (left != 0) {
            left_origin = border_origin(left);
            right_origin = border_origin(right);
        }
        gEXEnable((*gfx)++);
        if (widest > WIDEST_4X3) {
            // Each edge as far across the picture's width (SCREEN_H * widest) from its middle, the window's centre.
            f32 width = SCREEN_H * widest;
            s32 left_x = (s32)(width * ((f32)left_origin / G_EX_ORIGIN_RIGHT - 0.5f));
            s32 right_x = (s32)(width * ((f32)right_origin / G_EX_ORIGIN_RIGHT - 0.5f));
            if (sideways) {
                span_width(gfx, G_EX_ORIGIN_CENTER, (s32)(width * -0.5f), G_EX_ORIGIN_CENTER, (s32)(width * 0.5f));
            }
            gEXSetScissor((*gfx)++, G_SC_NON_INTERLACE, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, left_x, top, right_x,
                          bottom);
            keep_wipe(left, game_top, right, game_bottom, G_EX_ORIGIN_CENTER, left_x, G_EX_ORIGIN_CENTER, right_x, top,
                      bottom);
            return;
        }
        if (sideways) {
            span_width(gfx, G_EX_ORIGIN_LEFT, 0, G_EX_ORIGIN_RIGHT, 0);
        }
        gEXSetScissor((*gfx)++, G_SC_NON_INTERLACE, left_origin, right_origin, 0, top, 0, bottom);
        keep_wipe(left, game_top, right, game_bottom, left_origin, 0, right_origin, 0, top, bottom);
        return;
    }

    gDPSetScissor((*gfx)++, G_SC_NON_INTERLACE, left, top, right, bottom);
    keep_wipe(left, top, right, bottom, G_EX_ORIGIN_NONE, left, G_EX_ORIGIN_NONE, right, top, bottom);
}

void camera_view_span(Gfx** gfx) {
    if (spanning) {
        span_width(gfx, span_origins[0], span_edges[0], span_origins[1], span_edges[1]);
    }
}

void camera_view_projection(Gfx** gfx, s32 camera, Vp* viewport, u16 persp_norm, Mtx* projection, s32 widened) {
    u8 aspect = G_EX_ASPECT_AUTO;
    u8 interpolate = G_EX_COMPONENT_AUTO;

    // The viewport in the middle and what it was moved by in the projection: (x, y) at the viewport's middle plus its
    // scale times the projection's (x, y) / w, the y down.
    if (widened && (viewport->vp.vscale[0] != 0) && (viewport->vp.vscale[1] != 0)) {
        Vp* centred = &centred_viewports[shifted_index];
        Mtx* shifted = &shifted_projections[shifted_index];
        f32 dx = (f32)(viewport->vp.vtrans[0] - SCREEN_W * 2) / viewport->vp.vscale[0];
        f32 dy = -(f32)(viewport->vp.vtrans[1] - SCREEN_H * 2) / viewport->vp.vscale[1];
        f32 m[4][4];
        s32 i;

        shifted_index = (shifted_index + 1) % SHIFTED;
        if (cut(camera, viewport)) {
            interpolate = G_EX_COMPONENT_SKIP;
        }
        *centred = *viewport;
        centred->vp.vtrans[0] = SCREEN_W * 2;
        centred->vp.vtrans[1] = SCREEN_H * 2;
        guMtxL2F(m, projection);
        for (i = 0; i < 4; i++) {
            m[i][0] += dx * m[i][3];
            m[i][1] += dy * m[i][3];
        }
        guMtxF2L(m, shifted);
        viewport = centred;
        projection = shifted;
        aspect = G_EX_ASPECT_ADJUST;
    }

    gSPViewport((*gfx)++, viewport);
    gSPPerspNormalize((*gfx)++, persp_norm);
    gEXEnable((*gfx)++);
    gEXMatrixGroup((*gfx)++, G_EX_ID_AUTO, G_EX_INTERPOLATE_DECOMPOSE, G_EX_NOPUSH, G_MTX_PROJECTION, interpolate,
                   interpolate, interpolate, interpolate, interpolate, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO,
                   G_EX_ORDER_AUTO, G_EX_EDIT_NONE, aspect, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    gSPMatrix((*gfx)++, osVirtualToPhysical(projection), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    // Mario Party 3 draws a board's characters during a sideways wipe under the camera's projection loaded again, with
    // the scissor the wipe's from before.
    if (widened) {
        camera_view_span(gfx);
    }
}
