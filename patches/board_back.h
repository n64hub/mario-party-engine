#ifndef BOARD_BACK_H
#define BOARD_BACK_H

// A board's background drawn past 4:3 and moving with the board's 3D at the display's rate (board_back.c says how).
// A game's patch of the function that draws the background's tiles picks the tiles under a picture wider than 4:3 and
// draws them through these, in this order:
//
//     board_back_area(left, top, scale, mirrored, side, area);   the map to pick the tiles of
//     gfx = board_back_begin(&game_gfx);           its own list, black behind it, a scissor to the window's edges
//     ...the game's own modes and combiner...
//     board_back_view(gfx, left, top, scale, mirrored, map_width);
//     ...for each tile: board_back_tile (or the game's texture load, then board_back_quad)...
//     board_back_end(gfx);

// The most commands and corners a frame's tiles may take: up to 34 commands and 8 corners a tile.
#define BOARD_BACK_COMMANDS 8192
#define BOARD_BACK_CORNERS 2048

// The map (x0, y0, x1, y1) under a picture side pixels wider than 320 on each side, at this view (board_back_view's)
// and every view RT64 draws between the last frame's and it, unless the view jumped (a cut). Before board_back_view,
// which makes this view the last.
void board_back_area(f32 left, f32 top, f32 scale, s32 mirrored, f32 side, f32 area[4]);
Gfx** board_back_begin(Gfx** game_gfx);
// The map's (left, top) at the picture's left edge (or its right edge, mirrored), the map drawn at scale times its size.
// map_width is the whole map's width in its pixels, the tiles not loaded yet included.
void board_back_view(Gfx** gfx, f32 left, f32 top, f32 scale, s32 mirrored, s32 map_width);
// A 64x48 tile of 16-bit RGBA texels at (x, y) on the map, and the tiles to its right, below it and below and to its
// right on the map (NULL where there is none), whose edge texels it is filtered with.
void board_back_tile(Gfx** gfx, s32 x, s32 y, void* tile, void* right, void* below, void* below_right);
// A width x height texture at (x, y) on the map, from its texel (s, t), from the texture the game loaded last.
void board_back_quad(Gfx** gfx, s32 x, s32 y, s32 width, s32 height, s32 s, s32 t);
void board_back_end(Gfx** gfx);

// A tile's compressed bytes as they are read and decompressed, for a game's slots past its own (extra counts from 0,
// fewer than BOARD_BACK_ARCHIVES), in place of the game's heap, which it sized for the tiles under 4:3: a cut or a zoom
// out needs most of the tiles past 4:3 at once, and held to a few at a time on the heap they came over several frames,
// with black at the window's side until they had. NULL for bytes larger than a tile's texels, which take the heap.
#define BOARD_BACK_ARCHIVES 88
void* board_back_archive(s32 extra, u32 size);
// Whether a slot's bytes are board_back_archive's, which the game's heap never frees.
s32 board_back_own_archive(void* bytes);

// Whether the frame's picture is 4:3: its background a map that reaches no further than 4:3's sides, as a board's
// opening scene's does, so the window's sides are black. The camera's view and the board's HUD stay inside 4:3 then.
s32 board_back_4x3(void);
// A new frame, from the process scheduler, once a frame: a picture is 4:3 only while a background says so.
void board_back_frame(void);

#endif
