#ifndef CONFIG_H
#define CONFIG_H

#include "app_types.h"

#define PROJECT_CONFIG_PATH "config/gameplay.cfg"
#define LOCAL_CONFIG_PATH "tuning.local.cfg"
#define PROFILE_CONFIG_PATH "profile.cfg"
#define LEGACY_CONFIG_PATH "tuning.cfg"

// Initializes compiled recovery values, loads the required tracked project file, then
// overlays the optional local draft and profile. Returns false only when the canonical
// project file could not be validated; recovery defaults remain playable in that case.
bool ConfigInitialize(App *w);

// Local draft/profile persistence. Command-center edits autosave here and never mutate
// the tracked project file until one of the explicit promotion functions is called.
void ConfigMarkDirty(void);
void ConfigAutoSave(App *w, float realDt);
void ConfigFlush(App *w);

// Explicit authoring actions.
bool ConfigPromoteAll(App *w);
bool ConfigPromoteKit(App *w, BrawlerClass cls);
void ConfigResetAllToProject(App *w);
void ConfigResetKitToProject(App *w, BrawlerClass cls);

// Provenance and feedback for the command center.
int ConfigProjectOverrideCount(App *w);
int ConfigKitOverrideCount(App *w, BrawlerClass cls);
const char *ConfigStatus(const App *w);

// Standalone validator used by tests and the Makefile validation target.
bool ConfigValidateProjectFile(const char *path, char *message, int messageSize);

#endif // CONFIG_H
