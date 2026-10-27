// Writing commands into a game's display list from a native its us.toml hooks call: the game's code is the
// decompilations' asm, so a native gets the address of the list's next-command pointer (a Gfx**) and writes there.
//
// RT64 reads extended commands only after a gEXEnable in the same display list, so a native that writes them sends one.

#pragma once

#include <cstdint>

#include "recomp.h"

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

namespace mp_display_list {
    constexpr int32_t SCREEN_W = 320;
    constexpr int32_t SCREEN_H = 240;
    // gDPSetScissor.
    constexpr uint32_t G_SETSCISSOR = 0xED;

    // Writes commands at *gfx, the display list's next command, and moves it past them.
    inline void emit(uint8_t* rdram, gpr gfx, const GfxCommand* commands, int count) {
        gpr at = (int32_t)MEM_W(0, gfx);
        for (int i = 0; i < count; i++) {
            MEM_W(0, at) = commands[i].values.word0;
            MEM_W(4, at) = commands[i].values.word1;
            at += 8;
        }
        MEM_W(0, gfx) = (int32_t)at;
    }

    // gDPSetScissor of the whole screen.
    inline void whole_screen(GfxCommand* command) {
        command->values.word0 = (G_SETSCISSOR << 24);
        command->values.word1 = ((SCREEN_W * 4) << 12) | (SCREEN_H * 4);
    }

    // gEXSetScissorAlign: every scissor until the next one aligned to the window's edges, so that one of the whole
    // screen is the whole window. Three commands.
    inline void scissor_to_window(GfxCommand* commands) {
        gEXSetScissorAlign(commands, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -SCREEN_W, 0, 0, 0, SCREEN_W, SCREEN_H);
    }

    // gEXSetScissorAlign back to RT64's own, which has no bounds; these are the furthest the command holds. Three
    // commands.
    inline void scissor_own(GfxCommand* commands) {
        gEXSetScissorAlign(commands, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0, 0x3FFF, 0x3FFF);
    }
}
