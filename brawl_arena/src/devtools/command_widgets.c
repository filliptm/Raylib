#include "command_widgets.h"

#include "config.h"
#include "raymath.h"
#include <stddef.h>

#define ROW_H 27
#define LABEL_W 132
#define VALUE_W 62

const Color COMMAND_PANEL_BG   = { 14, 18, 28, 238 };
const Color COMMAND_PANEL_EDGE = { 60, 74, 98, 255 };
const Color COMMAND_TEXT_MAIN  = { 226, 232, 242, 255 };
const Color COMMAND_TEXT_DIM   = { 138, 150, 170, 255 };
const Color COMMAND_ACCENT     = { 92, 178, 255, 255 };
const Color COMMAND_ACCENT_DIM = { 44, 88, 130, 255 };
const Color COMMAND_TRACK_BG   = { 34, 40, 52, 255 };
const Color COMMAND_WARN       = { 255, 176, 80, 255 };

static const void *g_activeSlider = NULL;

Rectangle CommandPanelRect(void)
{
    return (Rectangle){
        COMMAND_PANEL_X,
        COMMAND_PANEL_TOP,
        COMMAND_PANEL_W,
        GetScreenHeight() - COMMAND_PANEL_TOP - 16
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
    DrawText(title, ui->x, ui->y, 13, COMMAND_ACCENT);
    ui->y += 17;
    DrawRectangle(ui->x, ui->y, ui->width, 1, (Color){ 48, 60, 80, 255 });
    ui->y += 8;
}

void CommandUiText(CommandUi *ui, const char *text, Color color)
{
    DrawText(text, ui->x, ui->y + 4, 13, color);
    ui->y += ROW_H - 6;
}

bool CommandUiButton(CommandUi *ui, const char *label)
{
    Rectangle bounds = { ui->x, ui->y, ui->width, ROW_H - 5 };
    bool hover = CommandUiMouseIn(bounds) && CommandUiMouseIn(ui->clip);
    bool clicked = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    DrawRectangleRounded(bounds, 0.28f, 6,
                         hover ? COMMAND_ACCENT_DIM : (Color){ 34, 42, 56, 255 });
    DrawRectangleRoundedLines(bounds, 0.28f, 6,
                              hover ? COMMAND_ACCENT : COMMAND_PANEL_EDGE);

    int textWidth = MeasureText(label, 13);
    DrawText(label, (int)(bounds.x + bounds.width/2 - textWidth/2),
             (int)(bounds.y + 5), 13,
             hover ? COMMAND_TEXT_MAIN : COMMAND_TEXT_DIM);

    ui->y += ROW_H;
    return clicked;
}

bool CommandUiToggle(CommandUi *ui, const char *label, bool *value)
{
    Rectangle bounds = { ui->x, ui->y, ui->width, ROW_H - 5 };
    bool hover = CommandUiMouseIn(bounds) && CommandUiMouseIn(ui->clip);
    bool clicked = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (clicked)
    {
        *value = !*value;
        ConfigMarkDirty();
    }

    DrawText(label, ui->x, ui->y + 5, 13,
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

    bool hover = CommandUiMouseIn(row) && CommandUiMouseIn(ui->clip);
    bool changed = false;

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) g_activeSlider = id;
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) && g_activeSlider == id)
        g_activeSlider = NULL;

    if (g_activeSlider == id)
    {
        float amount = (GetMousePosition().x - track.x)/track.width;
        *normalized = Clamp(amount, 0.0f, 1.0f);
        changed = true;
        ConfigMarkDirty();
    }

    DrawText(label, ui->x, ui->y + 4, 13,
             hover ? COMMAND_TEXT_MAIN : COMMAND_TEXT_DIM);
    DrawRectangleRounded(track, 0.9f, 6, COMMAND_TRACK_BG);

    Rectangle fill = track;
    fill.width = track.width*(*normalized);
    if (fill.width > 2)
        DrawRectangleRounded(fill, 0.9f, 6,
                             hover ? COMMAND_ACCENT : COMMAND_ACCENT_DIM);

    DrawCircle((int)(track.x + track.width*(*normalized)),
               (int)(track.y + track.height/2), 7,
               hover ? COMMAND_TEXT_MAIN : COMMAND_ACCENT);

    int textWidth = MeasureText(valueText, 12);
    DrawText(valueText, ui->x + ui->width - textWidth, ui->y + 5, 12,
             COMMAND_WARN);

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

    bool changed = SliderTrack(ui, value, label, &normalized,
                               TextFormat(format, *value));
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

    bool changed = SliderTrack(ui, value, label, &normalized,
                               TextFormat("%d", *value));
    if (changed)
        *value = minimum + (int)(normalized*(maximum - minimum) + 0.5f);
    return changed;
}

bool CommandUiCycler(CommandUi *ui, const char *label, int *value,
                     int count, const char **names)
{
    DrawText(label, ui->x, ui->y + 5, 13, COMMAND_TEXT_DIM);

    int boxWidth = ui->width - LABEL_W;
    Rectangle left = { ui->x + LABEL_W, ui->y + 1, 24, ROW_H - 7 };
    Rectangle right = { ui->x + ui->width - 24, ui->y + 1, 24, ROW_H - 7 };
    bool changed = false;

    bool leftHover = CommandUiMouseIn(left) && CommandUiMouseIn(ui->clip);
    bool rightHover = CommandUiMouseIn(right) && CommandUiMouseIn(ui->clip);

    DrawRectangleRounded(left, 0.3f, 5,
                         leftHover ? COMMAND_ACCENT_DIM : COMMAND_TRACK_BG);
    DrawRectangleRounded(right, 0.3f, 5,
                         rightHover ? COMMAND_ACCENT_DIM : COMMAND_TRACK_BG);
    DrawText("<", (int)left.x + 9, (int)left.y + 3, 13, COMMAND_TEXT_MAIN);
    DrawText(">", (int)right.x + 9, (int)right.y + 3, 13, COMMAND_TEXT_MAIN);

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
    if (changed) ConfigMarkDirty();

    const char *name = names[*value];
    int textWidth = MeasureText(name, 14);
    DrawText(name, (int)(left.x + boxWidth/2 - textWidth/2),
             ui->y + 4, 14, COMMAND_ACCENT);

    ui->y += ROW_H;
    return changed;
}
