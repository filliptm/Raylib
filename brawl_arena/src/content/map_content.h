#ifndef BRAWL_MAP_CONTENT_H
#define BRAWL_MAP_CONTENT_H

#include "content_types.h"

// Loads and validates one map directory containing map.cfg plus its declared layers.
bool MapDefinitionLoad(const char *directory, MapDefinition *out,
                       char *message, int messageSize);

// Loads every map named by a manifest into the application-owned content catalog.
// Weapon definitions already present in the catalog are left untouched.
bool MapCatalogLoad(ContentCatalog *catalog, const char *manifestPath,
                    char *message, int messageSize);

const MapDefinition *MapCatalogSelected(const ContentCatalog *catalog);
const MapDefinition *MapCatalogFind(const ContentCatalog *catalog, const char *id);

#endif
