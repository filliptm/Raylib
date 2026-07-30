#ifndef COMMAND_WIDGETS_H
#define COMMAND_WIDGETS_H

#include "app_types.h"

#define COMMAND_PANEL_X 16
#define COMMAND_PANEL_TOP 74
#define COMMAND_PANEL_W 372

typedef struct CommandUi {
    App *world;
    int x;
    int y;
    int width;
    Rectangle clip;
} CommandUi;

extern const Color COMMAND_PANEL_BG;
extern const Color COMMAND_PANEL_EDGE;
extern const Color COMMAND_TEXT_MAIN;
extern const Color COMMAND_TEXT_DIM;
extern const Color COMMAND_ACCENT;
extern const Color COMMAND_ACCENT_DIM;
extern const Color COMMAND_TRACK_BG;
extern const Color COMMAND_WARN;

Rectangle CommandPanelRect(void);
bool CommandUiMouseIn(Rectangle bounds);
bool CommandUiHasActiveSlider(void);
void CommandUiResetInteraction(void);

void CommandUiSection(CommandUi *ui, const char *title);
void CommandUiText(CommandUi *ui, const char *text, Color color);
bool CommandUiButton(CommandUi *ui, const char *label);
bool CommandUiToggle(CommandUi *ui, const char *label, bool *value);
bool CommandUiSliderF(CommandUi *ui, const char *label, float *value,
                      float minimum, float maximum, const char *format);
bool CommandUiSliderI(CommandUi *ui, const char *label, int *value,
                      int minimum, int maximum);
bool CommandUiCycler(CommandUi *ui, const char *label, int *value,
                     int count, const char **names);

#endif // COMMAND_WIDGETS_H
