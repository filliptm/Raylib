#ifndef COMMAND_CENTER_H
#define COMMAND_CENTER_H

#include "types.h"

// Toggle key handling. Call once per frame before PlayerUpdate.
void CommandCenterUpdate(World *w);

// Draws the panel and processes its widgets. Call last, over the HUD.
void CommandCenterDraw(World *w);

bool CommandCenterIsOpen(void);

// Opens the panel from elsewhere, e.g. the menu's TUNING card.
void CommandCenterForceOpen(void);

// True when the cursor is over the panel, so the game should ignore mouse input.
bool CommandCenterCapturesMouse(void);

#endif // COMMAND_CENTER_H
