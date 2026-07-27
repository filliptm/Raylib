#ifndef BRAWL_CONTENT_CATALOG_H
#define BRAWL_CONTENT_CATALOG_H

#include "content_types.h"

// Rebuilds validated runtime character/ability records from the configuration
// compatibility records. Call after authoring edits or configuration loads.
void ContentCatalogRebuildTyped(ContentCatalog *catalog);
void ContentCatalogResetAll(ContentCatalog *catalog);
void ContentCatalogResetCharacter(ContentCatalog *catalog, BrawlerClass character);

const CharacterDefinition *ContentCharacter(const ContentCatalog *catalog,
                                            BrawlerClass character);
const AbilityDefinition *ContentMainAbility(const ContentCatalog *catalog,
                                            BrawlerClass character);
const AbilityDefinition *ContentSuperAbility(const ContentCatalog *catalog,
                                             BrawlerClass character);

#endif
