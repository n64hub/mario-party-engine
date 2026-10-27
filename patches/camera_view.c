#include "patches.h"
#include "rt64_extended_gbi.h"
#include "camera_view.h"
#include "board_back.h"

// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges, its 3D moving with the board's background under it.
//
// - The scissor. Each game sets a camera's scissor before its view is drawn, and RT64 keeps a scissor that stops short
//   of 4:3's edges as far in from the window's, so a board, whose camera keeps the console's overscan border (16, 12,
//   304, 228), drew in a box with black at its sides. A camera whose scissor is the whole screen or that border is
//   drawn to the window's edges (gEXSetScissor, from the left and right): from top to bottom for the border, and
//   between the game's own top and bottom while it animates them, as Mario Party 2's board wipe between turns closes it
//   to a band across the middle. Mario Party 3's wipes it sideways instead, 32 pixels a frame: the left edge goes
//   across to close it, then the right edge to open it again after the camera cuts to the next player. A scissor on
//   those steps with one edge at the border's is widened too, each edge as far across the window as across the border
//   (an origin of that fraction of the window's width), so its 3D keeps its projection and the wipe crosses the window
//   as it crosses the console's screen. A screen split between players (a minigame's halves and quarters, at 160)
//   keeps the game's, and so does every camera while the picture is 4:3 (board_back.c: a board's opening scene, whose
//   map is no wider than 4:3), so that its 3D, the scene's host and the players among them, keeps inside 4:3 with its
//   background, not out over the black at the window's sides.
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
#define SCREEN_H 240
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
    return ((left == 0) && (right == SCREEN_W)) || border_wipe(left, right);
}

void camera_view_scissor(Gfx** gfx, s32 left, s32 top, s32 right, s32 bottom) {
    u16 left_origin = G_EX_ORIGIN_LEFT;
    u16 right_origin = G_EX_ORIGIN_RIGHT;

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
        gEXSetScissor((*gfx)++, G_SC_NON_INTERLACE, left_origin, right_origin, 0, top, 0, bottom);
        return;
    }

    gDPSetScissor((*gfx)++, G_SC_NON_INTERLACE, left, top, right, bottom);
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
}
