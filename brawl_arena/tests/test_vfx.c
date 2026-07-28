#include "vfx.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static GameEvent Event(VfxEffectId effect, float size)
{
    return (GameEvent){
        .type = GAME_EVENT_VFX,
        .position = { 1.0f, 0.5f, 2.0f },
        .endPosition = { 1.0f, 0.5f, 5.0f },
        .color = { 90, 220, 255, 255 },
        .size = size,
        .vfxId = effect,
        .sourceBrawler = -1,
        .targetBrawler = -1
    };
}

int main(void)
{
    char message[128];
    CHECK(VFX_RENDER_SCALE == 4.0f,
          "imported VFX are not using the documented 4x readability scale");
    CHECK(VfxCatalogValidate(message, sizeof(message)), message);
    for (int effect = VFX_NONE + 1; effect < VFX_EFFECT_COUNT; effect++)
        CHECK(VfxCatalogGet((VfxEffectId)effect) != NULL,
              "a stable VFX ID has no presentation recipe");

    VfxLayerDefinition frameLayer = {
        .firstFrame = 3,
        .frameCount = 4,
        .framesPerSecond = 10.0f,
        .delay = 0.1f,
        .duration = 0.6f
    };
    CHECK(VfxFrameForLayer(&frameLayer, 0.05f) == -1,
          "delayed flipbook became visible too early");
    CHECK(VfxFrameForLayer(&frameLayer, 0.10f) == 3,
          "one-shot flipbook did not start on its first frame");
    CHECK(VfxFrameForLayer(&frameLayer, 0.80f) == 6,
          "one-shot flipbook did not clamp to its final frame");
    frameLayer.loop = true;
    CHECK(VfxFrameForLayer(&frameLayer, 0.50f) == 3,
          "looping flipbook did not wrap by frame count");

    PresentationState presentation;
    memset(&presentation, 0, sizeof(presentation));
    GameEvent mortar = Event(VFX_MORTAR_IMPACT, 2.6f);
    CHECK(VfxSpawnEvent(&presentation, &mortar) == 3,
          "Mortar impact did not expand into its three layers");
    CHECK(VfxActiveCount(&presentation) == 3,
          "spawned VFX layers are not active");
    VfxUpdate(&presentation, 0.2f);
    CHECK(VfxActiveCount(&presentation) == 3,
          "VFX layers expired before their authored lifetimes");
    VfxUpdate(&presentation, 4.0f);
    CHECK(VfxActiveCount(&presentation) == 0,
          "VFX layers did not expire");

    GameEvent attached = Event(VFX_TANK_RECLAIM, 1.0f);
    attached.targetBrawler = 2;
    attached.endSocket = VFX_SOCKET_CHEST;
    CHECK(VfxSpawnEvent(&presentation, &attached) == 2,
          "attached reclaim effect did not spawn");
    bool keptAttachment = false;
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
        if (presentation.vfx[i].active &&
            presentation.vfx[i].effect == VFX_TANK_RECLAIM &&
            presentation.vfx[i].targetBrawler == 2 &&
            presentation.vfx[i].endSocket == VFX_SOCKET_CHEST)
            keptAttachment = true;
    CHECK(keptAttachment,
          "VFX recipe expansion discarded semantic socket metadata");
    VfxUpdate(&presentation, 2.0f);

    GameEvent trail = Event(VFX_TANK_JETS_TRAIL, 0.8f);
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
        VfxSpawnEvent(&presentation, &trail);
    CHECK(VfxActiveCount(&presentation) == MAX_VFX_INSTANCES,
          "fixed VFX pool did not saturate at its declared capacity");

    GameEvent resonance = Event(VFX_GUARDIAN_RESONANCE_CAST, 1.4f);
    CHECK(VfxSpawnEvent(&presentation, &resonance) == 3,
          "high-priority cast could not replace cosmetic layers");
    CHECK(VfxActiveCount(&presentation) == MAX_VFX_INSTANCES,
          "pool replacement changed the fixed active capacity");
    bool foundResonance = false;
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
        if (presentation.vfx[i].active &&
            presentation.vfx[i].effect == VFX_GUARDIAN_RESONANCE_CAST)
            foundResonance = true;
    CHECK(foundResonance, "priority replacement discarded the important cast");

    printf("VFX tests passed: %s, %.1fx render scale, %d-instance fixed pool\n",
           message, VFX_RENDER_SCALE, MAX_VFX_INSTANCES);
    return 0;
}
