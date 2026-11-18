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
// growing from nothing over the frames between. So is a part turned more than 75 degrees since the frame before: RT64
// turns it the short way, and that far in one frame is a game putting it somewhere else, not a turn. Mario Party 3's
// party setup spins a star drawn as two models at one place, its front and its back half a turn from it, at most 51
// degrees a frame; to start a spin again it puts the front at -51 degrees and the back, which was at 0, at 129, and the
// back turned the short way, 64 degrees, stood across the front in the frame between, the BONUS sign on its face
// showing. Mario Party 1's dice, rolled to pick the turns' order, show another face each frame, a quarter or half turn,
// and the frames between drew them halfway, two faces at once. The parts of Mario Party 1's opening, menus and first
// board turn under 60 degrees a frame but for 124 of 268,000, and those (the dice) 80 degrees or more.
//
// Nor is any part where its camera cut to another shot. A part's matrix holds the camera's view (the engine multiplies
// it in as it walks the model), so a part interpolated across a cut was drawn with the camera partway from one shot to
// the other, the picture swept for a frame: Mario Party 2's opening cuts from the side of the warp pipe to down its
// length, and from the block on the wall back to the pipe, without moving the viewport, where camera_view.c tells a cut.
// A game's patch calls mp_model_camera as it sets a camera up (its patches/syms.ld names the native), with its view and
// projection, and the camera cut where its view turned more than 45 degrees, or its eye jumped more than 8 of its near
// plane's distances and 4 times as far as it moved the frame before, as RT64 tells a cut of a camera with no group of a
// game's own (RT64_SKIP_CAMERA_CUTS, which can't see this one: the projection holds no view). Mario Party 2's two cuts
// jump 11.5 near planes after moving under 0.1, and its opening's camera otherwise moves at most 1.5 and turns at most
// 9 degrees a frame. A game whose patch doesn't call it keeps every camera's parts interpolated, as before.
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

    // A part's matrix's axes (its first three rows, each made 1 long, or 0) and its determinant's sign (0 where it
    // collapses what it draws).
    struct Pose {
        float axes[3][3];
        int8_t sign;
    };
    // An ID's pose in the frame it was last drawn.
    struct Seen {
        int32_t frame;
        Pose pose;
    };
    // 1 + 2 cos(75 degrees): the trace of a turn of 75 degrees.
    constexpr float TURN_TRACE = 1.5176f;
    std::unordered_map<uint32_t, Seen> seen;
    int32_t seen_frame = 0;

    // The frame's tags so far: where each part's origin is in its camera's view, where its group command is in the
    // list, and whether it is drawn at its own matrix.
    struct Placed {
        uint32_t id;
        float origin[3];
        gpr group;
        bool own;
    };
    constexpr size_t PLACED = 512;
    Placed placed[PLACED];
    size_t placed_count = 0;
    int32_t placed_frame = 0;

    // Whether two origins are one place: within a hundredth of their distance from the eye.
    bool same_place(const float a[3], const float b[3]) {
        float d = 0.0f;
        float r = 0.0f;
        for (int c = 0; c < 3; c++) {
            d += (a[c] - b[c]) * (a[c] - b[c]);
            r = std::fmax(r, std::fmax(std::fabs(a[c]), std::fabs(b[c])));
        }
        return std::sqrt(d) <= (r * 0.01f + 0.01f);
    }

    // Each camera's view when it was last set up (its eye in the world, the way it faces and how far the eye moved, in
    // its near plane's distances), and whether it cut to another shot there.
    struct View {
        bool known;
        float eye[3];
        float forward[3];
        float step;
    };
    View views[CAMERAS];
    bool cut[CAMERAS];

    // An element of a Mtx (the N64's fixed point, the integer halves of all sixteen first and then their fractions), its
    // sign kept: put together as a signed whole, as an unsigned fraction made a negative one read near 65536.
    float element(uint8_t* rdram, gpr mtx, int i) {
        int32_t whole = (int16_t)MEM_H(i * 2, mtx);
        uint32_t fraction = (uint16_t)MEM_HU(32 + i * 2, mtx);
        return (float)(int32_t)(((uint32_t)whole << 16) | fraction) / 65536.0f;
    }

    Pose pose(uint8_t* rdram, gpr mtx) {
        float m[3][3];
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                m[r][c] = element(rdram, mtx, r * 4 + c);
            }
        }
        float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                    m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        Pose result;
        result.sign = (det > 0.0f) ? 1 : ((det < 0.0f) ? -1 : 0);
        for (int r = 0; r < 3; r++) {
            float length = std::sqrt(m[r][0] * m[r][0] + m[r][1] * m[r][1] + m[r][2] * m[r][2]);
            for (int c = 0; c < 3; c++) {
                result.axes[r][c] = (length > 0.0f) ? (m[r][c] / length) : 0.0f;
            }
        }
        return result;
    }

    // Whether a part turned more than 75 degrees from one pose to the other: the sum of their axes' products is the
    // trace of the turn between them, 1 + 2 cos(angle).
    bool turned_far(const Pose& before, const Pose& now) {
        float trace = 0.0f;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                trace += before.axes[r][c] * now.axes[r][c];
            }
        }
        return trace < TURN_TRACE;
    }

    // Whether the camera cut to another shot since its view before (the header says how it is told). A view that can't
    // be read so (no perspective, a matrix that can't be inverted) is never a cut, and is forgotten.
    bool camera_cut(uint8_t* rdram, gpr view, gpr projection, View& last) {
        bool known = last.known;
        last.known = false;

        // The near plane's distance, from a perspective that sets w from z (w = z * m23): m32 / (m22 + m23).
        float m22 = element(rdram, projection, 10);
        float m23 = element(rdram, projection, 11);
        float m32 = element(rdram, projection, 14);
        float near_distance = ((m22 + m23) != 0.0f) ? std::fabs(m32 / (m22 + m23)) : 0.0f;
        if ((m23 == 0.0f) || !std::isfinite(near_distance) || (near_distance <= 0.0f)) {
            return false;
        }

        // The view takes a point of the world v to v R + t (a row vector): the eye is -t R^-1, and the way it faces is
        // the camera's z axis in the world, R^-1's last row.
        float m[3][3];
        float t[3];
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                m[r][c] = element(rdram, view, r * 4 + c);
            }
            t[r] = element(rdram, view, 12 + r);
        }
        float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                    m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        if (!std::isfinite(det) || (det == 0.0f)) {
            return false;
        }
        float inv[3][3] = {
            { m[1][1] * m[2][2] - m[1][2] * m[2][1], m[0][2] * m[2][1] - m[0][1] * m[2][2], m[0][1] * m[1][2] - m[0][2] * m[1][1] },
            { m[1][2] * m[2][0] - m[1][0] * m[2][2], m[0][0] * m[2][2] - m[0][2] * m[2][0], m[0][2] * m[1][0] - m[0][0] * m[1][2] },
            { m[1][0] * m[2][1] - m[1][1] * m[2][0], m[0][1] * m[2][0] - m[0][0] * m[2][1], m[0][0] * m[1][1] - m[0][1] * m[1][0] },
        };
        float eye[3];
        float forward[3];
        float length = 0.0f;
        for (int c = 0; c < 3; c++) {
            eye[c] = -(t[0] * inv[0][c] + t[1] * inv[1][c] + t[2] * inv[2][c]) / det;
            forward[c] = inv[2][c];
            length += forward[c] * forward[c];
        }
        length = std::sqrt(length);
        if (!std::isfinite(length) || (length == 0.0f)) {
            return false;
        }
        for (int c = 0; c < 3; c++) {
            forward[c] /= length;
        }

        float step = 0.0f;
        float turn = 1.0f;
        if (known) {
            float d[3] = { eye[0] - last.eye[0], eye[1] - last.eye[1], eye[2] - last.eye[2] };
            step = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / near_distance;
            turn = forward[0] * last.forward[0] + forward[1] * last.forward[1] + forward[2] * last.forward[2];
        }
        constexpr float MAX_TURN_COSINE = 0.7071f;
        constexpr float MIN_JUMP = 8.0f;
        constexpr float MIN_JUMP_RATIO = 4.0f;
        bool jumped = known && std::isfinite(step) && std::isfinite(turn) &&
                      ((turn < MAX_TURN_COSINE) || ((step > MIN_JUMP) && (step > last.step * MIN_JUMP_RATIO)));
        last.known = true;
        for (int c = 0; c < 3; c++) {
            last.eye[c] = eye[c];
            last.forward[c] = forward[c];
        }
        last.step = jumped ? 0.0f : step;
        return jumped;
    }

    // Whether the ID's matrix is interpolated: unless it collapses or mirrors what it draws and didn't in the frame
    // before, or the other way round, or it turned more than 75 degrees since the frame before. IDs not drawn for a
    // second are forgotten.
    bool interpolated(uint32_t id, const Pose& now, int32_t frame, bool& paired) {
        if (frame != seen_frame) {
            if (seen.size() > SLOTS * 64) {
                for (auto it = seen.begin(); it != seen.end();) {
                    it = ((frame - it->second.frame) > 60) ? seen.erase(it) : std::next(it);
                }
            }
            seen_frame = frame;
        }
        auto it = seen.find(id);
        paired = (it != seen.end()) && (it->second.frame == frame - 1);
        bool same = (it == seen.end()) ||
                    ((it->second.pose.sign == now.sign) &&
                     ((it->second.frame != frame - 1) || !turned_far(it->second.pose, now)));
        seen[id] = Seen{ frame, now };
        return same && (now.sign != 0);
    }
}

// mp_model_camera(camera, view, projection), a native a game's patch calls as it sets a camera up for a frame, with the
// camera's view and projection: whether the camera cut to another shot, so its models' parts aren't interpolated.
extern "C" void mp_model_camera(uint8_t* rdram, recomp_context* ctx) {
    int32_t camera = (int32_t)ctx->r4;
    if ((camera < 0) || (camera >= CAMERAS)) {
        return;
    }
    cut[camera] = camera_cut(rdram, ctx->r5, ctx->r6, views[camera]);
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

// The group's two commands at the address: the part interpolated (its components AUTO) or drawn at its own matrix.
static void group(uint8_t* rdram, gpr at, uint32_t id, bool keep) {
    uint8_t component = keep ? G_EX_COMPONENT_AUTO : G_EX_COMPONENT_SKIP;
    GfxCommand commands[2];
    gEXMatrixGroup(&commands[0], id, G_EX_INTERPOLATE_DECOMPOSE, G_EX_PUSH, MODELVIEW, component, component,
                   component, component, component, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR,
                   G_EX_EDIT_NONE, G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    for (int i = 0; i < 2; i++) {
        MEM_W(i * 8, at) = commands[i].values.word0;
        MEM_W(i * 8 + 4, at) = commands[i].values.word1;
    }
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
    uint32_t camera = (id >> 26) & (CAMERAS - 1);
    Pose now = pose(rdram, mtx);
    bool paired = false;
    bool keep = interpolated(id, now, frame, paired) && !cut[camera];

    // A part drawn at its own matrix (new, mirrored or turned far) draws the parts at its place at theirs too, those
    // tagged before it and after it, in this frame: the halves of one thing (Mario Party 3's star and the BONUS dial the
    // game puts behind it) stay together, rather than one drawn at its own turn and the other between two.
    if (frame != placed_frame) {
        placed_count = 0;
        placed_frame = frame;
    }
    Placed here;
    here.id = id;
    for (int c = 0; c < 3; c++) {
        here.origin[c] = element(rdram, mtx, 12 + c);
    }
    here.own = (now.sign != 0) && (!keep || !paired);
    for (size_t i = 0; i < placed_count; i++) {
        Placed& other = placed[i];
        if ((((other.id >> 26) & (CAMERAS - 1)) != camera) || (other.own == here.own) ||
            !same_place(other.origin, here.origin)) {
            continue;
        }
        if (here.own) {
            other.own = true;
            group(rdram, other.group, other.id, false);
        }
        else {
            here.own = true;
            keep = false;
        }
    }

    GfxCommand enable;
    gEXEnable(&enable);
    MEM_W(0, at) = enable.values.word0;
    MEM_W(4, at) = enable.values.word1;
    at += 8;
    here.group = at;
    group(rdram, at, id, keep);
    at += 16;
    if (placed_count < PLACED) {
        placed[placed_count++] = here;
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
