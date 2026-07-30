#ifndef BRAWL_ATTACK_CONTENT_H
#define BRAWL_ATTACK_CONTENT_H

#include "content_types.h"

#define ATTACK_PRESENTATION_PATH "data/attacks/presentation.cfg"
#define ATTACK_DRAFT_PATH "attacks.local.cfg"

// Clears every document back to "unauthored" (legacy recipes drive the ability).
void AttackContentDefaults(ContentCatalog *catalog);

// True when this ability's visuals come from an authored document.
bool AttackAuthored(const ContentCatalog *catalog, int abilityIndex);

// Fills a starter document for the ability: a muzzle flash and an impact burst
// tuned by behavior, so authoring never begins from a blank page.
void AttackPresentationTemplate(const ContentCatalog *catalog, int abilityIndex,
                                AttackPresentation *out);

// Range/count validation over every authored document.
bool AttackContentValidate(const ContentCatalog *catalog, char *message, int capacity);

// Tolerant load: a missing file is success (nothing authored there yet); a
// malformed line fails with a message and leaves the catalog unchanged.
bool AttackContentLoadFile(ContentCatalog *catalog, const char *path,
                           char *message, int capacity);

// Atomic write of all authored documents.
bool AttackContentSaveFile(const ContentCatalog *catalog, const char *path,
                           char *message, int capacity);

#endif
