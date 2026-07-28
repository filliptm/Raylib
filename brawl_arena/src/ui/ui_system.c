#include "ui_system.h"

#include "raymath.h"
#include <float.h>
#include <math.h>
#include <string.h>

static UiSystem *g_ui = NULL;

static Font LoadUiFont(const char *path, int size, bool *owned, bool *fallback)
{
    Font font = LoadFontEx(path, size, NULL, 0);
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
    ui->theme = UiThemeHelios();
    ui->resources.display = LoadUiFont("resources/fonts/BarlowCondensed-Bold.ttf", 96,
                                       &ui->resources.displayOwned, &ui->fontFallback);
    ui->resources.body = LoadUiFont("resources/fonts/Barlow-Regular.ttf", 64,
                                    &ui->resources.bodyOwned, &ui->fontFallback);
    ui->resources.emphasis = LoadUiFont("resources/fonts/Barlow-SemiBold.ttf", 64,
                                        &ui->resources.emphasisOwned, &ui->fontFallback);
    ui->resources.data = LoadUiFont("resources/fonts/IBMPlexMono-Medium.ttf", 64,
                                    &ui->resources.dataOwned, &ui->fontFallback);
    if (!UiSkinLoad(&ui->skin))
        TraceLog(LOG_WARNING, "UI: one or more skin resources are using safe fallbacks");
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
    float scale = UiReferenceScaleForViewport(width, height, preferenceScale);
    float refW = UI_REFERENCE_WIDTH*scale;
    float refH = UI_REFERENCE_HEIGHT*scale;
    float ox = (width - refW)*0.5f;
    float oy = (height - refH)*0.5f;
    float pad = 24.0f*scale;
    return (Rectangle){ ox + pad, oy + pad, refW - pad*2.0f, refH - pad*2.0f };
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
    if (next) ui->focused = next;
}

float UiMotionDuration(float normalDuration, bool reducedMotion)
{
    return reducedMotion ? 0.0f : fmaxf(0.0f, normalDuration);
}

void UiSystemBeginFrame(UiSystem *ui, const UiPreferences *preferences,
                        int width, int height, float dt)
{
    (void)dt;
    UiSystemSetActive(ui);
    ui->previousNodeCount = ui->nodeCount;
    memcpy(ui->previousNodes, ui->nodes,
           sizeof(UiFocusNode)*(size_t)ui->previousNodeCount);
    ui->nodeCount = 0;
    ui->interactionsEnabled = true;
    ui->focusOverflow = false;

    float prefScale = preferences ? preferences->scale : 1.0f;
    ui->glyphMode = preferences ? preferences->inputGlyphMode : UI_GLYPH_AUTO;
    ui->layout.viewportWidth = width;
    ui->layout.viewportHeight = height;
    ui->layout.viewportScale = fminf(width/UI_REFERENCE_WIDTH, height/UI_REFERENCE_HEIGHT);
    ui->layout.preferenceScale = Clamp(prefScale, 0.75f, 1.50f);
    ui->layout.scale = UiReferenceScaleForViewport(width, height, prefScale);
    ui->layout.origin = (Vector2){
        (width - UI_REFERENCE_WIDTH*ui->layout.scale)*0.5f,
        (height - UI_REFERENCE_HEIGHT*ui->layout.scale)*0.5f
    };
    ui->layout.safe = UiReferenceSafeRect(width, height, prefScale);
    ui->layout.content = (Rectangle){
        ui->layout.origin.x, ui->layout.origin.y,
        UI_REFERENCE_WIDTH*ui->layout.scale, UI_REFERENCE_HEIGHT*ui->layout.scale
    };

    Vector2 mouse = GetMousePosition();
    if (Vector2Distance(mouse, ui->previousMouse) > 1.5f || GetMouseWheelMove() != 0.0f)
    {
        ui->modality = UI_INPUT_POINTER;
        ui->focusVisible = false;
    }
    ui->previousMouse = mouse;

    int gamepad = AnyGamepadAvailable();
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

const char *UiBindingLabel(const char *keyboardMouse, const char *gamepad)
{
    if (!g_ui) return keyboardMouse;
    if (g_ui->glyphMode == UI_GLYPH_KEYBOARD_MOUSE) return keyboardMouse;
    if (g_ui->glyphMode == UI_GLYPH_GAMEPAD) return gamepad;
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
    while (measure.x > bounds.width && size > minimum)
    {
        size -= 1.0f;
        measure = MeasureTextEx(font, text, size, size*0.025f);
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
    // Panels are physical Helios-9 control surfaces, not glass. Keep explicit
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
    if (g_ui && UiSkinDrawButton(&g_ui->skin, bounds, fill, edge, raised))
        return;
    DrawPanelGeometry(bounds, fill, edge, raised);
}

void UiDrawSignalRail(Rectangle bounds, Color color, bool rightSide)
{
    float x = rightSide ? bounds.x + bounds.width - UiScale(4) : bounds.x;
    DrawRectangleRec((Rectangle){ x, bounds.y + UiScale(5), UiScale(4),
                                  bounds.height - UiScale(10) }, color);
    for (int i = 0; i < 3; i++)
    {
        float y = bounds.y + bounds.height - UiScale(10 + i*6);
        DrawLineEx((Vector2){ x - UiScale(5), y },
                   (Vector2){ x + UiScale(8), y - UiScale(5) }, UiScale(1), color);
    }
}

void UiDrawKeycap(Rectangle bounds, const char *label, bool active)
{
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeHelios();
    UiDrawControlSurface(bounds, active ? t->hullBright : t->hull,
                         active ? t->ion : t->line, false);
    UiDrawTextFit(UI_TEXT_CAPTION, label, bounds, UI_ALIGN_CENTER,
                  active ? t->paper : t->mist);
}

void UiDrawProgress(Rectangle bounds, float value, Color fill, bool segmented, int segments)
{
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeHelios();
    value = Clamp(value, 0.0f, 1.0f);
    if (g_ui && UiSkinDrawProgress(&g_ui->skin, bounds, value, t->hull, fill,
                                   segmented, segments, UiScale(3)))
        return;
    DrawRectangleRec(bounds, t->hull);
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

    // Decorative resources are optional. A restrained geometric fallback keeps
    // the composition intentional when assets are missing without blocking play.
    Vector2 center = {
        bounds.x + bounds.width*0.5f, bounds.y + bounds.height*0.5f
    };
    float radius = fminf(bounds.width, bounds.height)*0.48f;
    DrawCircleLines((int)center.x, (int)center.y, radius, tint);
    DrawCircleLines((int)center.x, (int)center.y, radius*0.68f, tint);
    DrawCircleLines((int)center.x, (int)center.y, radius*0.34f, tint);
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
    response.hovered = MouseIn(bounds);
    response.focused = g_ui->focused == id;
    response.held = response.hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    response.activated =
        (response.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) ||
        (response.focused && g_ui->focusVisible && g_ui->activatePressed);
    if (response.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        g_ui->focused = id;
        g_ui->modality = UI_INPUT_POINTER;
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
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeHelios();
    Color fill = t->deckRaised;
    Color edge = t->line;
    Color text = t->paper;
    if (style == UI_BUTTON_PRIMARY)
    {
        fill = r.held ? (Color){ 174, 111, 40, 255 } : t->safety;
        edge = t->ready;
        text = t->voidBg;
    }
    else if (style == UI_BUTTON_DANGER) edge = t->enemy;
    else if (style == UI_BUTTON_UTILITY) fill = t->deck;
    if (r.hovered || (r.focused && g_ui && g_ui->focusVisible))
        edge = style == UI_BUTTON_PRIMARY ? t->paper : t->ion;

    UiDrawControlSurface(bounds, fill, edge, style == UI_BUTTON_PRIMARY);
    if (r.focused && g_ui && g_ui->focusVisible)
    {
        Rectangle ring = { bounds.x - UiScale(3), bounds.y - UiScale(3),
                           bounds.width + UiScale(6), bounds.height + UiScale(6) };
        UiDrawPanel(ring, BLANK, t->paper, false);
    }
    if (icon >= 0)
    {
        Vector2 center = { bounds.x + UiScale(24), bounds.y + bounds.height*0.5f };
        UiIconDraw(icon, center, UiScale(18), text);
        Rectangle textBounds = bounds;
        textBounds.x += UiScale(42);
        textBounds.width -= UiScale(52);
        UiDrawTextFit(style == UI_BUTTON_PRIMARY ? UI_TEXT_HEADING : UI_TEXT_LABEL,
                      label, textBounds, UI_ALIGN_LEFT, text);
    }
    else
        UiDrawTextFit(style == UI_BUTTON_PRIMARY ? UI_TEXT_HEADING : UI_TEXT_LABEL,
                      label, bounds, UI_ALIGN_CENTER, text);
    return r;
}

UiResponse UiIconButton(UiId id, Rectangle bounds, UiIcon icon, const char *accessibleLabel)
{
    (void)accessibleLabel;
    UiResponse r = Register(id, bounds, true);
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeHelios();
    UiDrawControlSurface(bounds, r.hovered ? t->hull : t->deck,
                         (r.focused && g_ui && g_ui->focusVisible) ? t->paper :
                         (r.hovered ? t->ion : t->line), false);
    UiIconDraw(icon, (Vector2){ bounds.x + bounds.width*0.5f,
                               bounds.y + bounds.height*0.5f },
               fminf(bounds.width, bounds.height)*0.42f, t->paper);
    return r;
}

UiResponse UiToggle(UiId id, Rectangle bounds, const char *label, bool value)
{
    UiResponse r = Register(id, bounds, true);
    const UiTheme *t = g_ui ? g_ui->theme : UiThemeHelios();
    UiDrawPanel(bounds, r.hovered ? t->deckRaised : t->deck,
                (r.focused && g_ui && g_ui->focusVisible) ? t->paper : t->line, false);
    Rectangle labelBounds = bounds;
    labelBounds.x += UiScale(14);
    labelBounds.width -= UiScale(76);
    UiDrawTextFit(UI_TEXT_BODY, label, labelBounds, UI_ALIGN_LEFT, t->paper);
    Rectangle state = { bounds.x + bounds.width - UiScale(54),
                        bounds.y + (bounds.height - UiScale(24))*0.5f,
                        UiScale(42), UiScale(24) };
    DrawRectangleRounded(state, 0.9f, 8, value ? t->ion : t->hull);
    DrawCircle((int)(value ? state.x + state.width - state.height*0.5f
                          : state.x + state.height*0.5f),
               (int)(state.y + state.height*0.5f), state.height*0.32f,
               value ? t->voidBg : t->mist);
    return r;
}
