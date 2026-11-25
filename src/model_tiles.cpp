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
#include <cstdlib>

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

    // DIAG: the tile's texels, to measure what its clamped edges keep out.
    struct DiagTex { uint32_t img = 0, pal = 0; int fmt = 0, siz = 0, line = 0; } diag_tex;
    uint32_t diag_addr(uint32_t a) {
        if (a >= 0x80000000u && a < 0x80800000u) return a;
        if (a < 0x00800000u && a != 0) return a | 0x80000000u;
        return 0;
    }
    void diag_rgba16(uint8_t* rdram, uint32_t at, int* c) {
        uint32_t v = (uint16_t)MEM_HU(0, (gpr)(int32_t)at);
        c[0] = ((v >> 11) & 31) * 255 / 31; c[1] = ((v >> 6) & 31) * 255 / 31; c[2] = ((v >> 1) & 31) * 255 / 31;
        c[3] = (v & 1) * 255;
    }
    void diag_texel(uint8_t* rdram, int x, int y, int* c) {
        const DiagTex& t = diag_tex;
        uint32_t row = t.img + (uint32_t)y * (uint32_t)t.line * (t.siz == 3 ? 16 : 8);
        if (row + 64 >= 0x80800000u) { c[0] = c[1] = c[2] = c[3] = 0; return; }
        if (t.siz == 0 || t.siz == 1) {
            uint32_t v;
            if (t.siz == 0) { uint32_t b = MEM_BU(0, (gpr)(int32_t)(row + x / 2)); v = (x & 1) ? (b & 15) : (b >> 4); }
            else { v = MEM_BU(0, (gpr)(int32_t)(row + x)); }
            if (t.fmt == 2 && t.pal != 0) { diag_rgba16(rdram, t.pal + v * 2, c); return; }
            if (t.fmt == 3) {   // IA
                int i, a;
                if (t.siz == 0) { i = ((v >> 1) & 7) * 255 / 7; a = (v & 1) * 255; } else { i = (v >> 4) * 17; a = (v & 15) * 17; }
                c[0] = c[1] = c[2] = i; c[3] = a; return;
            }
            int i = t.siz == 0 ? v * 17 : v;    // I (or CI without a palette: its index)
            c[0] = c[1] = c[2] = c[3] = i; return;
        }
        if (t.siz == 2) {
            if (t.fmt == 3) {
                uint32_t v = (uint16_t)MEM_HU(0, (gpr)(int32_t)(row + x * 2));
                c[0] = c[1] = c[2] = v >> 8; c[3] = v & 255; return;
            }
            diag_rgba16(rdram, row + x * 2, c); return;
        }
        uint32_t v = (uint32_t)MEM_W(0, (gpr)(int32_t)(row + x * 4));
        c[0] = v >> 24; c[1] = (v >> 16) & 255; c[2] = (v >> 8) & 255; c[3] = v & 255;
    }
    int texel_diff(uint8_t* rdram, int x0, int y0, int x1, int y1) {
        int a[4], b[4];
        diag_texel(rdram, x0, y0, a);
        diag_texel(rdram, x1, y1, b);
        int d = abs(a[3] - b[3]);
        for (int k = 0; k < 3; k++) {
            int e = abs(a[k] * a[3] / 255 - b[k] * b[3] / 255);
            if (e > d) d = e;
        }
        return d;
    }

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
    uint32_t image = 0, img = 0, pal = 0;
    for (gpr cmd = batch_start; cmd < end; cmd += 8) {
        uint32_t w0 = (uint32_t)MEM_W(0, cmd), w1 = (uint32_t)MEM_W(4, cmd);
        if ((w0 >> 24) == 0xFD) {
            image = w1;
        } else if ((w0 >> 24) == 0xF0) {
            pal = image;
        } else if ((w0 >> 24) == 0xF3 || (w0 >> 24) == 0xF4) {
            img = image;
        }
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
            uint32_t t0 = (uint32_t)MEM_W(0, settile);
            diag_tex.img = diag_addr(img);
            diag_tex.pal = diag_addr(pal);
            diag_tex.fmt = (t0 >> 21) & 7;
            diag_tex.siz = (t0 >> 19) & 3;
            diag_tex.line = (t0 >> 9) & 0x1FF;
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
    if (last_s != tile.last_s || last_t != tile.last_t) {
        static std::set<uint64_t> seen;
        uint64_t key = ((uint64_t)(uint32_t)tile.texture << 32) | ((uint64_t)(last_s & 0xFFF) << 12) | (uint64_t)(last_t & 0xFFF);
        // DIAG: a table at 0x80F00000 ('MPTT', count, then {command, the game's w1, the clamped w1}) a page's script
        // swaps to see a scene with the clamps and without.
        const gpr table = (gpr)(int32_t)0x80F00000;
        if ((uint32_t)MEM_W(0, table) != 0x4D505454) {
            MEM_W(0, table) = 0x4D505454;
            MEM_W(4, table) = 0;
        }
        uint32_t count = (uint32_t)MEM_W(4, table);
        uint32_t own = (w1 & 0xFF000000) | ((uint32_t)(tile.last_s << 2) << 12) | (uint32_t)(tile.last_t << 2);
        uint32_t clamped = (w1 & 0xFF000000) | ((uint32_t)(last_s << 2) << 12) | (uint32_t)(last_t << 2);
        if (count > 0 && (gpr)(int32_t)MEM_W(8 + (count - 1) * 12, table) == tile.size) {
            MEM_W(8 + (count - 1) * 12 + 8, table) = (int32_t)clamped;
        } else if (count < 80000) {
            MEM_W(8 + count * 12, table) = (int32_t)tile.size;
            MEM_W(8 + count * 12 + 4, table) = (int32_t)own;
            MEM_W(8 + count * 12 + 8, table) = (int32_t)clamped;
            MEM_W(4, table) = (int32_t)(count + 1);
        }
        if (seen.insert(key).second) {
            // DIAG: how different the texels past each clamped edge are from the last ones drawn: the line the
            // filter would show. max/mean of the larger of the premultiplied color's and alpha's differences.
            int maxd_t = -1, sum_t = 0, n_t = 0, maxd_s = -1, sum_s = 0, n_s = 0;
            if (diag_tex.img != 0) {
                if (last_t != tile.last_t) {
                    for (int x = 0; x <= last_s; x++) {
                        int d = texel_diff(rdram, x, last_t, x, last_t + 1);
                        if (d > maxd_t) maxd_t = d;
                        sum_t += d; n_t++;
                    }
                }
                if (last_s != tile.last_s) {
                    for (int y = 0; y <= last_t; y++) {
                        int d = texel_diff(rdram, last_s, y, last_s + 1, y);
                        if (d > maxd_s) maxd_s = d;
                        sum_s += d; n_s++;
                    }
                }
            }
            fprintf(stderr, "MPCLAMP tex=%08X tile=%d,%d to %d,%d reach=%d,%d img=%08X pal=%08X fmt=%d siz=%d line=%d "
                    "edge_t max=%d mean=%d edge_s max=%d mean=%d\n", (unsigned)(uint32_t)tile.texture,
                    (int)tile.last_s, (int)tile.last_t, (int)last_s, (int)last_t, (int)tile.reach_s, (int)tile.reach_t,
                    diag_tex.img, diag_tex.pal, diag_tex.fmt, diag_tex.siz, diag_tex.line,
                    maxd_t, n_t ? sum_t / n_t : -1, maxd_s, n_s ? sum_s / n_s : -1);
        }
    }
    if ((uint32_t)MEM_W(0, (gpr)(int32_t)0x80EFFFF0) == 0x4F464621) {   // DIAG: 'OFF!', the fix off
        return;
    }
    MEM_W(4, tile.size) = (int32_t)((w1 & 0xFF000000) | ((uint32_t)(last_s << 2) << 12) | (uint32_t)(last_t << 2));
}
