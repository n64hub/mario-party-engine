#ifndef CAMERA_VIEW_H
#define CAMERA_VIEW_H

// A camera's view past 4:3 (camera_view.c says how). A game's patches of the two functions that start a camera's view
// in a display list, its scissor and its viewport and projection, call these in their place.

// Whether a camera with this scissor is drawn to the window's edges: the whole screen (to 320 or 319), or the console's
// overscan border (in a sideways wipe across it, too).
s32 camera_view_widened(s32 left, s32 right);
// The camera's scissor, to the window's edges for a widened camera.
void camera_view_scissor(Gfx** gfx, s32 left, s32 top, s32 right, s32 bottom);
// The camera's viewport, perspective normalization and projection, as the game sets them, with a widened camera's
// viewport kept in the middle. The modelview is the game's own, after this. camera is the game's index of the camera.
void camera_view_projection(Gfx** gfx, s32 camera, Vp* viewport, u16 persp_norm, Mtx* projection, s32 widened);

#endif
