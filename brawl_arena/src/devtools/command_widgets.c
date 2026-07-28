#include "command_widgets.h"

#include "config.h"
#include "ui_system.h"
#include "raymath.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define ROW_H 27
#define LABEL_W 132
#define VALUE_W 62

const Color COMMAND_PANEL_BG   = { 7, 17, 31, 255 };
const Color COMMAND_PANEL_EDGE = { 59, 88, 116, 255 };
const Color COMMAND_TEXT_MAIN  = { 243, 247, 251, 255 };
const Color COMMAND_TEXT_DIM   = { 130, 149, 170, 255 };
const Color COMMAND_ACCENT     = { 100, 185, 255, 255 };
const Color COMMAND_ACCENT_DIM = { 41, 67, 94, 255 };
const Color COMMAND_TRACK_BG   = { 26, 48, 73, 255 };
const Color COMMAND_WARN       = { 255, 157, 66, 255 };

static const void *g_activeSlider = NULL;

Rectangle CommandPanelRect(void)
{
    float width = fminf(UiScale(520.0f), GetScreenWidth()*0.48f);
    return (Rectangle){
        UiScale(16.0f),
        UiScale(18.0f),
        width,
        GetScreenHeight() - UiScale(36.0f)
    };
}

bool CommandUiMouseIn(Rectangle bounds)
{
    Vector2 mouse = GetMousePosition();
    return mouse.x >= bounds.x && mouse.x <= bounds.x + bounds.width &&
           mouse.y >= bounds.y && mouse.y <= bounds.y + bounds.height;
}

bool CommandUiHasActiveSlider(void)
{
    return g_activeSlider != NULL;
}

void CommandUiResetInteraction(void)
{
    g_activeSlider = NULL;
}

void CommandUiSection(CommandUi *ui, const char *title)
{
    ui->y += 8;
    UiDrawText(UI_TEXT_CAPTION, title, (Vector2){ ui->x, ui->y }, COMMAND_ACCENT);
    ui->y += 17;
    DrawRectangle(ui->x, ui->y, ui->width, 1, (Color){ 48, 60, 80, 255 });
    ui->y += 8;
}

void CommandUiText(CommandUi *ui, const char *text, Color color)
{
    UiDrawText(UI_TEXT_CAPTION, text, (Vector2){ ui->x, ui->y + 4 }, color);
    ui->y += ROW_H - 6;
}

bool CommandUiButton(CommandUi *ui, const char *label)
{
    Rectangle bounds = { ui->x, ui->y, ui->width, ROW_H - 5 };
    bool inClip = CheckCollisionRecs(bounds, ui->clip);
    UiResponse response = UiInteract(UiHash(label), bounds, inClip);
    bool hover = response.hovered && CommandUiMouseIn(ui->clip);
    bool clicked = response.activated;

    UiDrawControlSurface(bounds,
                         hover ? COMMAND_ACCENT_DIM : (Color){ 34, 42, 56, 255 },
                         hover ? COMMAND_ACCENT : COMMAND_PANEL_EDGE, false);

    if (response.focused && UiSystemActive()->focusVisible)
        DrawRectangleLinesEx(bounds, UiScale(2), COMMAND_TEXT_MAIN);
    UiDrawTextFit(UI_TEXT_CAPTION, label, bounds, UI_ALIGN_CENTER,
                  hover ? COMMAND_TEXT_MAIN : COMMAND_TEXT_DIM);

    ui->y += ROW_H;
    return clicked;
}

bool CommandUiToggle(CommandUi *ui, const char *label, bool *value)
{
    Rectangle bounds = { ui->x, ui->y, ui->width, ROW_H - 5 };
    bool inClip = CheckCollisionRecs(bounds, ui->clip);
    UiResponse response = UiInteract(UiHash(label), bounds, inClip);
    bool hover = response.hovered && CommandUiMouseIn(ui->clip);
    bool clicked = response.activated;
    if (clicked)
    {
        *value = !*value;
        ConfigMarkDirty();
    }

    UiDrawText(UI_TEXT_CAPTION, label, (Vector2){ ui->x, ui->y + 5 },
               hover ? COMMAND_TEXT_MAIN : COMMAND_TEXT_DIM);

    float pillWidth = 44.0f;
    float pillHeight = 18.0f;
    Rectangle pill = {
        ui->x + ui->width - pillWidth,
        ui->y + 2,
        pillWidth,
        pillHeight
    };
    DrawRectangleRounded(pill, 0.9f, 8,
                         *value ? COMMAND_ACCENT : COMMAND_TRACK_BG);

    float knobX = *value ? pill.x + pillWidth - pillHeight + 2 : pill.x + 2;
    DrawCircle((int)(knobX + (pillHeight - 4)/2),
               (int)(pill.y + pillHeight/2),
               (pillHeight - 4)/2,
               *value ? (Color){ 12, 20, 30, 255 } : COMMAND_TEXT_DIM);
    if (response.focused && UiSystemActive()->focusVisible)
        DrawRectangleLinesEx(bounds, UiScale(1), COMMAND_TEXT_MAIN);

    ui->y += ROW_H;
    return clicked;
}

static bool SliderTrack(CommandUi *ui, const void *id, const char *label,
                        float *normalized, const char *valueText)
{
    Rectangle row = { ui->x, ui->y, ui->width, ROW_H - 5 };
    Rectangle track = {
        ui->x + LABEL_W,
        ui->y + 7,
        ui->width - LABEL_W - VALUE_W,
        9
    };

    bool inClip = CheckCollisionRecs(row, ui->clip);
    UiId focusId = (UiId)((uintptr_t)id & 0xffffffffu);
    if (focusId == 0) focusId = UiHash(label);
    UiResponse response = UiInteract(focusId, row, inClip);
    bool hover = response.hovered && CommandUiMouseIn(ui->clip);
    bool changed = false;

    if (response.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) g_activeSlider = id;
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) && g_activeSlider == id)
        g_activeSlider = NULL;

    if (g_activeSlider == id)
    {
        float amount = (GetMousePosition().x - track.x)/track.width;
        *normalized = Clamp(amount, 0.0f, 1.0f);
        changed = true;
        ConfigMarkDirty();
    }
    if (response.focused && UiSystemActive()->focusVisible &&
        UiSystemActive()->navigationX != 0)
    {
        float step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                   ? 0.005f : 0.025f;
        *normalized = Clamp(*normalized + step*UiSystemActive()->navigationX,
                            0.0f, 1.0f);
        changed = true;
        ConfigMarkDirty();
    }

    UiDrawText(UI_TEXT_CAPTION, label, (Vector2){ ui->x, ui->y + 4 },
               hover ? COMMAND_TEXT_MAIN : COMMAND_TEXT_DIM);
    // The command body already owns a scissor rectangle. Keep this compact
    // inspector track code-drawn because raylib scissor state is not nestable;
    // player-facing progress bars use the textured shared primitive.
    DrawRectangleRounded(track, 0.9f, 6, COMMAND_TRACK_BG);
    Rectangle fill = track;
    fill.width = track.width*(*normalized);
    if (fill.width > 2)
        DrawRectangleRounded(fill, 0.9f, 6,
                             hover ? COMMAND_ACCENT : COMMAND_ACCENT_DIM);

    DrawCircle((int)(track.x + track.width*(*normalized)),
               (int)(track.y + track.height/2), 7,
               hover ? COMMAND_TEXT_MAIN : COMMAND_ACCENT);

    UiDrawTextAligned(UI_TEXT_CAPTION, valueText,
                      (Rectangle){ ui->x + ui->width - VALUE_W, ui->y,
                                   VALUE_W, ROW_H - 5 },
                      UI_ALIGN_RIGHT, COMMAND_WARN);
    if (response.focused && UiSystemActive()->focusVisible)
        DrawRectangleLinesEx(row, UiScale(1), COMMAND_TEXT_MAIN);

    ui->y += ROW_H;
    return changed;
}

bool CommandUiSliderF(CommandUi *ui, const char *label, float *value,
                      float minimum, float maximum, const char *format)
{
    float normalized = maximum > minimum
                     ? (*value - minimum)/(maximum - minimum)
                     : 0.0f;
    normalized = Clamp(normalized, 0.0f, 1.0f);

    char valueText[48];
    snprintf(valueText, sizeof(valueText), format, *value);
    bool changed = SliderTrack(ui, value, label, &normalized, valueText);
    if (changed) *value = minimum + normalized*(maximum - minimum);
    return changed;
}

bool CommandUiSliderI(CommandUi *ui, const char *label, int *value,
                      int minimum, int maximum)
{
    float normalized = maximum > minimum
                     ? (float)(*value - minimum)/(float)(maximum - minimum)
                     : 0.0f;
    normalized = Clamp(normalized, 0.0f, 1.0f);

    char valueText[32];
    snprintf(valueText, sizeof(valueText), "%d", *value);
    bool changed = SliderTrack(ui, value, label, &normalized, valueText);
    if (changed)
        *value = minimum + (int)(normalized*(maximum - minimum) + 0.5f);
    return changed;
}

bool CommandUiCycler(CommandUi *ui, const char *label, int *value,
                     int count, const char **names)
{
    UiDrawText(UI_TEXT_CAPTION, label, (Vector2){ ui->x, ui->y + 5 }, COMMAND_TEXT_DIM);

    int boxWidth = ui->width - LABEL_W;
    Rectangle left = { ui->x + LABEL_W, ui->y + 1, 24, ROW_H - 7 };
    Rectangle right = { ui->x + ui->width - 24, ui->y + 1, 24, ROW_H - 7 };
    bool changed = false;

    bool inClip = CheckCollisionRecs((Rectangle){ ui->x, ui->y, ui->width, ROW_H - 5 },
                                     ui->clip);
    UiId focusId = (UiId)((uintptr_t)value & 0xffffffffu);
    if (focusId == 0) focusId = UiHash(label);
    UiResponse response = UiInteract(focusId,
        (Rectangle){ ui->x, ui->y, ui->width, ROW_H - 5 }, inClip);
    bool leftHover = CommandUiMouseIn(left) && CommandUiMouseIn(ui->clip);
    bool rightHover = CommandUiMouseIn(right) && CommandUiMouseIn(ui->clip);

    UiDrawControlSurface(left,
                         leftHover ? COMMAND_ACCENT_DIM : COMMAND_TRACK_BG,
                         leftHover ? COMMAND_ACCENT : COMMAND_PANEL_EDGE, false);
    UiDrawControlSurface(right,
                         rightHover ? COMMAND_ACCENT_DIM : COMMAND_TRACK_BG,
                         rightHover ? COMMAND_ACCENT : COMMAND_PANEL_EDGE, false);
    UiIconDraw(UI_ICON_PREVIOUS,
               (Vector2){ left.x + left.width/2, left.y + left.height/2 },
               UiScale(10), COMMAND_TEXT_MAIN);
    UiIconDraw(UI_ICON_NEXT,
               (Vector2){ right.x + right.width/2, right.y + right.height/2 },
               UiScale(10), COMMAND_TEXT_MAIN);

    if (leftHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        *value = (*value - 1 + count)%count;
        changed = true;
    }
    if (rightHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        *value = (*value + 1)%count;
        changed = true;
    }
    if (response.focused && UiSystemActive()->focusVisible &&
        UiSystemActive()->navigationX != 0)
    {
        *value = (*value + UiSystemActive()->navigationX + count)%count;
        changed = true;
    }
    if (changed) ConfigMarkDirty();

    const char *name = names[*value];
    UiDrawTextAligned(UI_TEXT_CAPTION, name,
                      (Rectangle){ left.x + left.width, (float)ui->y,
                                   boxWidth - left.width - right.width,
                                   ROW_H - 5 },
                      UI_ALIGN_CENTER, COMMAND_ACCENT);
    if (response.focused && UiSystemActive()->focusVisible)
        DrawRectangleLinesEx((Rectangle){ ui->x, ui->y, ui->width, ROW_H - 5 },
                             UiScale(1), COMMAND_TEXT_MAIN);

    ui->y += ROW_H;
    return changed;
}
