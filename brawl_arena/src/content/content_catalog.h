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
const CharacterUiStyle *ContentCharacterUiStyle(BrawlerClass character);
const AbilityDefinition *ContentMainAbility(const ContentCatalog *catalog,
                                            BrawlerClass character);
const AbilityDefinition *ContentSuperAbility(const ContentCatalog *catalog,
                                             BrawlerClass character);
const AbilityDefinition *ContentMobilityAbility(const ContentCatalog *catalog,
                                                BrawlerClass character);
const AbilityDefinition *ContentSecondaryAbility(const ContentCatalog *catalog,
                                                 BrawlerClass character);
const AbilityDefinition *ContentAbility(const ContentCatalog *catalog, int abilityId);
const CharacterShowcaseDefinition *ContentCharacterShowcase(
    const ContentCatalog *catalog);
bool ContentShowcaseValid(const CharacterShowcaseDefinition *showcase);

#endif
