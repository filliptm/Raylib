#ifndef HUD_H
#define HUD_H

#include "app_types.h"
#include "player.h"
#include "ui_theme.h"

// Health-bar hue is a stable team identifier and never depends on remaining health.
static inline Color HudHealthBarColor(const UiTheme *theme, Team team,
                                      bool highContrast)
{
    Color base = team == TEAM_PLAYER ? theme->ally : theme->enemy;
    return UiThemeHighContrast(base, highContrast);
}

// Health bars floating over each brawler, drawn in screen space after the 3D pass.
void HudDrawBars(App *w);

// Objective/ability broadcast, respawn/result overlays, and control hints.
void HudDrawPanel(App *w);
void HudObservePlayerInput(App *w, const PlayerInput *input);
bool HudConsumeContinue(void);

#endif // HUD_H
