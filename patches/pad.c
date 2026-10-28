#include "patches.h"
#include "engine_names.h"

// The controllers' setup (PAD_INIT, once at boot) has the SI thread (the engine's thread for the controllers and the
// EEPROM, which runs each function a request hands it) read the controllers at each retrace (PAD_AUTO_READ) before
// it makes the lock those reads take, a queue of one message (PAD_LOCK: osCreateMesgQueue, then the message that frees
// it). A retrace between the two has the SI thread wait on that queue before it is made, osCreateMesgQueue then empties
// the queue's list of waiters, as libultra's does, and the message that frees the lock wakes nobody: the SI thread
// waits for good, and every request to it with it, the game's included, at boot. The race is the game's own, in all
// three games, but on a console the setup reaches the lock microseconds after it starts the reads, while the runtime
// runs the setup's thread, the game's lowest in priority, only once every other has waited, and gives the SI thread
// the retrace at its next OS call: Mario Party 2's page stopped there in about one boot in 25 on a busy host, and in
// every boot with the setup made to wait 50 ms between the two. Here the lock is made first, and the rest is the
// game's.
RECOMP_PATCH s16 PAD_INIT(s16 ch, s32 flags) {
    PAD_REQUEST_T request;

    PAD_REQUEST(&request, (void*)PAD_INIT_SI, (void*)&ch, 1);
    PAD_AUTO_READING = 0;
    // @recomp The lock before the reads that take it, not after them.
    osCreateMesgQueue(&PAD_LOCK, (OSMesg*)&PAD_LOCK_MESG, 1);
    osSendMesg(&PAD_LOCK, NULL, OS_MESG_BLOCK);
    if (flags & 1) {
        PAD_AUTO_READ();
    }
    PAD_STICK_LIMIT(0x46, 0x46);
    return PAD_CONTROLLERS;
}
