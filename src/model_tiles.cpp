// A 3D model's texture clamped where its triangles' part of it ends.
//
// The engine's model builder (Mario Party 2's func_80033288_33E88, Mario Party 3's func_800260F0_main, the
// decompilations' asm) writes a batch of a model part's triangles as the model is made, after the texture they are
// drawn with if it is another, its tile clamped at the texture's edges. Some models' textures are sheets of pictures
// stacked one under another, a triangle's coordinates on one of them: Mario Party's Crazy Cutter's cones, barricades
// and poles are quads on the top 32 rows of a 32x96 sheet, their coordinates t 0 to 32 (the middle cone's to 31.88).
// A picture's last rows are clear, but at a quad's bottom edge the bilinear filter mixes in row 32, the top row of the
// picture under it, and drawn above the console's resolution that row shows as a thin dark line under the object, a
// little under the bottom of what is drawn. Players don't see it on the console.
//
// So where the batches drawn with a texture reach no further than an edge between two texels inside it (or an eighth
// of a texel short of one), its tile is clamped at that edge, and the filter mixes in nothing past it. A batch whose
// coordinates end further between two texels' edges (most of a character's parts on its texture) keeps the texture's
// own tile, and so does one that runs to the texture's edge. Only a tile's right and bottom are clamped: its top and
// left are where the texture's memory starts.
//
// A game's hooks (its us.toml) call these natives with where the builder keeps its state, its globals:
// - mp_model_batch_start at the builder's start, with the display list's head (the Gfx* it writes at) and the texture
//   its tile holds (NULL at a part's start).
// - mp_model_batch_end before its last store, the batch written and its first vertex not yet moved past it (the
//   builder ends with first = end), with the head, the part's Vtx, the batch's first vertex and one past its last
//   (s16 each) and the texture.

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include <cstdio>
#include <set>
#include <tuple>

namespace {
    constexpr uint32_t G_SETTILE = 0xF5;
    constexpr uint32_t G_SETTILESIZE = 0xF2;
    constexpr int G_TX_RENDERTILE = 0;
    constexpr int G_TX_CLAMP = 2;

    // How far short of a texel's edge, and past it, a coordinate (10.5) may end and still be on it: an eighth of a
    // texel short (Crazy Cutter's middle cone's quad ends at t 31.88), a sixteenth past.
    constexpr int32_t SHORT_OF_EDGE = 4;
    constexpr int32_t PAST_EDGE = 2;

    gpr batch_start = 0;        // the DL head when the batch started

    // The tile the part's stream draws with now: its SetTileSize command, 0 if none to clamp.
    struct Tile {
        gpr size = 0;
        gpr texture = 0;
        int32_t last_s = 0, last_t = 0;     // its own last texel, the texture's
        bool clamp_s = false, clamp_t = false;
        int32_t reach_s = 0, reach_t = 0;   // the furthest its batches' coordinates go (10.5)
        bool any = false;
    } tile;

    // The last texel to clamp an axis at: the edge the batches reach, or the tile's own last texel.
    int32_t clamped_last(bool clamp, int32_t reach, int32_t last) {
        if (!clamp) {
            return last;
        }
        int32_t edge = (reach + 16) >> 5;
        if (edge < 1 || edge - 1 >= last || reach - edge * 32 > PAST_EDGE || edge * 32 - reach > SHORT_OF_EDGE) {
            return last;
        }
        return edge - 1;
    }
}

// At the builder's start: where the batch's commands start, and whether the tile tracked is still the stream's.
extern "C" void mp_model_batch_start(uint8_t* rdram, gpr dl_head, gpr texture) {
    batch_start = (gpr)(int32_t)MEM_W(0, dl_head);
    if (tile.size != 0 && (gpr)(int32_t)MEM_W(0, texture) != tile.texture) {
        tile.size = 0;
    }
}

// At its end, the batch written: a texture it loaded is tracked, and the tile it draws with clamped to the edges
// its batches reach.
extern "C" void mp_model_batch_end(uint8_t* rdram, gpr dl_head, gpr vertices_at, gpr batch_first, gpr batch_end,
                                   gpr texture) {
    gpr end = (gpr)(int32_t)MEM_W(0, dl_head);
    gpr settile = 0, settilesize = 0;
    for (gpr cmd = batch_start; cmd < end; cmd += 8) {
        uint32_t w0 = (uint32_t)MEM_W(0, cmd), w1 = (uint32_t)MEM_W(4, cmd);
        if ((w0 >> 24) == G_SETTILE && ((w1 >> 24) & 7) == G_TX_RENDERTILE) {
            settile = cmd;
        } else if ((w0 >> 24) == G_SETTILESIZE && ((w1 >> 24) & 7) == G_TX_RENDERTILE) {
            settilesize = cmd;
        }
    }

    if (settilesize != 0) {
        tile = Tile();
        uint32_t w0 = (uint32_t)MEM_W(0, settilesize), w1 = (uint32_t)MEM_W(4, settilesize);
        if (settile != 0 && settile < settilesize && (w0 & 0x00FFFFFF) == 0) {
            uint32_t t1 = (uint32_t)MEM_W(4, settile);
            tile.size = settilesize;
            tile.texture = (gpr)(int32_t)MEM_W(0, texture);
            tile.last_s = ((w1 >> 12) & 0xFFF) >> 2;
            tile.last_t = (w1 & 0xFFF) >> 2;
            // cms, shifts and cmt, shiftt: a shift scales the coordinates, which then aren't the texture's texels.
            tile.clamp_s = (((t1 >> 8) & 3) & G_TX_CLAMP) && (t1 & 0xF) == 0;
            tile.clamp_t = (((t1 >> 18) & 3) & G_TX_CLAMP) && ((t1 >> 10) & 0xF) == 0;
        }
    }
    if (tile.size == 0) {
        return;
    }

    gpr vertices = (gpr)(int32_t)MEM_W(0, vertices_at);
    int32_t first = MEM_H(0, batch_first), last = MEM_H(0, batch_end);
    for (int32_t i = first; i < last; i++) {
        int32_t s = MEM_H(8, vertices + i * 16), t = MEM_H(10, vertices + i * 16);
        if (!tile.any || s > tile.reach_s) {
            tile.reach_s = s;
        }
        if (!tile.any || t > tile.reach_t) {
            tile.reach_t = t;
        }
        tile.any = true;
    }
    if (!tile.any) {
        return;
    }

    int32_t last_s = clamped_last(tile.clamp_s, tile.reach_s, tile.last_s);
    int32_t last_t = clamped_last(tile.clamp_t, tile.reach_t, tile.last_t);
    uint32_t w1 = (uint32_t)MEM_W(4, tile.size);
    { static std::set<std::tuple<uint32_t,int,int,int,int>> seen; if ((last_s != tile.last_s || last_t != tile.last_t) && seen.insert({(uint32_t)tile.texture, tile.last_s, tile.last_t, last_s, last_t}).second) printf("MPCLAMP tex=%08X tile=%d,%d -> %d,%d reach=%d,%d
", (uint32_t)tile.texture, tile.last_s, tile.last_t, last_s, last_t, tile.reach_s, tile.reach_t); }
    MEM_W(4, tile.size) = (int32_t)((w1 & 0xFF000000) | ((uint32_t)(last_s << 2) << 12) | (uint32_t)(last_t << 2));
}
