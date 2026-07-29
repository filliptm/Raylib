#include "studio.h"

#include "studio_session.h"
#include "attack_content.h"
#include "command_widgets.h"
#include "content_catalog.h"
#include "vfx_catalog.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

static StudioSession g_studio;      // loop configuration persists across visits
static float g_orbitYaw = PI;
static float g_orbitPitch = 0.75f;
static float g_orbitDist = 15.0f;

// Editor state
static int g_selectedLayer = 0;
static float g_draftTimer = -1.0f;  // >=0 counts down to a draft autosave
static char g_status[128];
static float g_statusTimer;

static const char *SLOT_NAMES[STUDIO_SLOT_COUNT] = { "MAIN", "SUPER", "SECONDARY" };
static const char *ANCHOR_LABELS[ATTACK_ANCHOR_COUNT] = {
    "Cast", "Self", "Projectile", "Impact"
};
static const char *PATTERN_LABELS[ATTACK_PATTERN_COUNT] = {
    "Single", "Burst", "Ring", "Cone"
};
static const char *BLEND_LABELS[ATTACK_BLEND_COUNT] = { "Alpha", "Additive" };
static const char *ATLAS_LABELS[] = {
    "Shapes", "Explosion", "Water", "Energy", "Air burst", "Divine", "Smoke", "?"
};

static void SetStatus(const char *message)
{
    TextCopy(g_status, message);
    g_statusTimer = 3.5f;
}

static void MarkDraftDirty(void)
{
    g_draftTimer = 0.8f;
}

static int SelectedAbilityIndex(const App *w)
{
    const CharacterDefinition *character =
        ContentCharacter(&w->content, g_studio.cls);
    if (!character) return -1;
    switch (g_studio.slot)
    {
        case STUDIO_SLOT_SUPER: return character->superAbility;
        case STUDIO_SLOT_SECONDARY: return character->mobilityAbility;
        default: return character->mainAbility;
    }
}

float StudioFrame(App *w, float realDt)
{
    // A match reset replaces the session; rebuild the stage whenever the session
    // is not ours. Loop configuration survives, so settings stick between visits.
    if (!StudioSessionActive(w)) StudioSessionEnter(w, &g_studio);

    // Orbit camera: right-drag rotates, wheel zooms (outside the panels).
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
    {
        Vector2 drag = GetMouseDelta();
        g_orbitYaw -= drag.x*0.008f;
        g_orbitPitch = Clamp(g_orbitPitch - drag.y*0.006f, 0.12f, 1.45f);
    }
    Vector2 mouse = GetMousePosition();
    bool inLeftPanel = mouse.x < 396.0f;
    bool inRightPanel = mouse.x > GetScreenWidth() - 396.0f;
    if (!inLeftPanel && !inRightPanel)
        g_orbitDist = Clamp(g_orbitDist - GetMouseWheelMove()*1.4f, 4.0f, 42.0f);

    float studioDt = StudioSessionTick(w, &g_studio, realDt);

    // Draft autosave, on the real clock so pause does not stall it.
    if (g_draftTimer >= 0.0f)
    {
        g_draftTimer -= realDt;
        if (g_draftTimer < 0.0f)
        {
            char message[160];
            if (AttackContentSaveFile(&w->content, ATTACK_DRAFT_PATH,
                                      message, (int)sizeof(message)))
                SetStatus("Draft saved.");
            else SetStatus(message);
        }
    }
    if (g_statusTimer > 0.0f) g_statusTimer -= realDt;

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

static void DrawStagePanel(App *w)
{
    Rectangle panel = { 16, 74, 372, 520 };
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
        g_selectedLayer = 0;
        StudioSessionEnter(w, &g_studio);
    }
    if (CommandUiCycler(&ui, "Ability", &g_studio.slot, STUDIO_SLOT_COUNT,
                        SLOT_NAMES))
        g_selectedLayer = 0;
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
    if (g_statusTimer > 0.0f) CommandUiText(&ui, g_status, COMMAND_ACCENT);
}

static void ColorEditor(CommandUi *ui, const char *label, Color *color)
{
    CommandUiText(ui, label, COMMAND_TEXT_DIM);
    int r = color->r, g = color->g, b = color->b, a = color->a;
    bool changed = false;
    changed |= CommandUiSliderI(ui, "R", &r, 0, 255);
    changed |= CommandUiSliderI(ui, "G", &g, 0, 255);
    changed |= CommandUiSliderI(ui, "B", &b, 0, 255);
    changed |= CommandUiSliderI(ui, "A", &a, 0, 255);
    if (changed)
    {
        *color = (Color){ (unsigned char)r, (unsigned char)g,
                          (unsigned char)b, (unsigned char)a };
        MarkDraftDirty();
    }
}

// Clickable atlas grid: pick a layer's first frame visually.
static void AtlasBrowser(CommandUi *ui, Assets *assets, AttackEffectLayer *layer)
{
    if (layer->atlas < 0 || layer->atlas >= VFX_ATLAS_COUNT) return;
    Texture2D texture = assets->vfxAtlases[layer->atlas];
    if (texture.id == 0)
    {
        CommandUiText(ui, "Atlas not loaded.", COMMAND_TEXT_DIM);
        return;
    }

    int columns = 1, rows = 1, frames = 1;
    VfxAtlasGrid((VfxAtlasId)layer->atlas, &columns, &rows, &frames);

    float scale = fminf((float)ui->width/texture.width, 200.0f/texture.height);
    Rectangle dest = { (float)ui->x, (float)ui->y,
                       texture.width*scale, texture.height*scale };
    DrawTexturePro(texture,
                   (Rectangle){ 0, 0, (float)texture.width, (float)texture.height },
                   dest, (Vector2){ 0, 0 }, 0.0f, WHITE);
    DrawRectangleLinesEx(dest, 1.0f, COMMAND_PANEL_EDGE);

    float cellW = dest.width/columns;
    float cellH = dest.height/rows;
    for (int c = 1; c < columns; c++)
        DrawLineV((Vector2){ dest.x + c*cellW, dest.y },
                  (Vector2){ dest.x + c*cellW, dest.y + dest.height },
                  (Color){ 255, 255, 255, 40 });
    for (int r = 1; r < rows; r++)
        DrawLineV((Vector2){ dest.x, dest.y + r*cellH },
                  (Vector2){ dest.x + dest.width, dest.y + r*cellH },
                  (Color){ 255, 255, 255, 40 });

    Rectangle current = { dest.x + (layer->frame%columns)*cellW,
                          dest.y + (layer->frame/columns)*cellH, cellW, cellH };
    DrawRectangleLinesEx(current, 2.0f, COMMAND_ACCENT);

    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointRec(mouse, dest))
    {
        int cx = (int)((mouse.x - dest.x)/cellW);
        int cz = (int)((mouse.y - dest.y)/cellH);
        int frame = cz*columns + cx;
        if (frame >= 0 && frame < frames)
        {
            layer->frame = frame;
            MarkDraftDirty();
        }
    }
    ui->y += (int)dest.height + 10;
}

static void LayerEditor(CommandUi *ui, Assets *assets, AttackEffectLayer *layer)
{
    bool changed = false;
    changed |= CommandUiCycler(ui, "Anchor", &layer->anchor,
                               ATTACK_ANCHOR_COUNT, ANCHOR_LABELS);
    changed |= CommandUiCycler(ui, "Atlas", &layer->atlas,
                               MAX_ATTACK_ATLASES, ATLAS_LABELS);
    AtlasBrowser(ui, assets, layer);
    changed |= CommandUiSliderI(ui, "First frame", &layer->frame, 0, 63);
    changed |= CommandUiSliderI(ui, "Frame count", &layer->frameCount, 1, 64);
    changed |= CommandUiSliderF(ui, "Flipbook fps", &layer->fps, 0.0f, 60.0f, "%.0f");
    changed |= CommandUiCycler(ui, "Pattern", &layer->pattern,
                               ATTACK_PATTERN_COUNT, PATTERN_LABELS);
    changed |= CommandUiSliderI(ui, "Count", &layer->count, 1, 32);
    changed |= CommandUiSliderF(ui, "Delay", &layer->delay, 0.0f, 2.0f, "%.2f");
    changed |= CommandUiSliderF(ui, "Lifetime", &layer->duration, 0.05f, 3.0f, "%.2f");
    changed |= CommandUiSliderF(ui, "Offset fwd", &layer->forward, -4.0f, 4.0f, "%.2f");
    changed |= CommandUiSliderF(ui, "Offset up", &layer->up, -2.0f, 4.0f, "%.2f");
    changed |= CommandUiSliderF(ui, "Offset side", &layer->side, -4.0f, 4.0f, "%.2f");
    changed |= CommandUiSliderF(ui, "Spread deg", &layer->spreadDeg, 0.0f, 360.0f, "%.0f");
    changed |= CommandUiSliderF(ui, "Speed", &layer->speed, -20.0f, 20.0f, "%.1f");
    changed |= CommandUiSliderF(ui, "Gravity", &layer->gravity, -30.0f, 30.0f, "%.1f");
    changed |= CommandUiSliderF(ui, "Drag", &layer->drag, 0.0f, 10.0f, "%.1f");
    changed |= CommandUiSliderF(ui, "Scale start", &layer->scaleStart, 0.0f, 6.0f, "%.2f");
    changed |= CommandUiSliderF(ui, "Scale end", &layer->scaleEnd, 0.0f, 6.0f, "%.2f");
    changed |= CommandUiCycler(ui, "Blend", &layer->blend,
                               ATTACK_BLEND_COUNT, BLEND_LABELS);
    changed |= CommandUiSliderF(ui, "Rotate deg/s", &layer->rotateSpeed,
                                -720.0f, 720.0f, "%.0f");
    changed |= CommandUiToggle(ui, "Ground quad", &layer->ground);
    if (changed) MarkDraftDirty();

    ColorEditor(ui, "COLOR START", &layer->colorStart);
    ColorEditor(ui, "COLOR END", &layer->colorEnd);
}

static void DrawEditorPanel(App *w, Assets *assets)
{
    float panelWidth = 372.0f;
    Rectangle panel = { GetScreenWidth() - panelWidth - 16.0f, 74.0f,
                        panelWidth, GetScreenHeight() - 100.0f };
    DrawRectangleRec(panel, COMMAND_PANEL_BG);
    DrawRectangleLinesEx(panel, 1.0f, COMMAND_PANEL_EDGE);

    // Simple wheel scrolling while the pointer is over the editor.
    static float scroll = 0.0f;
    if (CommandUiMouseIn(panel))
        scroll = Clamp(scroll - GetMouseWheelMove()*36.0f, 0.0f, 1400.0f);

    BeginScissorMode((int)panel.x, (int)panel.y, (int)panel.width,
                     (int)panel.height);

    CommandUi ui = { .world = w, .x = (int)panel.x + 14,
                     .y = (int)(panel.y + 12 - scroll),
                     .width = (int)panel.width - 28, .clip = panel };

    int abilityIndex = SelectedAbilityIndex(w);
    if (abilityIndex < 0 || abilityIndex >= w->content.abilityCount)
    {
        CommandUiSection(&ui, "ATTACK EDITOR");
        CommandUiText(&ui, "This kit has no ability in that slot.",
                      COMMAND_TEXT_DIM);
        EndScissorMode();
        return;
    }

    AttackPresentation *doc = &w->content.attacks[abilityIndex];
    char heading[96];
    TextCopy(heading, TextFormat("ATTACK EDITOR // %s",
                                 w->content.abilities[abilityIndex].id));
    CommandUiSection(&ui, heading);

    bool authored = doc->authored;
    if (CommandUiToggle(&ui, "Author this attack", &authored))
    {
        if (authored)
        {
            // First authoring starts from the behavior template; re-enabling an
            // existing document keeps its layers.
            bool hasContent = false;
            for (int i = 0; i < MAX_ATTACK_LAYERS; i++)
                if (doc->layers[i].used) hasContent = true;
            if (!hasContent)
                AttackPresentationTemplate(&w->content, abilityIndex, doc);
            doc->authored = true;
        }
        else doc->authored = false;
        MarkDraftDirty();
    }

    if (!doc->authored)
    {
        CommandUiText(&ui, "Legacy recipes drive this ability.", COMMAND_TEXT_DIM);
        CommandUiText(&ui, "Toggle authoring to start editing.", COMMAND_TEXT_DIM);
        EndScissorMode();
        return;
    }

    //--- Layer list -------------------------------------------------------------
    CommandUiSection(&ui, "LAYERS");
    for (int i = 0; i < MAX_ATTACK_LAYERS; i++)
    {
        if (!doc->layers[i].used) continue;
        const char *marker = (i == g_selectedLayer) ? "> " : "  ";
        if (CommandUiButton(&ui, TextFormat("%sLayer %d  [%s]", marker, i,
                                            ANCHOR_LABELS[doc->layers[i].anchor])))
            g_selectedLayer = i;
    }
    if (CommandUiButton(&ui, "+ Add layer"))
    {
        for (int i = 0; i < MAX_ATTACK_LAYERS; i++)
        {
            if (doc->layers[i].used) continue;
            AttackPresentation fresh;
            AttackPresentationTemplate(&w->content, abilityIndex, &fresh);
            doc->layers[i] = fresh.layers[0];
            g_selectedLayer = i;
            MarkDraftDirty();
            break;
        }
    }

    if (g_selectedLayer >= 0 && g_selectedLayer < MAX_ATTACK_LAYERS &&
        doc->layers[g_selectedLayer].used)
    {
        AttackEffectLayer *layer = &doc->layers[g_selectedLayer];
        CommandUiSection(&ui, TextFormat("LAYER %d", g_selectedLayer));
        if (CommandUiButton(&ui, "Duplicate layer"))
        {
            for (int i = 0; i < MAX_ATTACK_LAYERS; i++)
            {
                if (doc->layers[i].used) continue;
                doc->layers[i] = *layer;
                g_selectedLayer = i;
                MarkDraftDirty();
                break;
            }
        }
        if (CommandUiButton(&ui, "Delete layer"))
        {
            layer->used = false;
            MarkDraftDirty();
            for (int i = 0; i < MAX_ATTACK_LAYERS; i++)
                if (doc->layers[i].used) { g_selectedLayer = i; break; }
        }
        if (doc->layers[g_selectedLayer].used)
            LayerEditor(&ui, assets, &doc->layers[g_selectedLayer]);
    }

    //--- Motions ------------------------------------------------------------------
    CommandUiSection(&ui, "MOTIONS");
    static const char *MOTION_LABELS[ATTACK_MOTION_COUNT] = {
        "None", "Recoil", "Raise right", "Raise left", "Swing right",
        "Twist", "Slam", "Lean"
    };
    for (int i = 0; i < MAX_ATTACK_MOTIONS; i++)
    {
        AttackMotion *motion = &doc->motions[i];
        bool used = motion->used;
        if (CommandUiToggle(&ui, TextFormat("Motion %d", i), &used))
        {
            motion->used = used;
            if (used && motion->duration <= 0.0f)
                *motion = (AttackMotion){ .used = true,
                                          .kind = ATTACK_MOTION_RECOIL,
                                          .duration = 0.3f, .amplitude = 1.0f };
            MarkDraftDirty();
        }
        if (!motion->used) continue;
        bool changed = false;
        changed |= CommandUiCycler(&ui, "Kind", &motion->kind,
                                   ATTACK_MOTION_COUNT, MOTION_LABELS);
        changed |= CommandUiSliderF(&ui, "Delay", &motion->delay, 0.0f, 1.0f, "%.2f");
        changed |= CommandUiSliderF(&ui, "Duration", &motion->duration,
                                    0.05f, 1.5f, "%.2f");
        changed |= CommandUiSliderF(&ui, "Amplitude", &motion->amplitude,
                                    0.0f, 2.0f, "%.2f");
        if (changed) MarkDraftDirty();
    }

    //--- Projectile block ---------------------------------------------------------
    CommandUiSection(&ui, "PROJECTILE");
    AttackProjectileVisual *pv = &doc->projectile;
    bool projectileChanged = false;
    projectileChanged |= CommandUiToggle(&ui, "Hide projectile body", &pv->hideBody);
    projectileChanged |= CommandUiToggle(&ui, "Tint override", &pv->tintOverride);
    if (pv->tintOverride) ColorEditor(&ui, "TINT", &pv->tint);
    projectileChanged |= CommandUiSliderF(&ui, "Glow", &pv->glow, 0.0f, 4.0f, "%.2f");
    projectileChanged |= CommandUiSliderF(&ui, "Visual scale", &pv->visualScale,
                                          0.2f, 4.0f, "%.2f");
    projectileChanged |= CommandUiSliderF(&ui, "Spin rev/s", &pv->spin,
                                          -10.0f, 10.0f, "%.1f");
    projectileChanged |= CommandUiSliderF(&ui, "Trail seconds", &pv->trailLength,
                                          0.0f, 1.0f, "%.2f");
    if (projectileChanged) MarkDraftDirty();

    //--- Save / promote -----------------------------------------------------------
    CommandUiSection(&ui, "SAVE");
    CommandUiText(&ui, "Edits autosave to the local draft.", COMMAND_TEXT_DIM);
    if (CommandUiButton(&ui, "SAVE AS PROJECT DEFAULT"))
    {
        char message[160];
        if (AttackContentSaveFile(&w->content, ATTACK_PRESENTATION_PATH,
                                  message, (int)sizeof(message)))
            SetStatus("Attack presentation saved to project.");
        else SetStatus(message);
    }
    if (CommandUiButton(&ui, "Rebuild + reload atlases"))
    {
        // Runs the curated CC0 pipeline, then re-imports the textures without a
        // restart, so new source art shows up in the browser immediately.
        int result = system("python3 tools/build_vfx_assets.py "
                            "--manifest data/vfx/asset_manifest.json");
        AssetsReloadVfxAtlases(assets);
        SetStatus(result == 0 ? "Atlases rebuilt and reloaded."
                              : "Atlas rebuild failed; see console.");
    }
    if (CommandUiButton(&ui, "Revert to project file"))
    {
        char message[160];
        AttackContentDefaults(&w->content);
        if (!AttackContentLoadFile(&w->content, ATTACK_PRESENTATION_PATH,
                                   message, (int)sizeof(message)))
            SetStatus(message);
        else SetStatus("Reverted to the tracked project documents.");
        MarkDraftDirty();
    }

    EndScissorMode();
}

// Bottom timeline: one row per used layer, block position = delay, width =
// lifetime. Dragging a block slides its delay; the playhead tracks the loop.
static void DrawTimeline(App *w)
{
    int abilityIndex = SelectedAbilityIndex(w);
    if (abilityIndex < 0 || abilityIndex >= w->content.abilityCount) return;
    AttackPresentation *doc = &w->content.attacks[abilityIndex];
    if (!doc->authored) return;

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    Rectangle panel = { 404.0f, screenH - 150.0f, screenW - 808.0f, 134.0f };
    if (panel.width < 320.0f) return;

    DrawRectangleRec(panel, COMMAND_PANEL_BG);
    DrawRectangleLinesEx(panel, 1.0f, COMMAND_PANEL_EDGE);
    DrawText("TIMELINE  (drag a block to move its delay)",
             (int)panel.x + 10, (int)panel.y + 6, 10, COMMAND_TEXT_DIM);

    const float window = 2.0f;
    float originX = panel.x + 10.0f;
    float pxPerSec = (panel.width - 20.0f)/window;
    for (int tick = 0; tick <= 4; tick++)
    {
        float x = originX + tick*0.5f*pxPerSec;
        DrawLineV((Vector2){ x, panel.y + 20.0f },
                  (Vector2){ x, panel.y + panel.height - 6.0f },
                  (Color){ 255, 255, 255, 26 });
        DrawText(TextFormat("%.1fs", tick*0.5f), (int)x + 2,
                 (int)panel.y + 20, 10, COMMAND_TEXT_DIM);
    }

    static const Color ANCHOR_COLORS[ATTACK_ANCHOR_COUNT] = {
        { 255, 196, 92, 255 },   // cast
        { 120, 200, 255, 255 },  // self
        { 190, 140, 255, 255 },  // projectile
        { 255, 120, 140, 255 }   // impact
    };

    static int dragLayer = -1;
    static float dragGrab = 0.0f;
    Vector2 mouse = GetMousePosition();

    int row = 0;
    for (int i = 0; i < MAX_ATTACK_LAYERS; i++)
    {
        AttackEffectLayer *layer = &doc->layers[i];
        if (!layer->used) continue;
        float y = panel.y + 36.0f + row*11.0f;
        Rectangle block = { originX + layer->delay*pxPerSec, y,
                            fmaxf(layer->duration*pxPerSec, 6.0f), 9.0f };
        if (block.x + block.width > panel.x + panel.width - 8.0f)
            block.width = panel.x + panel.width - 8.0f - block.x;

        Color color = ANCHOR_COLORS[layer->anchor];
        if (i != g_selectedLayer) color.a = 150;
        DrawRectangleRec(block, color);
        if (i == g_selectedLayer)
            DrawRectangleLinesEx((Rectangle){ block.x - 1, block.y - 1,
                                              block.width + 2, block.height + 2 },
                                 1.0f, COMMAND_ACCENT);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(mouse, block))
        {
            g_selectedLayer = i;
            dragLayer = i;
            dragGrab = (mouse.x - originX)/pxPerSec - layer->delay;
        }
        row++;
    }

    if (dragLayer >= 0)
    {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            dragLayer = -1;
            MarkDraftDirty();
        }
        else if (doc->layers[dragLayer].used)
        {
            float delay = (mouse.x - originX)/pxPerSec - dragGrab;
            doc->layers[dragLayer].delay = Clamp(delay, 0.0f, window);
        }
    }

    // Playhead: seconds since the last cast, wrapped by the loop interval.
    float age = g_studio.interval - g_studio.castTimer;
    if (age >= 0.0f && age <= window)
    {
        float x = originX + age*pxPerSec;
        DrawLineV((Vector2){ x, panel.y + 20.0f },
                  (Vector2){ x, panel.y + panel.height - 6.0f },
                  (Color){ 255, 255, 255, 170 });
    }
}

void StudioDraw(App *w, Assets *assets)
{
    DrawStagePanel(w);
    DrawEditorPanel(w, assets);
    DrawTimeline(w);
}
