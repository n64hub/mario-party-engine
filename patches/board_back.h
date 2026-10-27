#ifndef BOARD_BACK_H
#define BOARD_BACK_H

// A board's background drawn past 4:3 and moving with the board's 3D at the display's rate (board_back.c says how).
// A game's patch of the function that draws the background's tiles picks the tiles under a picture wider than 4:3 and
// draws them through these, in this order:
//
//     gfx = board_back_begin(&game_gfx);           its own list, black behind it, a scissor to the window's edges
//     ...the game's own modes and combiner...
//     board_back_view(gfx, left, top, scale, mirrored);
//     ...for each tile: board_back_tile (or the game's texture load, then board_back_quad)...
//     board_back_end(gfx);

// The most commands and corners a frame's tiles may take: up to 34 commands and 8 corners a tile.
#define BOARD_BACK_COMMANDS 8192
#define BOARD_BACK_CORNERS 2048

Gfx** board_back_begin(Gfx** game_gfx);
// The map's (left, top) at the picture's left edge (or its right edge, mirrored), the map drawn at scale times its size.
void board_back_view(Gfx** gfx, f32 left, f32 top, f32 scale, s32 mirrored);
// A 64x48 tile of 16-bit RGBA texels at (x, y) on the map, and the tiles to its right, below it and below and to its
// right on the map (NULL where there is none), whose edge texels it is filtered with.
void board_back_tile(Gfx** gfx, s32 x, s32 y, void* tile, void* right, void* below, void* below_right);
// A width x height texture at (x, y) on the map, from its texel (s, t), from the texture the game loaded last.
void board_back_quad(Gfx** gfx, s32 x, s32 y, s32 width, s32 height, s32 s, s32 t);
void board_back_end(Gfx** gfx);

#endif
