#ifndef CAMERA_VIEW_H
#define CAMERA_VIEW_H

// A camera's view past 4:3 (camera_view.c says how). A game's patches of the two functions that start a camera's view
// in a display list, its scissor and its viewport and projection, call these in their place.

// Whether a camera with this scissor is drawn to the window's edges: the whole screen (to 320 or 319), or the console's
// overscan border (in a sideways wipe across it, too).
s32 camera_view_widened(s32 left, s32 right);
// The camera's scissor, to the window's edges for a widened camera.
void camera_view_scissor(Gfx** gfx, s32 left, s32 top, s32 right, s32 bottom);
// Whether the last camera's scissor closes the console's overscan border, as a wipe does (sideways, or to a band), and
// that scissor as gEXSetScissor takes it: its left and right edges' origins, and its edges (left, top, right, bottom)
// in pixels, the left and right from their origins. Forgotten once asked, so a scene whose camera doesn't come here
// never sees a wipe that ended in another.
s32 camera_view_wipe(u16 origins[2], s32 edges[4]);
// Where the last camera's scissor is a sideways wipe's, the triangle of no area that keeps its 3D on the window's
// viewport, again: after a list that set a projection or viewport of its own and put the camera's back, which RT64
// counts as a new projection. The scissor is put back after it.
void camera_view_span(Gfx** gfx);
// Where the last camera's scissor is a sideways wipe's, a list of the game's own that sets what RT64 counts as a new
// projection (the clip ratio), the triangle again after it, and the display list's end: for a game whose code ends the
// camera's list with a call of that list after the scissor, which follows these and is never read.
void camera_view_span_after(Gfx** gfx, Gfx* list);
// The camera's viewport, perspective normalization and projection, as the game sets them, with a widened camera's
// viewport kept in the middle. The modelview is the game's own, after this. camera is the game's index of the camera.
void camera_view_projection(Gfx** gfx, s32 camera, Vp* viewport, u16 persp_norm, Mtx* projection, s32 widened);

#endif
