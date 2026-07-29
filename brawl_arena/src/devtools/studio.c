#include "studio.h"

#include "studio_session.h"
#include "command_widgets.h"
#include "content_catalog.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>

static StudioSession g_studio;      // loop configuration persists across visits
static float g_orbitYaw = PI;
static float g_orbitPitch = 0.75f;
static float g_orbitDist = 15.0f;

static const char *SLOT_NAMES[STUDIO_SLOT_COUNT] = { "MAIN", "SUPER", "SECONDARY" };

float StudioFrame(App *w, float realDt)
{
    // A match reset replaces the session; rebuild the stage whenever the session
    // is not ours. Loop configuration survives, so settings stick between visits.
    if (!StudioSessionActive(w)) StudioSessionEnter(w, &g_studio);

    // Orbit camera: right-drag rotates, wheel zooms (outside the panel).
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
    {
        Vector2 drag = GetMouseDelta();
        g_orbitYaw -= drag.x*0.008f;
        g_orbitPitch = Clamp(g_orbitPitch - drag.y*0.006f, 0.12f, 1.45f);
    }
    if (!CommandUiMouseIn((Rectangle){ 16, 74, 372, 640 }))
        g_orbitDist = Clamp(g_orbitDist - GetMouseWheelMove()*1.4f, 4.0f, 42.0f);

    float studioDt = StudioSessionTick(w, &g_studio, realDt);

    // Frame the midpoint between the character and the dummy.
    float mid = g_studio.dummyEnabled ? g_studio.dummyDistance*0.4f : 1.5f;
    Vector3 focus = { 0.0f, 0.9f, mid };
    Camera3D *camera = &w->presentation.camera;
    camera->position = (Vector3){
        focus.x + sinf(g_orbitYaw)*cosf(g_orbitPitch)*g_orbitDist,
        focus.y + sinf(g_orbitPitch)*g_orbitDist,
        focus.z + cosf(g_orbitYaw)*cosf(g_orbitPitch)*g_orbitDist
    };
    camera->target = focus;
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera->fovy = 46.0f;
    camera->projection = CAMERA_PERSPECTIVE;

    return studioDt;
}

void StudioDraw(App *w)
{
    Rectangle panel = { 16, 74, 372, 540 };
    DrawRectangleRec(panel, COMMAND_PANEL_BG);
    DrawRectangleLinesEx(panel, 1.0f, COMMAND_PANEL_EDGE);

    CommandUi ui = { .world = w, .x = (int)panel.x + 14,
                     .y = (int)panel.y + 12, .width = (int)panel.width - 28,
                     .clip = panel };

    CommandUiSection(&ui, "VFX STUDIO");

    int cls = (int)g_studio.cls;
    if (CommandUiCycler(&ui, "Character", &cls, CLASS_COUNT, CLASS_NAMES))
    {
        g_studio.cls = (BrawlerClass)cls;
        StudioSessionEnter(w, &g_studio);
    }
    CommandUiCycler(&ui, "Ability", &g_studio.slot, STUDIO_SLOT_COUNT, SLOT_NAMES);
    CommandUiSliderF(&ui, "Loop interval", &g_studio.interval, 0.3f, 5.0f, "%.1f");

    bool dummy = g_studio.dummyEnabled;
    if (CommandUiToggle(&ui, "Target dummy", &dummy))
    {
        g_studio.dummyEnabled = dummy;
        StudioSessionEnter(w, &g_studio);
    }
    CommandUiSliderF(&ui, "Dummy range", &g_studio.dummyDistance, 3.0f, 20.0f, "%.1f");

    CommandUiSection(&ui, "TIME");
    CommandUiSliderF(&ui, "Speed", &g_studio.timeScale, 0.05f, 1.0f, "%.2f");
    CommandUiToggle(&ui, "Pause", &g_studio.paused);
    if (CommandUiButton(&ui, "Step one frame"))
    {
        g_studio.paused = true;
        g_studio.pendingStep = 1.0f/60.0f;
    }
    if (CommandUiButton(&ui, "Restart loop")) StudioSessionEnter(w, &g_studio);

    CommandUiSection(&ui, "");
    CommandUiText(&ui, "Right-drag orbits. Wheel zooms.", COMMAND_TEXT_DIM);
    CommandUiText(&ui, "ESC returns to the menu.", COMMAND_TEXT_DIM);
}
