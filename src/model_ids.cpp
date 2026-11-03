// A 3D model's parts paired with their own between the game's frames: each part's matrix tagged with an ID of its
// model and its part (gEXMatrixGroup), so RT64 interpolates it from the matrix the same part had in the frame before.
//
// RT64 draws frames between the game's at the display's rate by interpolating matrices, and with no tags in a game's
// display lists it guesses which matrix of the frame before each one is: of the draws that look alike (the same
// combiner, render modes and triangle count) it pairs the matrices nearest each other first. Hudson's engine draws a
// model's every part (a node of its hierarchy: a head, a body, a hand) through a matrix of its own, and the characters'
// parts look alike, so a part could be paired with another character's: in Mario Party 3's opening, while Lakitu brings
// the toy box down, a frame between the game's drew Yoshi's head on Peach and Wario's on Donkey Kong, and the
// characters flickered. Tagged, a part is paired by its ID alone, before RT64 guesses for what is left untagged.
//
// The engine walks its models (up to 128, each a slot in its table) once a frame for each camera (up to 6: a screen
// split between players has one a player), each part's matrix into a queue, then draws the queue sorted by depth, a
// gSPMatrix (a load into the modelview) before each part's display list. A game's hooks (its us.toml) call these
// natives, since that code is the decompilations' asm:
// - mp_model_slot before the walk of a model's parts, with the camera, the model's slot and its data (what it draws),
//   and mp_model_slot_end after it. A slot whose data changed holds another model: its parts' IDs change with it, so
//   the new model's parts aren't paired with the old one's. The camera is in the ID too, as each camera draws the same
//   parts at a matrix of its own.
// - mp_model_queued where a part goes into the queue, with its place in the queue, its part number and its model: the
//   part is tagged with the model that is being walked. A part queued outside a model's walk (a minigame's own) isn't,
//   nor one of another model queued in its walk (a model held at one of its parts, or a part taken from another
//   model), whose part number is that model's.
// - mp_model_push just before the part's gSPMatrix, with the list's next command, its place in the queue, its matrix,
//   the frame's number and the list's start and size: the tag (a gEXEnable and the group, pushed, G_EX_PUSH) at the
//   list's next command, which it returns after itself. RT64 reads extended commands only after a gEXEnable, and
//   forgets it at each gDPFullSync, which the draw sends between its layers when it splits them into tasks of their
//   own, so each tag sends one. mp_model_pop after the part's display list, with the list's next-command pointer: the
//   tag popped. Nothing for an untagged part, or near the end of the list (below).
//
// The part is interpolated as RT64 interpolates a matrix it pairs by itself (decomposed, each component its own
// choice), except where its matrix collapses what it draws (a zero determinant: a game hides a part by scaling it to 0)
// or mirrors it, and didn't in the frame before, or the other way round: there it is drawn at its own matrix, as RT64
// doesn't pair those (N64Recomp-browser's RT64_SKIP_COLLAPSED_MATCHES), so a part that comes back from 0 isn't drawn
// growing from nothing over the frames between.
//
// A game gives each frame's display list a size of its own (each scene sets it), and the tags take room in it, four
// commands a part: a part whose tag would leave less than a quarter of the list after it is left untagged, for RT64 to
// pair as before, so the tags never take the room the game draws the rest of its frame in.

#include <cmath>
#include <unordered_map>

#include "display_list.h"

namespace {
    constexpr int32_t SLOTS = 128;
    constexpr int32_t CAMERAS = 8;
    // The parts of a model told apart, and the queue's places kept.
    constexpr int32_t PARTS = 1 << 12;
    constexpr int32_t QUEUE = 1024;
    // The IDs' own range, clear of G_EX_ID_IGNORE (0), G_EX_ID_AUTO (all ones) and the engine's other groups: the camera
    // at bit 26, the slot's generation at 19, the slot at 12 and the part.
    constexpr uint32_t ID_BASE = 0x40000000;
    // Commands a tag takes: a gEXEnable, the group (two) and its pop.
    constexpr int32_t TAG_BYTES = 4 * 8;
    // gbi.h's G_MTX_MODELVIEW, for a group of the modelview.
    constexpr uint32_t MODELVIEW = 0;

    // What each slot last held, and how many times it changed, which goes into its parts' IDs.
    gpr slot_data[SLOTS];
    uint32_t slot_generation[SLOTS];
    // The model being walked (its slot, or -1, its data and the camera).
    int32_t walking = -1;
    gpr walking_data = 0;
    uint32_t walking_camera = 0;
    // Each place in the queue's ID, 0 for an untagged part.
    uint32_t queued[QUEUE];
    bool pushed = false;

    // An ID's matrix's determinant's sign (0 when it collapses what it draws) in the frame it was last drawn.
    struct Seen {
        int32_t frame;
        int8_t sign;
    };
    std::unordered_map<uint32_t, Seen> seen;
    int32_t seen_frame = 0;

    // An element of a Mtx (the N64's fixed point, the integer halves of all sixteen first and then their fractions).
    float element(uint8_t* rdram, gpr mtx, int i) {
        int32_t whole = (int16_t)MEM_H(i * 2, mtx);
        uint32_t fraction = (uint16_t)MEM_HU(32 + i * 2, mtx);
        return (float)((whole << 16) | fraction) / 65536.0f;
    }

    int8_t determinant_sign(uint8_t* rdram, gpr mtx) {
        float m[3][3];
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                m[r][c] = element(rdram, mtx, r * 4 + c);
            }
        }
        float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                    m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        return (det > 0.0f) ? 1 : ((det < 0.0f) ? -1 : 0);
    }

    // Whether the ID's matrix is interpolated: unless it collapses or mirrors what it draws and didn't in the frame
    // before, or the other way round. IDs not drawn for a second are forgotten.
    bool interpolated(uint32_t id, int8_t sign, int32_t frame) {
        if (frame != seen_frame) {
            if (seen.size() > SLOTS * 64) {
                for (auto it = seen.begin(); it != seen.end();) {
                    it = ((frame - it->second.frame) > 60) ? seen.erase(it) : std::next(it);
                }
            }
            seen_frame = frame;
        }
        auto it = seen.find(id);
        bool same = (it == seen.end()) || (it->second.sign == sign);
        seen[id] = Seen{ frame, sign };
        return same && (sign != 0);
    }
}

extern "C" void mp_model_slot(uint8_t*, int32_t camera, int32_t slot, gpr data) {
    if ((slot < 0) || (slot >= SLOTS) || (camera < 0) || (camera >= CAMERAS)) {
        walking = -1;
        return;
    }
    if (slot_data[slot] != data) {
        slot_data[slot] = data;
        slot_generation[slot]++;
    }
    walking = slot;
    walking_data = data;
    walking_camera = (uint32_t)camera;
}

extern "C" void mp_model_slot_end(uint8_t*) {
    walking = -1;
}

extern "C" void mp_model_queued(uint8_t*, int32_t place, int32_t part, gpr model) {
    if ((place < 0) || (place >= QUEUE)) {
        return;
    }
    if ((walking < 0) || (model != walking_data) || (part < 0) || (part >= PARTS)) {
        queued[place] = 0;
        return;
    }
    queued[place] = ID_BASE | (walking_camera << 26) | ((slot_generation[walking] & 0x7F) << 19) |
                    ((uint32_t)walking << 12) | (uint32_t)part;
}

extern "C" gpr mp_model_push(uint8_t* rdram, gpr at, int32_t place, gpr mtx, int32_t frame, gpr list,
                              int32_t list_bytes) {
    pushed = false;
    if ((place < 0) || (place >= QUEUE) || (queued[place] == 0)) {
        return at;
    }
    if (((int32_t)(list + list_bytes - at) - TAG_BYTES) < (list_bytes / 4)) {
        return at;
    }
    uint32_t id = queued[place];
    uint8_t component = interpolated(id, determinant_sign(rdram, mtx), frame) ? G_EX_COMPONENT_AUTO : G_EX_COMPONENT_SKIP;
    GfxCommand commands[3];
    int count = 0;
    gEXEnable(&commands[count++]);
    gEXMatrixGroup(&commands[count], id, G_EX_INTERPOLATE_DECOMPOSE, G_EX_PUSH, MODELVIEW, component, component,
                   component, component, component, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR,
                   G_EX_EDIT_NONE, G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    count += 2;
    for (int i = 0; i < count; i++) {
        MEM_W(0, at) = commands[i].values.word0;
        MEM_W(4, at) = commands[i].values.word1;
        at += 8;
    }
    pushed = true;
    return at;
}

extern "C" void mp_model_pop(uint8_t* rdram, gpr gfx) {
    if (!pushed) {
        return;
    }
    GfxCommand command;
    gEXPopMatrixGroup(&command, MODELVIEW);
    mp_display_list::emit(rdram, gfx, &command, 1);
    pushed = false;
}
