#ifndef MENU_H
#define MENU_H

#include "types.h"
#include "assets.h"

void MenuInit(Assets *a);

// Lets an open menu overlay swallow ESC. Returns true when it did.
bool MenuConsumeEscape(void);

// Advances the podium camera and idle animation. Feed it unscaled real time.
void MenuUpdate(World *w, float dt);

// Draws whichever of the menu screens is current, 3D scene then interface.
void MenuDraw(World *w);

// Screen flow. A request fades out, swaps at black, then fades back in.
void ShellRequestScreen(World *w, AppScreen screen);
void ShellUpdate(World *w, float dt);
void ShellDrawFade(World *w);
bool ShellIsTransitioning(const World *w);

#endif // MENU_H
