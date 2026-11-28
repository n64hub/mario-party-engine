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
//   model), whose part number is that model's. A model's hierarchy can name one part at several of its nodes, each
//   drawn at a matrix of its own: Mario Party 1's Coin Block Bash draws the coins that burst from a block as 7 copies
//   of one part, the coin bags as 3 and a model of its field up to 9, 5 to 19 copies a frame in all. Under one ID, a
//   frame held several matrices for RT64 to pair with the frame before's, and a copy was drawn in the frames between
//   partway to another copy's matrix. So each copy has an ID of its own, which holds its number: that of the copy of
//   its part the frame before that lies nearest where that copy's step of the frame before goes on to (mp_model_push),
//   or one no copy had. Not the how-manyth it is in the walk: when a block breaks, the walk leaves its copy out, and the
//   copies after it took each the number of the one before, so the blocks left were each drawn in the frame between
//   halfway to the next one's place. A copy takes another's number within a fiftieth of its distance from the eye of
//   where that one's step goes on to, or within a quarter of it of where one first drawn the frame before was (the
//   blocks fly in from the eye at the start, a tenth of their distance a frame), so a copy that comes into the walk
//   ahead of one at rest, a block's length away, gets a number of its own, and isn't interpolated. A part drawn once,
//   as it was the frame before, keeps its one number unless it moved a tenth of its distance from the eye (a block
//   that lands, or breaks and is put away far below). Past 32 copies, a part is left untagged.
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
// But a turn that far which goes on with the part's turn of the frame before is a turn: about the same axis (within 30
// degrees, the axis as the camera sees it), the same way, where the turn before was 10 degrees or more and interpolated
// itself, and under 135 degrees (a half turn has no one way to go). In Mario Party 3's opening, as the characters run
// to the Millennium Star, a foot swings through 77 to 85 degrees in one frame of each step, after 33 to 65 the frame
// before and 54 after, and drawn at its own matrix it stood still for the frame between and then stepped. A die never
// goes on so, as it turns to its first face from rest and to each after from one it was put at, nor the setup star's
// back, from rest. But the setup's stars, put back to start a spin, may: one went 25.7 degrees, then 77.1 and 51.4, and
// the frame between drew it 38 degrees on and the card on its face at its own turn, the card twice. So a part that goes
// on so at the place of another model's part (as below) is still drawn at its own matrix, and the parts at its place
// with it: two models at one place are one thing the game put somewhere else, where a foot turns alone.
//
// Nor is a part that blinks: moved a tenth of its distance from the eye or more in a frame, back to its place of the
// frame before that (within a hundredth of its distance, as below). Mario Party 1's Coin Block Bash blinks a player
// the hammer hit by putting them about 2,100 units off, near the eye and out of view, every other frame, each step as
// long as the one before, so RT64 didn't take it for a jump: the frames between drew the player halfway, by the
// field's near fence, and they seemed to warp. A player's every part blinks so, 0.4 of its distance or more, while the
// parts that go back and forth otherwise there (a coin bag bouncing, a part shaking) move under 0.05.
//
// And a part drawn at its own matrix (new, mirrored, turned far or blinking) draws every other part at the same place (its
// origin within a hundredth of its distance from the eye, the same camera) at its own too, in that frame, those tagged
// before it (their group rewritten in the list) and after it: what the game draws as one thing in two models stays
// in one piece. The setup's star, turned face on, swaps its back for the BONUS dial, another model, put there at the
// front's turn plus half a turn; the dial was drawn at its own turn and the front between its two, 12 degrees apart,
// and the frame between showed a second outline and the dial's pink rim.
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
// And where a camera turned, every part of it has its place interpolated outright. RT64 otherwise chooses
// (G_EX_COMPONENT_AUTO), and puts a part at its new place, not partway, where it moved 5 units or more in a frame and
// 10 times as far as in the frame before: it takes that for the game putting it somewhere else. A part's matrix holds
// the camera's view, so where the camera turns from rest every part moves as far as it is from the eye: the near ones
// moved under 5 and were drawn partway, the far ones put at the new place, and the frame between came apart. Mario
// Party 1's Mini-Game House turns from its red door to the Pot o' Skills or the Mini-Game table, and in the first frame
// between, the wall was drawn a tenth of the way and the door all of it, out of its archway, the white past the room's
// back showing beside it. A camera turned where most of its parts turned since the frame before (a quarter of a degree
// or more), as each part turns with it: the room's parts don't turn otherwise, nor do most of the characters'. Not a
// part alone, as one the game turns to face the camera never turns with it: Toad's crystal ball's glass was put at
// its new place while the ball inside it, which turns, was drawn partway. The parts of the camera already tagged in
// the frame have their group rewritten as the count tips one way or the other.
//
// And so where most of a camera's parts moved 5 units or more since the frame before, as where it moves. A place
// interpolated outright leaves RT64 no step of the frame before (rt64_rigid_body.cpp sets it to 0), so in the frame
// after, its place RT64's to choose again, a part that moved 5 units or more is put at its new place, as from rest.
// Mario Party 1's Mushroom Mix-Up opens with its camera moving and turning a little, every one of its 68 parts moving
// 37 units a frame and 13 to 38 of them turning: the count tipped under half and back from frame to frame, and in each
// frame it tipped under, the characters were put at their new place in the first frame between and their feet went
// into the mushroom they stand on (Yoshi's shoes went from 140-210 of a 1280x720 dump's pixels to 7-37 for 2 or 3
// frames between, four times in the camera's first second, at 144 Hz). Where most of its parts move that far without
// it (the characters running), their places are interpolated outright too, so one the game puts somewhere else among
// them is swept for a frame, as where the camera turned. Where the camera neither turned nor moved, a part's place is
// still RT64's to choose, so one the game puts somewhere else in a frame is still put there.
//
// A game gives each frame's display list a size of its own (each scene sets it), and the tags take room in it, four
// commands a part: a part whose tag would leave less than a quarter of the list after it is left untagged, for RT64 to
// pair as before, so the tags never take the room the game draws the rest of its frame in.

#include <cmath>
#include <cstring>
#include <unordered_map>

#include "display_list.h"

namespace {
    constexpr int32_t SLOTS = 128;
    constexpr int32_t CAMERAS = 8;
    // The parts of a model told apart, the copies of a part in one walk, and the queue's places kept.
    constexpr int32_t PARTS = 1 << 12;
    constexpr int32_t COPIES = 1 << 5;
    constexpr int32_t QUEUE = 1024;
    // The IDs' own range, clear of G_EX_ID_IGNORE (0), G_EX_ID_AUTO (all ones) and the engine's other groups: the camera
    // at bit 27, the slot's generation at 24, the slot at 17, the copy at 12 and the part. A slot's generation only has
    // to differ from the frame before's, as an ID is paired with that frame's alone. The bits from MODEL_SHIFT up tell the
    // model apart (its camera, its slot and what the slot held).
    constexpr uint32_t ID_BASE = 0x40000000;
    constexpr int CAMERA_SHIFT = 27;
    constexpr int GENERATION_SHIFT = 24;
    constexpr uint32_t GENERATIONS = 1 << 3;
    constexpr int MODEL_SHIFT = 17;
    constexpr int COPY_SHIFT = 12;
    uint32_t camera_of(uint32_t id) {
        return (id >> CAMERA_SHIFT) & (CAMERAS - 1);
    }
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
    // How many times each part was queued in the walk so far, kept for the walk it was last counted in.
    uint32_t walk_number = 0;
    uint32_t part_walk[PARTS];
    uint8_t part_copies[PARTS];
    // Each place in the queue's ID, 0 for an untagged part.
    uint32_t queued[QUEUE];
    bool pushed = false;

    // A part's matrix's axes (its first three rows, each made 1 long, or 0) and its determinant's sign (0 where it
    // collapses what it draws).
    struct Pose {
        float axes[3][3];
        int8_t sign;
    };
    // An ID's pose in the frame it was last drawn, and the axis it turned about since the frame before that (1 long), where
    // that turn was 10 degrees or more and interpolated, or 0. Its origin in that frame, and in the frame it was drawn
    // in before that.
    struct Seen {
        int32_t frame;
        Pose pose;
        float spin[3];
        float origin[3];
        int32_t before_frame;
        float before[3];
    };
    // 1 + 2 cos(75 degrees): the trace of a turn of 75 degrees.
    constexpr float TURN_TRACE = 1.5176f;
    // 1 + 2 cos(135 degrees): the trace of a turn of 135 degrees.
    constexpr float FLIP_TRACE = -0.4142f;
    // 2 sin(10 degrees): how long a turn's axis (below) is at 10 degrees.
    constexpr float SPIN_AXIS = 0.3473f;
    // cos(30 degrees): how near a turn's axis is to the one before, at least, to go on with it.
    constexpr float SPIN_SAME = 0.8660f;
    // How far a part moved in a frame, at least, for its move back to where it was the frame before to be a blink: a tenth
    // of its distance from the eye.
    constexpr float BLINK = 0.1f;
    // 4 (1 - cos(0.25 degrees)): the sum of a pose's axes' squared differences from one turned a quarter of a degree
    // from it.
    constexpr float TURNING = 3.81e-5f;
    std::unordered_map<uint32_t, Seen> seen;
    int32_t seen_frame = 0;

    // A part's copies (its ID less the copy): how many its model's last walk queued, and those drawn in a frame, each
    // number's origin and its step from the frame before (where its number was drawn then), and those of the frame
    // before.
    struct Copy {
        float origin[3];
        float step[3];
    };
    struct Copies {
        uint32_t walk;
        int32_t walked;
        int32_t frame;
        uint32_t taken;
        uint32_t stepped;
        Copy now[COPIES];
        uint32_t had;
        uint32_t had_stepped;
        Copy before[COPIES];
    };
    std::unordered_map<uint32_t, Copies> copies;
    // How far a copy may lie from where its step of the frame before puts it, to take that copy's number: a fiftieth of
    // its distance from the eye, and a quarter of it from where a copy first drawn in the frame before was.
    constexpr float COPY_NEAR = 0.02f;
    constexpr float COPY_NEW = 0.25f;

    // The frame's tags so far: where each part's origin is in its camera's view, where its group command is in the
    // list, whether it is drawn at its own matrix, and whether it turned more than 75 degrees, going on with its turn.
    struct Placed {
        uint32_t id;
        float origin[3];
        gpr group;
        bool own;
        bool keep;
        bool went_on;
    };
    constexpr size_t PLACED = 512;
    Placed placed[PLACED];
    size_t placed_count = 0;
    int32_t placed_frame = 0;
    // Each camera's parts interpolated and paired in the frame so far, how many of them turned and how many moved 5
    // units or more since the frame before, and whether either is most of them, which their groups say.
    int32_t paired_parts[CAMERAS];
    int32_t turned_parts[CAMERAS];
    int32_t moved_parts[CAMERAS];
    bool turning_camera[CAMERAS];
    // How far a part moved in a frame, at least, for RT64 to put it at its new place where it had no step before.
    constexpr float MOVING = 5.0f;

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

    // The turn from one pose to the other (before's axes inverted, which is their transpose, times now's): its trace,
    // 1 + 2 cos(angle), the sum of their axes' products, and its axis, from its antisymmetric part, 2 sin(angle) long.
    float turn_between(const Pose& before, const Pose& now, float axis[3]) {
        float t[3][3];
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                t[r][c] = before.axes[0][r] * now.axes[0][c] + before.axes[1][r] * now.axes[1][c] +
                          before.axes[2][r] * now.axes[2][c];
            }
        }
        axis[0] = t[1][2] - t[2][1];
        axis[1] = t[2][0] - t[0][2];
        axis[2] = t[0][1] - t[1][0];
        return t[0][0] + t[1][1] + t[2][2];
    }

    // Whether a part turned at all from one pose to the other (a quarter of a degree or more), told by its axes'
    // differences, which a turn that small leaves clearer than the trace.
    bool turning(const Pose& before, const Pose& now) {
        float sum = 0.0f;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                float d = now.axes[r][c] - before.axes[r][c];
                sum += d * d;
            }
        }
        return sum >= TURNING;
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

    // Whether a part moved from one origin to the other as far as a blink (a tenth of its distance from the eye or more).
    bool blink_far(const float a[3], const float b[3]) {
        float d = 0.0f;
        float la = 0.0f;
        float lb = 0.0f;
        for (int c = 0; c < 3; c++) {
            d += (a[c] - b[c]) * (a[c] - b[c]);
            la += a[c] * a[c];
            lb += b[c] * b[c];
        }
        return std::sqrt(d) > (BLINK * std::sqrt(std::fmax(la, lb)));
    }

    // The copy's number (the header says how it is chosen), or -1 where the part's 32 are all drawn in the frame.
    int32_t copy_number(uint32_t part_id, const float origin[3], int32_t frame) {
        auto found = copies.find(part_id);
        if (found == copies.end()) {
            found = copies.emplace(part_id, Copies{}).first;
            found->second.frame = frame - 2;
            found->second.walked = 1;
        }
        Copies& c = found->second;
        if (c.frame != frame) {
            bool last = (c.frame == (frame - 1));
            c.had = last ? c.taken : 0;
            c.had_stepped = last ? c.stepped : 0;
            for (int i = 0; i < COPIES; i++) {
                c.before[i] = c.now[i];
            }
            c.taken = 0;
            c.stepped = 0;
            c.frame = frame;
        }
        int32_t best = -1;
        bool matched = false;
        if ((c.walked <= 1) && ((c.had & ~1u) == 0)) {
            // A part drawn once, as it was the frame before: its one number, unless it moved a tenth of its distance
            // from the eye or more (a block that flies in, or that breaks and is put away), so it isn't drawn between.
            matched = ((c.had & 1u) != 0) && !(c.taken & 1u) && !blink_far(c.before[0].origin, origin);
            best = matched ? 0 : -1;
        }
        else {
            float reach = 0.0f;
            for (int k = 0; k < 3; k++) {
                reach += origin[k] * origin[k];
            }
            reach = std::sqrt(reach);
            float best_share = 0.0f;
            for (int i = 0; i < COPIES; i++) {
                if (!(c.had & (1u << i)) || (c.taken & (1u << i))) {
                    continue;
                }
                bool stepped = (c.had_stepped & (1u << i)) != 0;
                float d = 0.0f;
                for (int k = 0; k < 3; k++) {
                    float to = c.before[i].origin[k] + (stepped ? c.before[i].step[k] : 0.0f);
                    d += (origin[k] - to) * (origin[k] - to);
                }
                float share = std::sqrt(d) / ((stepped ? COPY_NEAR : COPY_NEW) * reach + 0.01f);
                if ((share <= 1.0f) && ((best < 0) || (share < best_share))) {
                    best = i;
                    best_share = share;
                }
            }
            matched = best >= 0;
        }
        // One no copy had the frame before, so a copy still to come can have its own, or else any left.
        for (int i = 0; (best < 0) && (i < COPIES); i++) {
            if (!((c.taken | c.had) & (1u << i))) {
                best = i;
            }
        }
        for (int i = 0; (best < 0) && (i < COPIES); i++) {
            if (!(c.taken & (1u << i))) {
                best = i;
            }
        }
        if (best >= 0) {
            c.taken |= 1u << best;
            for (int k = 0; k < 3; k++) {
                c.now[best].step[k] = matched ? (origin[k] - c.before[best].origin[k]) : 0.0f;
                c.now[best].origin[k] = origin[k];
            }
            c.stepped |= matched ? (1u << best) : 0u;
        }
        return best;
    }

    // Whether the ID's matrix is interpolated: unless it collapses or mirrors what it draws and didn't in the frame
    // before, or the other way round, or it turned more than 75 degrees since the frame before, unless that goes on
    // with its turn of the frame before (the header says how), or it blinked: it moved a tenth of its distance from the
    // eye or more since the frame before, back to its place of the frame before that. Whether it turned at all since
    // the frame before goes in turned, whether it turned more than 75 degrees, going on with its turn, in went_on, and
    // whether it moved 5 units or more since the frame before in moved.
    // IDs not drawn for a second are forgotten.
    bool interpolated(uint32_t id, const Pose& now, const float origin[3], int32_t frame, bool& paired, bool& turned,
                      bool& went_on, bool& moved) {
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
        turned = paired && turning(it->second.pose, now);
        went_on = false;
        moved = false;
        Seen next{ frame, now, { 0.0f, 0.0f, 0.0f }, { origin[0], origin[1], origin[2] }, 0, { 0.0f, 0.0f, 0.0f } };
        next.before_frame = (it != seen.end()) ? it->second.frame : (frame - 2);
        bool far = false;
        bool blinked = false;
        if (it != seen.end()) {
            for (int c = 0; c < 3; c++) {
                next.before[c] = it->second.origin[c];
            }
        }
        if (paired) {
            float d = 0.0f;
            for (int c = 0; c < 3; c++) {
                d += (origin[c] - it->second.origin[c]) * (origin[c] - it->second.origin[c]);
            }
            moved = d >= (MOVING * MOVING);
            blinked = (it->second.before_frame == frame - 2) && blink_far(it->second.origin, origin) &&
                      same_place(it->second.before, origin);
            float axis[3];
            float trace = turn_between(it->second.pose, now, axis);
            float length = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
            const float* spin = it->second.spin;
            bool going_on = (trace > FLIP_TRACE) && (length >= SPIN_AXIS) &&
                            ((axis[0] * spin[0] + axis[1] * spin[1] + axis[2] * spin[2]) >= (SPIN_SAME * length));
            far = (trace < TURN_TRACE) && !going_on;
            went_on = (trace < TURN_TRACE) && going_on;
            if (!far && (length >= SPIN_AXIS) && (it->second.pose.sign == now.sign)) {
                for (int c = 0; c < 3; c++) {
                    next.spin[c] = axis[c] / length;
                }
            }
        }
        bool same = (it == seen.end()) || ((it->second.pose.sign == now.sign) && !far && !blinked);
        seen[id] = next;
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
    walk_number++;
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
    if (part_walk[part] != walk_number) {
        part_walk[part] = walk_number;
        part_copies[part] = 0;
    }
    if (part_copies[part] >= COPIES) {
        queued[place] = 0;
        return;
    }
    part_copies[part]++;
    // The copy's number goes in as it is pushed (mp_model_push), by where it is drawn.
    queued[place] = ID_BASE | (walking_camera << CAMERA_SHIFT) |
                    ((slot_generation[walking] & (GENERATIONS - 1)) << GENERATION_SHIFT) |
                    ((uint32_t)walking << MODEL_SHIFT) | (uint32_t)part;
    if (copies.size() > SLOTS * 64) {
        for (auto it = copies.begin(); it != copies.end();) {
            it = ((walk_number - it->second.walk) > (uint32_t)(SLOTS * 60)) ? copies.erase(it) : std::next(it);
        }
    }
    Copies& c = copies[queued[place]];
    if (c.walk != walk_number) {
        if (c.walked == 0) {
            c.frame = -2;
        }
        c.walk = walk_number;
        c.walked = 0;
    }
    c.walked++;
}

// The group's two commands at the address: the part interpolated (its components AUTO, and its place INTERPOLATE where
// its camera turned or moved) or drawn at its own matrix.
static void group(uint8_t* rdram, gpr at, uint32_t id, bool keep, bool turned) {
    uint8_t component = keep ? G_EX_COMPONENT_AUTO : G_EX_COMPONENT_SKIP;
    uint8_t position = (keep && turned) ? G_EX_COMPONENT_INTERPOLATE : component;
    GfxCommand commands[2];
    gEXMatrixGroup(&commands[0], id, G_EX_INTERPOLATE_DECOMPOSE, G_EX_PUSH, MODELVIEW, position, component,
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
    Pose now = pose(rdram, mtx);
    float origin[3];
    for (int c = 0; c < 3; c++) {
        origin[c] = element(rdram, mtx, 12 + c);
    }
    int32_t copy = copy_number(queued[place], origin, frame);
    if (copy < 0) {
        return at;
    }
    uint32_t id = queued[place] | ((uint32_t)copy << COPY_SHIFT);
    uint32_t camera = camera_of(id);
    bool paired = false;
    bool turned = false;
    bool went_on = false;
    bool moved = false;
    bool keep = interpolated(id, now, origin, frame, paired, turned, went_on, moved) && !cut[camera];

    // A part drawn at its own matrix draws the parts at its place at theirs too (the header says why). A part that
    // collapses draws nothing, so it holds nobody back: Mario Party 3's dial keeps one part at 0 every frame.
    if (frame != placed_frame) {
        placed_count = 0;
        placed_frame = frame;
        for (int c = 0; c < CAMERAS; c++) {
            paired_parts[c] = 0;
            turned_parts[c] = 0;
            moved_parts[c] = 0;
            turning_camera[c] = false;
        }
    }
    Placed here;
    here.id = id;
    for (int c = 0; c < 3; c++) {
        here.origin[c] = origin[c];
    }
    here.own = (now.sign != 0) && (!keep || !paired);
    here.went_on = went_on && keep;
    // A part turned more than 75 degrees, going on with its turn, at the place of another model's part, is drawn at
    // its own matrix after all (the header says why), and so are the parts at its place, below.
    for (size_t i = 0; i < placed_count; i++) {
        Placed& other = placed[i];
        if ((camera_of(other.id) != camera) || ((other.id >> MODEL_SHIFT) == (id >> MODEL_SHIFT)) ||
            !same_place(other.origin, here.origin)) {
            continue;
        }
        if (here.went_on && !here.own) {
            here.own = true;
            keep = false;
        }
        if (other.went_on && !other.own) {
            other.own = true;
            other.keep = false;
            group(rdram, other.group, other.id, false, false);
        }
    }
    for (size_t i = 0; i < placed_count; i++) {
        Placed& other = placed[i];
        if ((camera_of(other.id) != camera) || (other.own == here.own) ||
            !same_place(other.origin, here.origin)) {
            continue;
        }
        if (here.own) {
            other.own = true;
            other.keep = false;
            group(rdram, other.group, other.id, false, false);
        }
        else {
            here.own = true;
            keep = false;
        }
    }

    // Whether the camera turned or moved, by most of its parts (the header says why), its parts tagged before this one
    // rewritten where that changed.
    if (keep && paired) {
        paired_parts[camera]++;
        turned_parts[camera] += turned ? 1 : 0;
        moved_parts[camera] += moved ? 1 : 0;
        bool turning_now = ((turned_parts[camera] * 2) > paired_parts[camera]) ||
                           ((moved_parts[camera] * 2) > paired_parts[camera]);
        if (turning_now != turning_camera[camera]) {
            turning_camera[camera] = turning_now;
            for (size_t i = 0; i < placed_count; i++) {
                Placed& other = placed[i];
                if ((camera_of(other.id) == camera) && other.keep) {
                    group(rdram, other.group, other.id, true, turning_now);
                }
            }
        }
    }

    GfxCommand enable;
    gEXEnable(&enable);
    MEM_W(0, at) = enable.values.word0;
    MEM_W(4, at) = enable.values.word1;
    at += 8;
    here.group = at;
    here.keep = keep;
    group(rdram, at, id, keep, turning_camera[camera]);
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

// A minigame's own effects, drawn outside the model walk, paired with their own between the game's frames the same way:
// each of its slots (an effect the minigame keeps in a table of its own, a puff or a spark, at its slot's matrix)
// tagged with an ID of the minigame and the slot. Untagged, RT64 paired the draws that look alike nearest first, and
// the number drawn changes from one frame to the next: Mario Party 1's Coin Block Bash, as puffs came and went, drew
// a small puff in the frames between stretched from a big one's matrix across the field. A slot is drawn at its own
// matrix where it wasn't drawn the frame before (the table hands an empty slot to a new effect), or where it moved more
// than the game's `far` from where its step of the frame before goes on to: the table hands a slot on to a new effect
// in the frame its effect ends, too. Coin Block Bash's puffs go at most 50 of its units a frame from where their step
// goes on to, and a slot handed on goes 70 or more (of 2,715 drawn in one play of it).
// - mp_effect_push just before the slot's gSPMatrix, with the list's next command, the slot's key (the game's, as the
//   minigame and the slot: under 2^24), the address of its place (three floats, the game's units), its `far` and the
//   frame's number: the tag at the list's next command, which it returns after itself. The game's table leaves room
//   for it in the list.
// - mp_effect_pop after the slot's display list, with the list's next command: the tag popped, and the command after.

namespace {
    constexpr uint32_t EFFECT_ID_BASE = 0x20000000;
    constexpr uint32_t EFFECT_KEYS = 1u << 24;

    struct Effect {
        int32_t frame = 0;
        float origin[3] = {};
        float step[3] = {};
    };
    std::unordered_map<uint32_t, Effect> effects;
    bool effect_pushed = false;

    float word_float(uint8_t* rdram, gpr at) {
        uint32_t word = (uint32_t)MEM_W(0, at);
        float value;
        memcpy(&value, &word, sizeof(value));
        return value;
    }
}

extern "C" gpr mp_effect_push(uint8_t* rdram, gpr at, uint32_t key, gpr place, float far, int32_t frame) {
    effect_pushed = false;
    if (key >= EFFECT_KEYS) {
        return at;
    }
    float origin[3];
    for (int c = 0; c < 3; c++) {
        origin[c] = word_float(rdram, place + c * 4);
    }
    auto found = effects.find(key);
    bool keep = false;
    Effect now;
    now.frame = frame;
    if ((found != effects.end()) && (found->second.frame == frame - 1)) {
        const Effect& before = found->second;
        float off = 0.0f;
        for (int c = 0; c < 3; c++) {
            now.step[c] = origin[c] - before.origin[c];
            float d = now.step[c] - before.step[c];
            off += d * d;
        }
        keep = off <= far * far;
        if (!keep) {
            for (int c = 0; c < 3; c++) {
                now.step[c] = 0.0f;
            }
        }
    }
    for (int c = 0; c < 3; c++) {
        now.origin[c] = origin[c];
    }
    effects[key] = now;

    GfxCommand enable;
    gEXEnable(&enable);
    MEM_W(0, at) = enable.values.word0;
    MEM_W(4, at) = enable.values.word1;
    at += 8;
    group(rdram, at, EFFECT_ID_BASE | key, keep, false);
    at += 16;
    effect_pushed = true;
    return at;
}

extern "C" gpr mp_effect_pop(uint8_t* rdram, gpr at) {
    if (!effect_pushed) {
        return at;
    }
    GfxCommand command;
    gEXPopMatrixGroup(&command, MODELVIEW);
    MEM_W(0, at) = command.values.word0;
    MEM_W(4, at) = command.values.word1;
    effect_pushed = false;
    return at + 8;
}
