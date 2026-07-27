#ifndef MENU_H
#define MENU_H

#include "app_types.h"
#include "assets.h"

void MenuInit(Assets *a);

// Lets an open menu overlay swallow ESC. Returns true when it did.
bool MenuConsumeEscape(void);

// Advances the podium camera and idle animation. Feed it unscaled real time.
void MenuUpdate(App *w, float dt);

// Draws whichever of the menu screens is current, 3D scene then interface.
void MenuDraw(App *w);

// Screen flow. A request fades out, swaps at black, then fades back in.
void ShellRequestScreen(App *w, AppScreen screen);
void ShellUpdate(App *w, float dt);
void ShellDrawFade(App *w);
bool ShellIsTransitioning(const App *w);

#endif // MENU_H
