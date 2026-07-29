#ifndef STUDIO_SESSION_H
#define STUDIO_SESSION_H

#include "app_types.h"

// The VFX Studio's simulation core: a tiny real GameSession on an open stage with
// one character and a target dummy, casting the selected ability on a metronome.
// Everything here is headless-testable; the devtools studio screen wraps it with
// a camera and panels.

typedef enum {
    STUDIO_SLOT_MAIN = 0,
    STUDIO_SLOT_SUPER,
    STUDIO_SLOT_SECONDARY,
    STUDIO_SLOT_COUNT
} StudioSlot;

typedef struct StudioSession {
    BrawlerClass cls;
    int slot;               // StudioSlot
    float interval;         // seconds between casts (also the zero-init sentinel)
    float castTimer;
    float dummyDistance;
    bool dummyEnabled;
    bool paused;
    float timeScale;
    float pendingStep;      // one queued simulated step while paused
} StudioSession;

// Rebuilds session/controller/presentation state onto the studio stage.
void StudioSessionEnter(App *app, StudioSession *studio);

// True while the app's session is the studio stage (a match reset replaces it).
bool StudioSessionActive(const App *app);

// Advances the stage by one rendered frame and returns the simulated dt
// (0 while paused with no step queued). The caller drives presentation
// consumers (effects, character animation) with the returned dt.
float StudioSessionTick(App *app, StudioSession *studio, float realDt);

#endif
