#include "ui_system.h"

#include "raymath.h"
#include <float.h>
#include <math.h>
#include <string.h>

static UiSystem *g_ui = NULL;

static Font LoadUiFont(const char *path, int size, bool *owned, bool *fallback)
{
    int codepoints[96];
    for (int i = 0; i < 95; i++) codepoints[i] = 32 + i;
    codepoints[95] = 0x2014;
    Font font = LoadFontEx(path, size, codepoints, 96);
    if (font.texture.id > 0 && font.glyphCount > 0)
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
        *owned = true;
        return font;
    }
    TraceLog(LOG_WARNING, "UI: could not load %s; using raylib fallback font", path);
    *owned = false;
    *fallback = true;
    return GetFontDefault();
}

bool UiSystemLoad(UiSystem *ui)
{
    memset(ui, 0, sizeof(*ui));
    ui->theme = UiThemeArenaInk();
    ui->resources.display = LoadUiFont("resources/fonts/BarlowCondensed-Bold.ttf", 96,
                                       &ui->resources.displayOwned, &ui->fontFallback);
    ui->resources.body = LoadUiFont("resources/fonts/Barlow-Regular.ttf", 64,
                                    &ui->resources.bodyOwned, &ui->fontFallback);
    ui->resources.emphasis = LoadUiFont("resources/fonts/Barlow-SemiBold.ttf", 64,
                                        &ui->resources.emphasisOwned, &ui->fontFallback);
    ui->resources.data = LoadUiFont("resources/fonts/IBMPlexMono-Medium.ttf", 64,
                                    &ui->resources.dataOwned, &ui->fontFallback);
    if (!UiSkinLoad(&ui->skin))
        TraceLog(LOG_WARNING, "UI: procedural skin unavailable; using safe geometry");
    ui->modality = UI_INPUT_POINTER;
    ui->interactionsEnabled = true;
    ui->previousMouse = GetMousePosition();
    UiSystemSetActive(ui);
    return !ui->fontFallback;
}

void UiSystemUnload(UiSystem *ui)
{
    UiSkinUnload(&ui->skin);
    if (ui->resources.displayOwned) UnloadFont(ui->resources.display);
    if (ui->resources.bodyOwned) UnloadFont(ui->resources.body);
    if (ui->resources.emphasisOwned) UnloadFont(ui->resources.emphasis);
    if (ui->resources.dataOwned) UnloadFont(ui->resources.data);
    if (g_ui == ui) g_ui = NULL;
    memset(ui, 0, sizeof(*ui));
}

void UiSystemSetActive(UiSystem *ui) { g_ui = ui; }
UiSystem *UiSystemActive(void) { return g_ui; }
void UiSetInteractionsEnabled(bool enabled)
{
    if (g_ui) g_ui->interactionsEnabled = enabled;
}

float UiReferenceScaleForViewport(int width, int height, float preferenceScale)
{
    float fit = fminf(width/UI_REFERENCE_WIDTH, height/UI_REFERENCE_HEIGHT);
    float preference = Clamp(preferenceScale, 0.75f, 1.50f);
    // At larger preference scales preserve the safe frame instead of clipping the
    // reference canvas. Individual text/components grow within the fitted layout.
    return fit*fminf(preference, 1.0f);
}

Rectangle UiReferenceSafeRect(int width, int height, float preferenceScale)
{
    return UiReferenceSafeRectWithInsets(
        width, height, preferenceScale, (UiViewportInsets){ 0 });
}

Rectangle UiReferenceSafeRectWithInsets(int width, int height,
                                        float preferenceScale,
                                        UiViewportInsets insets)
{
    float availableWidth = fmaxf(1.0f, width - fmaxf(0.0f, insets.left) -
                                        fmaxf(0.0f, insets.right));
    float availableHeight = fmaxf(1.0f, height - fmaxf(0.0f, insets.top) -
                                         fmaxf(0.0f, insets.bottom));
    float scale = UiReferenceScaleForViewport(
        (int)availableWidth, (int)availableHeight, preferenceScale);
    float refW = UI_REFERENCE_WIDTH*scale;
    float refH = UI_REFERENCE_HEIGHT*scale;
    float ox = fmaxf(0.0f, insets.left) + (availableWidth - refW)*0.5f;
    float oy = fmaxf(0.0f, insets.top) + (availableHeight - refH)*0.5f;
    float pad = 24.0f*scale;
    return (Rectangle){ ox + pad, oy + pad, refW - pad*2.0f, refH - pad*2.0f };
}

Rectangle UiTouchTargetBounds(Rectangle bounds, float minimumSize)
{
    float width = fmaxf(bounds.width, minimumSize);
    float height = fmaxf(bounds.height, minimumSize);
    return (Rectangle){
        bounds.x - (width - bounds.width)*0.5f,
        bounds.y - (height - bounds.height)*0.5f,
        width, height
    };
}

static int AnyGamepadAvailable(void)
{
    for (int gamepad = 0; gamepad < 4; gamepad++)
        if (IsGamepadAvailable(gamepad)) return gamepad;
    return -1;
}

static bool GamepadPressed(int gamepad, int button)
{
    return gamepad >= 0 && IsGamepadButtonPressed(gamepad, button);
}

static int FindNode(const UiFocusNode *nodes, int count, UiId id)
{
    for (int i = 0; i < count; i++)
        if (nodes[i].id == id && nodes[i].enabled) return i;
    return -1;
}

UiId UiFocusNeighbor(const UiFocusNode *nodes, int count, UiId currentId, int dx, int dy)
{
    if (!nodes || count <= 0) return 0;
    int current = FindNode(nodes, count, currentId);
    if (current < 0)
        return nodes[0].id;

    Vector2 from = {
        nodes[current].bounds.x + nodes[current].bounds.width*0.5f,
        nodes[current].bounds.y + nodes[current].bounds.height*0.5f
    };
    float bestScore = FLT_MAX;
    int best = current;
    for (int i = 0; i < count; i++)
    {
        if (i == current || !nodes[i].enabled) continue;
        Vector2 to = {
            nodes[i].bounds.x + nodes[i].bounds.width*0.5f,
            nodes[i].bounds.y + nodes[i].bounds.height*0.5f
        };
        float x = to.x - from.x;
        float y = to.y - from.y;
        if ((dx < 0 && x >= -1.0f) || (dx > 0 && x <= 1.0f) ||
            (dy < 0 && y >= -1.0f) || (dy > 0 && y <= 1.0f)) continue;
        float primary = dx != 0 ? fabsf(x) : fabsf(y);
        float secondary = dx != 0 ? fabsf(y) : fabsf(x);
        float score = primary + secondary*2.2f;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    return nodes[best].id;
}

static void MoveFocus(UiSystem *ui, int dx, int dy)
{
    UiId next = UiFocusNeighbor(ui->previousNodes, ui->previousNodeCount,
                                ui->focused, dx, dy);
    if (next && next != ui->focused)
        ui->focused = next;
}

float UiMotionDuration(float normalDuration, bool reducedMotion)
{
    return reducedMotion ? 0.0f : fmaxf(0.0f, normalDuration);
}

float UiEaseOutCubic(float value)
{
    float t = Clamp(value, 0.0f, 1.0f);
    float inverse = 1.0f - t;
    return 1.0f - inverse*inverse*inverse;
}

float UiEaseOutBack(float value)
{
    float t = Clamp(value, 0.0f, 1.0f) - 1.0f;
    const float overshoot = 1.70158f;
    return 1.0f + (overshoot + 1.0f)*t*t*t + overshoot*t*t;
}

float UiMotionProgress(float age, float delay, float duration, bool reducedMotion)
{
    if (reducedMotion || duration <= 0.0f) return 1.0f;
    return Clamp((age - fmaxf(delay, 0.0f))/duration, 0.0f, 1.0f);
}

void UiSystemBeginFrame(UiSystem *ui, const UiPreferences *preferences,
                        int width, int height, float dt)
{
    UiSystemSetActive(ui);
    ui->previousNodeCount = ui->nodeCount;
    memcpy(ui->previousNodes, ui->nodes,
           sizeof(UiFocusNode)*(size_t)ui->previousNodeCount);
    ui->nodeCount = 0;
    ui->interactionsEnabled = true;
    ui->focusOverflow = false;
    ui->frameDt = fmaxf(0.0f, dt);
    ui->elapsed += ui->frameDt;

    float prefScale = preferences ? preferences->scale : 1.0f;
    ui->glyphMode = preferences ? preferences->inputGlyphMode : UI_GLYPH_AUTO;
    ui->layout.viewportWidth = width;
    ui->layout.viewportHeight = height;
    float availableWidth = fmaxf(
        1.0f, width - fmaxf(0.0f, ui->insets.left) -
                    fmaxf(0.0f, ui->insets.right));
    float availableHeight = fmaxf(
        1.0f, height - fmaxf(0.0f, ui->insets.top) -
                    fmaxf(0.0f, ui->insets.bottom));
    ui->layout.viewportScale = fminf(
        availableWidth/UI_REFERENCE_WIDTH,
        availableHeight/UI_REFERENCE_HEIGHT);
    ui->layout.preferenceScale = Clamp(prefScale, 0.75f, 1.50f);
    ui->layout.scale = ui->layout.viewportScale*
        fminf(ui->layout.preferenceScale, 1.0f);
    ui->layout.origin = (Vector2){
        fmaxf(0.0f, ui->insets.left) +
            (availableWidth - UI_REFERENCE_WIDTH*ui->layout.scale)*0.5f,
        fmaxf(0.0f, ui->insets.top) +
            (availableHeight - UI_REFERENCE_HEIGHT*ui->layout.scale)*0.5f
    };
    ui->layout.safe = UiReferenceSafeRectWithInsets(
        width, height, prefScale, ui->insets);
    ui->layout.content = (Rectangle){
        ui->layout.origin.x, ui->layout.origin.y,
        UI_REFERENCE_WIDTH*ui->layout.scale, UI_REFERENCE_HEIGHT*ui->layout.scale
    };
    ui->reducedMotion = preferences && preferences->reducedMotion;

    int gamepad = AnyGamepadAvailable();
#if defined(BRAWL_MOBILE)
    ui->modality = UI_INPUT_TOUCH;
    ui->focusVisible = false;
#else
    Vector2 mouse = GetMousePosition();
    if (Vector2Distance(mouse, ui->previousMouse) > 1.5f || GetMouseWheelMove() != 0.0f)
    {
        ui->modality = UI_INPUT_POINTER;
        ui->focusVisible = false;
    }
    ui->previousMouse = mouse;

    bool keyNav = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                  IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) ||
                  IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) ||
                  IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S);
    bool padNav = GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT) ||
                  GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) ||
                  GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP) ||
                  GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    if (keyNav || padNav)
    {
        ui->modality = padNav ? UI_INPUT_GAMEPAD : UI_INPUT_KEYBOARD;
        ui->focusVisible = true;
    }
#endif

    ui->navigationX = ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) ||
                       GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) ? 1 : 0) -
                      ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) ||
                       GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) ? 1 : 0);
    ui->navigationY = ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) ||
                       GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) ? 1 : 0) -
                      ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
                       GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP)) ? 1 : 0);
    if (ui->navigationX != 0) MoveFocus(ui, ui->navigationX, 0);
    else if (ui->navigationY != 0) MoveFocus(ui, 0, ui->navigationY);

    ui->activatePressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        GamepadPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    ui->backPressed = GamepadPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    ui->previousPressed = IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_PAGE_UP) ||
        GamepadPressed(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    ui->nextPressed = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_PAGE_DOWN) ||
        GamepadPressed(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
}

void UiSystemSetViewportInsets(UiSystem *ui, UiViewportInsets insets)
{
    if (!ui) return;
    ui->insets.top = fmaxf(0.0f, insets.top);
    ui->insets.left = fmaxf(0.0f, insets.left);
    ui->insets.bottom = fmaxf(0.0f, insets.bottom);
    ui->insets.right = fmaxf(0.0f, insets.right);
}

void UiSystemEndFrame(UiSystem *ui)
{
    if (ui->focused != 0 && FindNode(ui->nodes, ui->nodeCount, ui->focused) < 0)
        ui->focused = ui->nodeCount > 0 ? ui->nodes[0].id : 0;
    if (ui->focusOverflow)
        TraceLog(LOG_WARNING, "UI: focus-node capacity (%d) exceeded",
                 UI_MAX_FOCUS_NODES);
}

Rectangle UiRefRect(float x, float y, float width, float height)
{
    if (!g_ui) return (Rectangle){ x, y, width, height };
    return (Rectangle){
        g_ui->layout.origin.x + x*g_ui->layout.scale,
        g_ui->layout.origin.y + y*g_ui->layout.scale,
        width*g_ui->layout.scale,
        height*g_ui->layout.scale
    };
}

Vector2 UiRefPoint(float x, float y)
{
    Rectangle r = UiRefRect(x, y, 0, 0);
    return (Vector2){ r.x, r.y };
}

float UiScale(float value)
{
    return g_ui ? value*g_ui->layout.scale : value;
}

UiId UiHash(const char *text)
{
    UiId hash = 2166136261u;
    while (*text)
    {
        hash ^= (unsigned char)*text++;
        hash *= 16777619u;
    }
    return hash ? hash : 1u;
}

void UiFocus(UiId id)
{
    if (g_ui) { g_ui->focused = id; g_ui->focusVisible = true; }
}

bool UiBackPressed(void) { return g_ui && g_ui->backPressed; }
UiInputModality UiCurrentModality(void) { return g_ui ? g_ui->modality : UI_INPUT_POINTER; }

const char *UiBindingLabel(const char *keyboardMouse, const char *gamepad,
                           const char *touch)
{
    if (!g_ui) return keyboardMouse;
    if (g_ui->glyphMode == UI_GLYPH_KEYBOARD_MOUSE) return keyboardMouse;
    if (g_ui->glyphMode == UI_GLYPH_GAMEPAD) return gamepad;
    if (g_ui->modality == UI_INPUT_TOUCH) return touch;
    return g_ui->modality == UI_INPUT_GAMEPAD ? gamepad : keyboardMouse;
}

static Font FontForRole(UiTextRole role)
{
    if (!g_ui) return GetFontDefault();
    if (role == UI_TEXT_DISPLAY || role == UI_TEXT_TITLE ||
        role == UI_TEXT_HEADING || role == UI_TEXT_RESULT)
        return g_ui->resources.display;
    if (role == UI_TEXT_EMPHASIS || role == UI_TEXT_LABEL)
        return g_ui->resources.emphasis;
    if (role == UI_TEXT_CAPTION || role == UI_TEXT_DATA)
        return g_ui->resources.data;
    return g_ui->resources.body;
}

float UiTextSize(UiTextRole role)
{
    static const float sizes[UI_TEXT_ROLE_COUNT] = {
        68, 46, 30, 18, 18, 14, 12, 16, 82
    };
    float fit = g_ui ? g_ui->layout.viewportScale : 1.0f;
    float preference = g_ui ? g_ui->layout.preferenceScale : 1.0f;
    return sizes[role]*fit*preference;
}

Vector2 UiMeasureText(UiTextRole role, const char *text)
{
    float size = UiTextSize(role);
    return MeasureTextEx(FontForRole(role), text, size, size*0.025f);
}

void UiDrawText(UiTextRole role, const char *text, Vector2 position, Color color)
{
    float size = UiTextSize(role);
    DrawTextEx(FontForRole(role), text, position, size, size*0.025f, color);
}

void UiDrawTextAligned(UiTextRole role, const char *text, Rectangle bounds,
                       UiAlign align, Color color)
{
    Vector2 measure = UiMeasureText(role, text);
    Vector2 position = { bounds.x, bounds.y + (bounds.height - measure.y)*0.5f };
    if (align == UI_ALIGN_CENTER) position.x += (bounds.width - measure.x)*0.5f;
    else if (align == UI_ALIGN_RIGHT) position.x += bounds.width - measure.x;
    UiDrawText(role, text, position, color);
}

void UiDrawTextFit(UiTextRole role, const char *text, Rectangle bounds,
                   UiAlign align, Color color)
{
    Font font = FontForRole(role);
    float size = UiTextSize(role);
    float minimum = 11.0f*(g_ui ? g_ui->layout.viewportScale : 1.0f);
    Vector2 measure = MeasureTextEx(font, text, size, size*0.025f);
    if (measure.x > bounds.width)
    {
        // Text width scales near-linearly with font size, so one proportional step
        // (plus a single verify-and-correct pass) replaces the old 1 px-at-a-time
        // shrink loop that re-measured every fitted string every frame.
        size = fmaxf(size*bounds.width/measure.x, minimum);
        measure = MeasureTextEx(font, text, size, size*0.025f);
        while (measure.x > bounds.width && size > minimum)
        {
            size -= 1.0f;
            measure = MeasureTextEx(font, text, size, size*0.025f);
        }
    }
    Vector2 position = { bounds.x, bounds.y + (bounds.height - measure.y)*0.5f };
    if (align == UI_ALIGN_CENTER) position.x += (bounds.width - measure.x)*0.5f;
    else if (align == UI_ALIGN_RIGHT) position.x += bounds.width - measure.x;
    DrawTextEx(font, text, position, size, size*0.025f, color);
}

void UiDrawTextShadow(UiTextRole role, const char *text, Vector2 position, Color color)
{
    Vector2 shadow = Vector2Add(position, (Vector2){ UiScale(2), UiScale(2) });
    UiDrawText(role, text, shadow, g_ui ? g_ui->theme->shadow : BLACK);
    UiDrawText(role, text, position, color);
}

static void DrawTextOutlineRaw(Font font, const char *text, Vector2 position,
                               float size, float spacing, Color fill,
                               Color outline, float thickness)
{
    static const Vector2 directions[] = {
        { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 },
        { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 }
    };
    for (int ring = (int)ceilf(thickness); ring >= 1; ring--)
    {
        float distance = fminf(thickness, (float)ring);
        for (int i = 0; i < 8; i++)
            DrawTextEx(font, text,
                       (Vector2){ position.x + directions[i].x*distance,
                                  position.y + directions[i].y*distance },
                       size, spacing, outline);
    }
    DrawTextEx(font, text, position, size, spacing, fill);
}

void UiDrawTextOutline(UiTextRole role, const char *text, Vector2 position,
                       Color fill, Color outline, float thickness)
{
    float size = UiTextSize(role);
    DrawTextOutlineRaw(FontForRole(role), text, position, size, size*0.025f,
                       fill, outline, UiScale(thickness));
}

static void ChamferPoints(Rectangle r, float cut, Vector2 points[6])
{
    // raylib's triangle helpers require counter-clockwise screen-space points.
    // Clockwise winding leaves the outline visible but culls the entire panel fill.
    points[0] = (Vector2){ r.x + cut, r.y };
    points[1] = (Vector2){ r.x, r.y + cut };
    points[2] = (Vector2){ r.x, r.y + r.height };
    points[3] = (Vector2){ r.x + r.width - cut, r.y + r.height };
    points[4] = (Vector2){ r.x + r.width, r.y + r.height - cut };
    points[5] = (Vector2){ r.x + r.width, r.y };
}

static void DrawPanelGeometry(Rectangle bounds, Color fill, Color edge, bool raised)
{
    float cut = fminf(UiScale(12), fminf(bounds.width, bounds.height)*0.18f);
    Vector2 points[6];
    if (raised)
    {
        Rectangle shadow = bounds;
        shadow.x += UiScale(4);
        shadow.y += UiScale(6);
        ChamferPoints(shadow, cut, points);
        DrawTriangleFan(points, 6, g_ui ? g_ui->theme->shadow : BLACK);
    }
    ChamferPoints(bounds, cut, points);
    DrawTriangleFan(points, 6, fill);
    for (int i = 0; i < 6; i++)
        DrawLineEx(points[i], points[(i + 1)%6], UiScale(1.25f), edge);
}

static Color OpaqueSurface(Color fill)
{
    // Arena Ink panels are opaque printed shapes, not glass. Keep explicit
    // zero-alpha fills for outline-only focus rings, but make every real surface
    // opaque even if a caller passes a legacy translucent color.
    if (fill.a > 0) fill.a = 255;
    return fill;
}

void UiDrawPanel(Rectangle bounds, Color fill, Color edge, bool raised)
{
    fill = OpaqueSurface(fill);
    if (g_ui && UiSkinDrawPanel(&g_ui->skin, bounds, fill, edge, raised, false))
        return;
    DrawPanelGeometry(bounds, fill, edge, raised);
}

void UiDrawFeaturePanel(Rectangle bounds, Color fill, Color edge, bool raised)
{
    fill = OpaqueSurface(fill);
    if (g_ui && UiSkinDrawPanel(&g_ui->skin, bounds, fill, edge, raised, true))
        return;
    DrawPanelGeometry(bounds, fill, edge, raised);
}

void UiDrawControlSurface(Rectangle bounds, Color fill, Color edge, bool raised)
{
    fill = OpaqueSurface(fill);
    if (g_ui && UiSkinDrawButton(&g_ui->skin, bounds, fill, g_ui->theme->ink,
                                 edge, raised))
        return;
    DrawPanelGeometry(bounds, fill, edge, raised);
}

void UiDrawSignalRail(Rectangle bounds, Color color, bool rightSide)
{
    float width = UiScale(7);
    float x = rightSide ? bounds.x + bounds.width - width - UiScale(3)
                        : bounds.x + UiScale(3);
    DrawRectangleRec((Rectangle){ x, bounds.y + UiScale(7), width,
                                  bounds.height - UiScale(14) }, color);
    for (int i = 0; i < 2; i++)
    {
        float y = bounds.y + bounds.height - UiScale(12 + i*8);
        DrawLineEx((Vector2){ x - UiScale(5), y },
                   (Vector2){ x + width + UiScale(5), y - UiScale(5) },
                   UiScale(2), g_ui ? g_ui->theme->ink : BLACK);
    }
}

void UiDrawKeycap(Rectangle bounds, const char *label, bool active)
{
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeArenaInk();
    UiDrawControlSurface(bounds, active ? t->yellow : t->surfaceMuted,
                         active ? t->ink : t->border, active);
    UiDrawTextFit(UI_TEXT_CAPTION, label, bounds, UI_ALIGN_CENTER,
                  active ? t->ink : t->paper);
}

void UiDrawProgress(Rectangle bounds, float value, Color fill, bool segmented, int segments)
{
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeArenaInk();
    value = Clamp(value, 0.0f, 1.0f);
    if (g_ui && UiSkinDrawProgress(&g_ui->skin, bounds, value, t->ink, fill,
                                   segmented, segments, UiScale(3)))
        return;
    DrawRectangleRec(bounds, t->ink);
    if (!segmented || segments <= 1)
    {
        Rectangle f = bounds;
        f.width *= value;
        DrawRectangleRec(f, fill);
        return;
    }
    float gap = UiScale(3);
    float part = (bounds.width - gap*(segments - 1))/segments;
    for (int i = 0; i < segments; i++)
    {
        float amount = Clamp(value*segments - i, 0.0f, 1.0f);
        DrawRectangleRec((Rectangle){ bounds.x + i*(part + gap), bounds.y,
                                      part*amount, bounds.height }, fill);
    }
}

void UiDrawDecoration(UiDecoration decoration, Rectangle bounds, Color tint, float opacity)
{
    tint.a = (unsigned char)roundf(255.0f*Clamp(opacity, 0.0f, 1.0f));
    if (g_ui && UiSkinDrawDecoration(&g_ui->skin, decoration, bounds, tint))
        return;

    // Keep a small burst fallback so a missing skin never leaves an empty stage.
    Vector2 center = {
        bounds.x + bounds.width*0.5f, bounds.y + bounds.height*0.5f
    };
    float radius = fminf(bounds.width, bounds.height)*0.48f;
    for (int i = 0; i < 16; i++)
    {
        float angle = (float)i*2.0f*PI/16.0f;
        Vector2 start = { center.x + cosf(angle)*radius*0.25f,
                          center.y + sinf(angle)*radius*0.25f };
        Vector2 end = { center.x + cosf(angle)*radius,
                        center.y + sinf(angle)*radius };
        DrawLineEx(start, end, UiScale(i%4 == 0 ? 4.0f : 2.0f), tint);
    }
}

static Color MotifColor(Color color, float opacity)
{
    color.a = (unsigned char)roundf(255.0f*Clamp(opacity, 0.0f, 1.0f));
    return color;
}

void UiDrawCharacterMotif(CharacterUiMotif motif, Rectangle bounds,
                          Color primary, Color secondary, float opacity)
{
    Vector2 center = {
        bounds.x + bounds.width*0.5f,
        bounds.y + bounds.height*0.5f
    };
    float radius = fminf(bounds.width, bounds.height)*0.39f;
    float stroke = fmaxf(2.0f, UiScale(4.0f));
    Color p = MotifColor(primary, opacity);
    Color s = MotifColor(secondary, opacity*0.88f);

    if (motif == CHARACTER_UI_SAW)
    {
        DrawCircleLinesV(center, radius*0.70f, p);
        DrawCircleLinesV(center, radius*0.48f, s);
        for (int i = 0; i < 18; i++)
        {
            float angle = (float)i*2.0f*PI/18.0f;
            float half = 0.060f;
            Vector2 a = { center.x + cosf(angle - half)*radius*0.69f,
                          center.y + sinf(angle - half)*radius*0.69f };
            Vector2 b = { center.x + cosf(angle)*radius,
                          center.y + sinf(angle)*radius };
            Vector2 c = { center.x + cosf(angle + half)*radius*0.69f,
                          center.y + sinf(angle + half)*radius*0.69f };
            DrawTriangle(a, b, c, (i & 1) ? p : s);
        }
    }
    else if (motif == CHARACTER_UI_CROSSHAIR)
    {
        DrawCircleLinesV(center, radius*0.74f, p);
        DrawCircleLinesV(center, radius*0.40f, s);
        for (int i = 0; i < 4; i++)
        {
            float angle = i*PI*0.5f;
            Vector2 a = { center.x + cosf(angle)*radius*0.48f,
                          center.y + sinf(angle)*radius*0.48f };
            Vector2 b = { center.x + cosf(angle)*radius,
                          center.y + sinf(angle)*radius };
            DrawLineEx(a, b, stroke, p);
        }
        DrawCircleV(center, stroke*1.4f, s);
    }
    else if (motif == CHARACTER_UI_BLAST)
    {
        for (int ring = 1; ring <= 3; ring++)
            DrawCircleLinesV(center, radius*(0.22f + ring*0.18f),
                             ring == 2 ? s : p);
        for (int i = 0; i < 12; i++)
        {
            float angle = (float)i*2.0f*PI/12.0f;
            Vector2 a = { center.x + cosf(angle)*radius*0.70f,
                          center.y + sinf(angle)*radius*0.70f };
            Vector2 b = { center.x + cosf(angle)*radius,
                          center.y + sinf(angle)*radius };
            DrawLineEx(a, b, stroke*(i%3 == 0 ? 1.45f : 0.75f),
                       (i & 1) ? s : p);
        }
    }
    else if (motif == CHARACTER_UI_SHIELD)
    {
        Vector2 points[6];
        for (int i = 0; i < 6; i++)
        {
            float angle = -PI*0.5f + (float)i*2.0f*PI/6.0f;
            points[i] = (Vector2){ center.x + cosf(angle)*radius*0.82f,
                                  center.y + sinf(angle)*radius*0.82f };
        }
        for (int i = 0; i < 6; i++)
            DrawLineEx(points[i], points[(i + 1)%6], stroke*1.55f,
                       (i & 1) ? p : s);
        DrawLineEx((Vector2){ center.x - radius*0.62f, center.y },
                   (Vector2){ center.x + radius*0.62f, center.y }, stroke, p);
        DrawLineEx((Vector2){ center.x, center.y - radius*0.62f },
                   (Vector2){ center.x, center.y + radius*0.62f }, stroke, s);
    }
    else
    {
        for (int i = 0; i < 8; i++)
        {
            float angle = -PI*0.80f + i*PI*1.60f/7.0f;
            float branch = radius*(0.36f + 0.07f*(i%3));
            Vector2 root = { center.x, center.y + radius*0.70f };
            Vector2 tip = { center.x + cosf(angle)*branch,
                            center.y + sinf(angle)*branch - radius*0.16f };
            DrawLineEx(root, tip, stroke*0.70f, p);
            Vector2 leaf = Vector2Lerp(root, tip, 0.72f);
            DrawEllipse((int)leaf.x, (int)leaf.y,
                        radius*0.10f, radius*0.055f, (i & 1) ? s : p);
        }
        DrawCircleLinesV(center, radius*0.70f, s);
    }
}

void UiDrawComicBackdrop(void)
{
    if (!g_ui) return;
    Rectangle viewport = {
        0, 0, (float)g_ui->layout.viewportWidth, (float)g_ui->layout.viewportHeight
    };
    Rectangle backdropCanvas = g_ui->layout.content;
#if defined(BRAWL_MOBILE)
    // Keep controls on the safe reference canvas, but let the poster field bleed
    // beneath the notch and home-indicator edges instead of letterboxing the menu.
    backdropCanvas = viewport;
#endif
    if (UiSkinDrawBackdrop(&g_ui->skin, viewport, backdropCanvas,
                           g_ui->theme->blue, g_ui->theme->ink,
                           g_ui->theme->yellow, g_ui->theme->enemy))
        return;
    DrawRectangleRec(viewport, g_ui->theme->ink);
    DrawRectangleRec(backdropCanvas, g_ui->theme->blue);
}

static void DrawLogoWord(const char *text, Rectangle bounds, Color fill)
{
    Font font = FontForRole(UI_TEXT_DISPLAY);
    float size = bounds.height;
    float spacing = size*0.01f;
    Vector2 measure = MeasureTextEx(font, text, size, spacing);
    if (measure.x > bounds.width)
    {
        size *= bounds.width/measure.x;
        spacing = size*0.01f;
        measure = MeasureTextEx(font, text, size, spacing);
    }
    Vector2 position = {
        bounds.x + (bounds.width - measure.x)*0.5f,
        bounds.y + (bounds.height - measure.y)*0.5f
    };
    float white = fmaxf(2.0f, bounds.height*0.075f);
    float black = fmaxf(1.5f, bounds.height*0.045f);
    DrawTextOutlineRaw(font, text,
                       (Vector2){ position.x + white*1.35f,
                                  position.y + white*1.55f },
                       size, spacing, g_ui->theme->ink, g_ui->theme->ink, black);
    DrawTextOutlineRaw(font, text, position, size, spacing,
                       g_ui->theme->ink, g_ui->theme->paper, white);
    DrawTextOutlineRaw(font, text, position, size, spacing,
                       fill, g_ui->theme->ink, black);
}

void UiDrawArenaLogo(Rectangle bounds)
{
    if (!g_ui) return;
    Vector2 center = {
        bounds.x + bounds.width*0.47f,
        bounds.y + bounds.height*0.43f
    };
    Vector2 burst[20];
    Vector2 shadow[20];
    for (int i = 0; i < 20; i++)
    {
        float angle = -PI*0.5f + (float)i*2.0f*PI/20.0f;
        float radial = (i & 1) ? 0.40f : (0.49f + 0.05f*(float)((i*3)%5)/4.0f);
        burst[i] = (Vector2){
            center.x + cosf(angle)*bounds.width*radial,
            center.y + sinf(angle)*bounds.height*radial
        };
        shadow[i] = Vector2Add(burst[i], (Vector2){ UiScale(6), UiScale(7) });
    }
    DrawTriangleFan(shadow, 20, g_ui->theme->ink);
    DrawTriangleFan(burst, 20, g_ui->theme->paper);

    DrawLogoWord("BRAWL",
                 (Rectangle){ bounds.x + bounds.width*0.02f,
                              bounds.y - bounds.height*0.03f,
                              bounds.width*0.91f, bounds.height*0.45f },
                 g_ui->theme->yellow);
    DrawLogoWord("ARENA",
                 (Rectangle){ bounds.x + bounds.width*0.10f,
                              bounds.y + bounds.height*0.30f,
                              bounds.width*0.88f, bounds.height*0.47f },
                 g_ui->theme->yellow);
}

static bool MouseIn(Rectangle bounds)
{
    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, bounds);
}

static UiResponse Register(UiId id, Rectangle bounds, bool enabled)
{
    UiResponse response = { 0 };
    if (!g_ui || !enabled || !g_ui->interactionsEnabled) return response;
    if (g_ui->nodeCount < UI_MAX_FOCUS_NODES)
        g_ui->nodes[g_ui->nodeCount++] = (UiFocusNode){ id, bounds, true };
    else
    {
        g_ui->focusOverflow = true;
        return response;
    }
    if (g_ui->focused == 0) g_ui->focused = id;
    Rectangle pointerBounds = g_ui->modality == UI_INPUT_TOUCH
        ? UiTouchTargetBounds(bounds, 44.0f) : bounds;
    response.hovered = MouseIn(pointerBounds);
    response.focused = g_ui->focused == id;
    response.held = response.hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    response.activated =
        (response.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) ||
        (response.focused && g_ui->focusVisible && g_ui->activatePressed);
    if (response.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        g_ui->focused = id;
#if defined(BRAWL_MOBILE)
        g_ui->modality = UI_INPUT_TOUCH;
#else
        g_ui->modality = UI_INPUT_POINTER;
#endif
    }
    return response;
}

UiResponse UiInteract(UiId id, Rectangle bounds, bool enabled)
{
    return Register(id, bounds, enabled);
}

UiResponse UiButton(UiId id, Rectangle bounds, const char *label,
                    UiButtonStyle style, UiIcon icon)
{
    UiResponse r = Register(id, bounds, true);
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeArenaInk();
    Color fill = t->surfaceRaised;
    Color edge = t->ink;
    Color text = t->paper;
    if (style == UI_BUTTON_PRIMARY)
    {
        fill = t->enemy;
        edge = t->ink;
        text = t->paper;
    }
    else if (style == UI_BUTTON_DANGER)
    {
        fill = t->enemy;
        edge = t->ink;
    }
    else if (style == UI_BUTTON_BLUE)
        fill = t->blue;
    else if (style == UI_BUTTON_YELLOW)
    {
        fill = t->yellow;
        text = t->ink;
    }
    else if (style == UI_BUTTON_PURPLE)
        fill = t->purple;
    else if (style == UI_BUTTON_UTILITY)
        fill = t->surface;
    if (r.held)
        fill = ColorLerp(fill, t->ink, 0.14f);
    else if (r.hovered)
        fill = ColorLerp(fill, t->ink, 0.07f);

    Rectangle visual = bounds;
    bool animated = !g_ui || !g_ui->reducedMotion;
    if (r.held && animated) visual.y += UiScale(3);
    else if (r.hovered && animated) visual.y -= UiScale(2);
    UiDrawControlSurface(visual, fill, edge,
                         !r.held && style != UI_BUTTON_UTILITY);
    if (r.focused && g_ui && g_ui->focusVisible)
    {
        Rectangle outer = { bounds.x - UiScale(5), bounds.y - UiScale(5),
                            bounds.width + UiScale(10), bounds.height + UiScale(10) };
        Rectangle inner = { bounds.x - UiScale(2), bounds.y - UiScale(2),
                            bounds.width + UiScale(4), bounds.height + UiScale(4) };
        UiDrawPanel(outer, BLANK, t->ink, false);
        UiDrawPanel(inner, BLANK, t->paper, false);
    }
    if (icon >= 0)
    {
        Vector2 center = {
            visual.x + UiScale(24), visual.y + visual.height*0.5f
        };
        UiIconDraw(icon, center, UiScale(18), text);
        UiDrawTextFit(style == UI_BUTTON_PRIMARY ? UI_TEXT_HEADING : UI_TEXT_LABEL,
                      label, visual, UI_ALIGN_CENTER, text);
    }
    else
        UiDrawTextFit(style == UI_BUTTON_PRIMARY ? UI_TEXT_HEADING : UI_TEXT_LABEL,
                      label, visual, UI_ALIGN_CENTER, text);
    return r;
}

UiResponse UiIconButton(UiId id, Rectangle bounds, UiIcon icon, const char *accessibleLabel)
{
    (void)accessibleLabel;
    UiResponse r = Register(id, bounds, true);
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeArenaInk();
    Color fill = r.hovered ? ColorLerp(t->blue, t->ink, r.held ? 0.14f : 0.07f)
                           : t->surface;
    Rectangle visual = bounds;
    bool animated = !g_ui || !g_ui->reducedMotion;
    if (r.held && animated) visual.y += UiScale(3);
    else if (r.hovered && animated) visual.y -= UiScale(2);
    UiDrawControlSurface(visual, fill,
                         (r.focused && g_ui && g_ui->focusVisible) ? t->yellow :
                         t->ink, r.hovered && !r.held);
    UiIconDraw(icon, (Vector2){ visual.x + visual.width*0.5f,
                               visual.y + visual.height*0.5f },
               fminf(visual.width, visual.height)*0.42f, t->paper);
    return r;
}

UiResponse UiToggle(UiId id, Rectangle bounds, const char *label, bool value)
{
    UiResponse r = Register(id, bounds, true);
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeArenaInk();
    UiDrawPanel(bounds, r.hovered ? t->surfaceRaised : t->surface,
                (r.focused && g_ui && g_ui->focusVisible) ? t->yellow : t->ink, false);
    Rectangle labelBounds = bounds;
    labelBounds.x += UiScale(14);
    labelBounds.width -= UiScale(76);
    UiDrawTextFit(UI_TEXT_BODY, label, labelBounds, UI_ALIGN_LEFT, t->paper);
    Rectangle state = { bounds.x + bounds.width - UiScale(62),
                        bounds.y + (bounds.height - UiScale(30))*0.5f,
                        UiScale(50), UiScale(30) };
    UiDrawControlSurface(state, value ? t->yellow : t->surfaceMuted,
                         t->ink, value);
    UiDrawTextFit(UI_TEXT_CAPTION, value ? "ON" : "OFF", state,
                  UI_ALIGN_CENTER, value ? t->ink : t->paper);
    return r;
}
