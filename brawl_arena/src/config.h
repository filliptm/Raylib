#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

#define CONFIG_PATH "tuning.cfg"

// Reads tuning.cfg over the current tuning and weapon table. Missing or unknown keys
// are left at whatever they already were, so a partial or older file still loads.
bool ConfigLoad(World *w);

void ConfigSave(const World *w);

// Called by the command center whenever a control changes something worth keeping.
void ConfigMarkDirty(void);

// Writes the file a short moment after the last edit, so dragging a slider does not
// hammer the disk. Feed it unscaled real time.
void ConfigAutoSave(World *w, float realDt);

// Writes straight away if anything is pending, instead of waiting out the debounce.
// Used at screen changes so a tweak is on disk before you can quit from the menu.
void ConfigFlush(World *w);

#endif // CONFIG_H
