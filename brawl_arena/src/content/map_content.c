#include "map_content.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_FORMAT_VERSION 1
#define MAP_PATH_SIZE 512
#define MAP_LINE_SIZE 512

static void SetMessage(char *message, int messageSize, const char *text)
{
    if (!message || messageSize <= 0) return;
    snprintf(message, (size_t)messageSize, "%s", text ? text : "");
}

static char *Trim(char *text)
{
    while (isspace((unsigned char)*text)) text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return text;
}

static bool JoinPath(char *out, int outSize, const char *directory, const char *name)
{
    int written = snprintf(out, (size_t)outSize, "%s/%s", directory, name);
    return written > 0 && written < outSize;
}

static bool ParseVisual(char c, MapVisualCell *out)
{
    *out = (MapVisualCell){ MAP_PALETTE_ORANGE, MAP_VISUAL_DEFAULT };
    switch (c)
    {
        case '.': return true;
        case 'p': out->palette = MAP_PALETTE_PURPLE; return true;
        case 'd': out->kind = MAP_VISUAL_DOOR; return true;
        case 'D': out->palette = MAP_PALETTE_PURPLE; out->kind = MAP_VISUAL_DOOR; return true;
        case 's': out->kind = MAP_VISUAL_DISPLAY; return true;
        case 'S': out->palette = MAP_PALETTE_PURPLE; out->kind = MAP_VISUAL_DISPLAY; return true;
        case 'a': out->kind = MAP_VISUAL_FLOOR_ACCENT; return true;
        case 'A': out->palette = MAP_PALETTE_PURPLE; out->kind = MAP_VISUAL_FLOOR_ACCENT; return true;
        case 'c': out->kind = MAP_VISUAL_CARGO; return true;
        case 'C': out->palette = MAP_PALETTE_PURPLE; out->kind = MAP_VISUAL_CARGO; return true;
        case 't': out->kind = MAP_VISUAL_TECH; return true;
        case 'T': out->palette = MAP_PALETTE_PURPLE; out->kind = MAP_VISUAL_TECH; return true;
        default: return false;
    }
}

static bool ReadLayer(const char *path, int width, int height, char rows[][MAX_ARENA_WIDTH],
                      char *message, int messageSize)
{
    FILE *file = fopen(path, "r");
    if (!file)
    {
        char detail[MAP_LINE_SIZE];
        snprintf(detail, sizeof(detail), "cannot open map layer: %s", path);
        SetMessage(message, messageSize, detail);
        return false;
    }

    char line[MAP_LINE_SIZE];
    int row = 0;
    while (fgets(line, sizeof(line), file))
    {
        size_t length = strcspn(line, "\r\n");
        line[length] = '\0';
        if (line[0] == ';') continue;
        if (row >= height)
        {
            fclose(file);
            SetMessage(message, messageSize, "map layer has too many rows");
            return false;
        }
        if ((int)strlen(line) != width)
        {
            fclose(file);
            char detail[MAP_LINE_SIZE];
            snprintf(detail, sizeof(detail), "%s row %d has width %zu; expected %d",
                     path, row + 1, strlen(line), width);
            SetMessage(message, messageSize, detail);
            return false;
        }
        memcpy(rows[row++], line, (size_t)width);
    }
    fclose(file);

    if (row != height)
    {
        char detail[MAP_LINE_SIZE];
        snprintf(detail, sizeof(detail), "%s has %d rows; expected %d", path, row, height);
        SetMessage(message, messageSize, detail);
        return false;
    }
    return true;
}

static bool ReadVisualLayer(const char *path, MapDefinition *map,
                            char *message, int messageSize)
{
    char rows[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH] = { 0 };
    if (!ReadLayer(path, map->width, map->height, rows, message, messageSize)) return false;

    for (int z = 0; z < map->height; z++)
    {
        for (int x = 0; x < map->width; x++)
        {
            if (!ParseVisual(rows[z][x], &map->visual[z][x]))
            {
                char detail[MAP_LINE_SIZE];
                snprintf(detail, sizeof(detail), "%s has invalid visual '%c' at %d,%d",
                         path, rows[z][x], x, z);
                SetMessage(message, messageSize, detail);
                return false;
            }
        }
    }
    return true;
}

static bool ReadProps(const char *path, MapDefinition *map, char *message, int messageSize)
{
    FILE *file = fopen(path, "r");
    if (!file)
    {
        char detail[MAP_LINE_SIZE];
        snprintf(detail, sizeof(detail), "cannot open map props: %s", path);
        SetMessage(message, messageSize, detail);
        return false;
    }

    char line[MAP_LINE_SIZE];
    int lineNumber = 0;
    while (fgets(line, sizeof(line), file))
    {
        lineNumber++;
        char *entry = Trim(line);
        if (!entry[0] || entry[0] == '#') continue;
        if (map->propCount >= MAX_MAP_PROPS)
        {
            fclose(file);
            SetMessage(message, messageSize, "map has too many decorative props");
            return false;
        }

        MapPropDefinition prop = { 0 };
        char palette[16] = { 0 };
        int fields = sscanf(entry, "%47s %f %f %f %f %f %f %f %15s %f",
                            prop.asset,
                            &prop.position.x, &prop.position.y, &prop.position.z,
                            &prop.yawDegrees,
                            &prop.scale.x, &prop.scale.y, &prop.scale.z,
                            palette, &prop.emissive);
        if (fields != 10 || prop.scale.x <= 0.0f || prop.scale.y <= 0.0f ||
            prop.scale.z <= 0.0f || prop.emissive < 0.0f || prop.emissive > 1.0f ||
            (strcmp(palette, "orange") != 0 && strcmp(palette, "purple") != 0))
        {
            fclose(file);
            char detail[MAP_LINE_SIZE];
            snprintf(detail, sizeof(detail), "%s line %d is not a valid prop", path, lineNumber);
            SetMessage(message, messageSize, detail);
            return false;
        }
        prop.palette = strcmp(palette, "purple") == 0
                     ? MAP_PALETTE_PURPLE : MAP_PALETTE_ORANGE;
        map->props[map->propCount++] = prop;
    }
    fclose(file);
    return true;
}

static bool IsWalkable(char terrain)
{
    return terrain != '#' && terrain != 'c';
}

static bool SpawnCanReachVent(const MapDefinition *map, int sx, int sz, int gx, int gz)
{
    bool seen[MAX_ARENA_HEIGHT][MAX_ARENA_WIDTH] = { 0 };
    short queueX[MAX_ARENA_WIDTH*MAX_ARENA_HEIGHT];
    short queueZ[MAX_ARENA_WIDTH*MAX_ARENA_HEIGHT];
    int head = 0, tail = 0;
    queueX[tail] = (short)sx;
    queueZ[tail++] = (short)sz;
    seen[sz][sx] = true;

    static const int DX[4] = { 1, -1, 0, 0 };
    static const int DZ[4] = { 0, 0, 1, -1 };
    while (head < tail)
    {
        int x = queueX[head], z = queueZ[head++];
        if (x == gx && z == gz) return true;
        for (int direction = 0; direction < 4; direction++)
        {
            int nx = x + DX[direction], nz = z + DZ[direction];
            if (nx < 0 || nx >= map->width || nz < 0 || nz >= map->height ||
                seen[nz][nx] || !IsWalkable(map->terrain[nz][nx])) continue;
            seen[nz][nx] = true;
            queueX[tail] = (short)nx;
            queueZ[tail++] = (short)nz;
        }
    }
    return false;
}

static bool ValidateMap(const MapDefinition *map, char *message, int messageSize)
{
    int playerSpawns = 0, enemySpawns = 0, vents = 0;
    int ventX = -1, ventZ = -1;

    for (int z = 0; z < map->height; z++)
    {
        for (int x = 0; x < map->width; x++)
        {
            char terrain = map->terrain[z][x];
            char gameplay = map->gameplay[z][x];
            if (!strchr(".#cb", terrain))
            {
                SetMessage(message, messageSize, "terrain layer contains an unknown tile");
                return false;
            }
            if (!strchr(".PEV", gameplay))
            {
                SetMessage(message, messageSize, "gameplay layer contains an unknown marker");
                return false;
            }
            if ((x == 0 || z == 0 || x == map->width - 1 || z == map->height - 1) &&
                terrain != '#')
            {
                SetMessage(message, messageSize, "map border must be sealed with walls");
                return false;
            }
            if (gameplay != '.' && !IsWalkable(terrain))
            {
                SetMessage(message, messageSize, "gameplay marker overlaps solid terrain");
                return false;
            }
            if (gameplay == 'P') playerSpawns++;
            if (gameplay == 'E') enemySpawns++;
            if (gameplay == 'V') { vents++; ventX = x; ventZ = z; }
        }
    }

    if (playerSpawns < 1 || enemySpawns < 1 || vents != 1)
    {
        SetMessage(message, messageSize, "map needs player/enemy spawns and exactly one vent");
        return false;
    }

    for (int z = 0; z < map->height; z++)
    {
        for (int x = 0; x < map->width; x++)
        {
            char marker = map->gameplay[z][x];
            if ((marker == 'P' || marker == 'E') &&
                !SpawnCanReachVent(map, x, z, ventX, ventZ))
            {
                SetMessage(message, messageSize, "a spawn cannot reach the objective");
                return false;
            }
        }
    }
    return true;
}

bool MapDefinitionLoad(const char *directory, MapDefinition *out,
                       char *message, int messageSize)
{
    if (!directory || !out)
    {
        SetMessage(message, messageSize, "map directory and output are required");
        return false;
    }

    MapDefinition candidate = { 0 };
    candidate.tileSize = 2.0f;
    char terrainFile[128] = { 0 };
    char gameplayFile[128] = { 0 };
    char visualFile[128] = { 0 };
    char propsFile[128] = { 0 };
    char configPath[MAP_PATH_SIZE];
    if (!JoinPath(configPath, sizeof(configPath), directory, "map.cfg"))
    {
        SetMessage(message, messageSize, "map path is too long");
        return false;
    }

    FILE *file = fopen(configPath, "r");
    if (!file)
    {
        char detail[MAP_LINE_SIZE];
        snprintf(detail, sizeof(detail), "cannot open map metadata: %s", configPath);
        SetMessage(message, messageSize, detail);
        return false;
    }

    char line[MAP_LINE_SIZE];
    while (fgets(line, sizeof(line), file))
    {
        char *entry = Trim(line);
        if (!entry[0] || entry[0] == '#') continue;
        char *equals = strchr(entry, '=');
        if (!equals)
        {
            fclose(file);
            SetMessage(message, messageSize, "map metadata entry is missing '='");
            return false;
        }
        *equals = '\0';
        char *key = Trim(entry);
        char *value = Trim(equals + 1);
        if (strcmp(key, "version") == 0) candidate.formatVersion = atoi(value);
        else if (strcmp(key, "id") == 0) snprintf(candidate.id, sizeof(candidate.id), "%s", value);
        else if (strcmp(key, "name") == 0) snprintf(candidate.name, sizeof(candidate.name), "%s", value);
        else if (strcmp(key, "width") == 0) candidate.width = atoi(value);
        else if (strcmp(key, "height") == 0) candidate.height = atoi(value);
        else if (strcmp(key, "tile_size") == 0) candidate.tileSize = strtof(value, NULL);
        else if (strcmp(key, "terrain") == 0) snprintf(terrainFile, sizeof(terrainFile), "%s", value);
        else if (strcmp(key, "gameplay") == 0) snprintf(gameplayFile, sizeof(gameplayFile), "%s", value);
        else if (strcmp(key, "visual") == 0) snprintf(visualFile, sizeof(visualFile), "%s", value);
        else if (strcmp(key, "props") == 0) snprintf(propsFile, sizeof(propsFile), "%s", value);
        else
        {
            fclose(file);
            SetMessage(message, messageSize, "map metadata contains an unknown key");
            return false;
        }
    }
    fclose(file);

    if (candidate.formatVersion != MAP_FORMAT_VERSION || !candidate.id[0] ||
        !candidate.name[0] || candidate.width < 5 || candidate.width > MAX_ARENA_WIDTH ||
        candidate.height < 5 || candidate.height > MAX_ARENA_HEIGHT ||
        candidate.tileSize < 0.5f || candidate.tileSize > 10.0f ||
        !terrainFile[0] || !gameplayFile[0] || !visualFile[0] || !propsFile[0])
    {
        SetMessage(message, messageSize, "map metadata is incomplete or outside supported limits");
        return false;
    }

    char path[MAP_PATH_SIZE];
    if (!JoinPath(path, sizeof(path), directory, terrainFile) ||
        !ReadLayer(path, candidate.width, candidate.height, candidate.terrain,
                   message, messageSize)) return false;
    if (!JoinPath(path, sizeof(path), directory, gameplayFile) ||
        !ReadLayer(path, candidate.width, candidate.height, candidate.gameplay,
                   message, messageSize)) return false;
    if (!JoinPath(path, sizeof(path), directory, visualFile) ||
        !ReadVisualLayer(path, &candidate, message, messageSize)) return false;
    if (!JoinPath(path, sizeof(path), directory, propsFile) ||
        !ReadProps(path, &candidate, message, messageSize)) return false;
    if (!ValidateMap(&candidate, message, messageSize)) return false;

    *out = candidate;
    SetMessage(message, messageSize, "map is valid");
    return true;
}

const MapDefinition *MapCatalogFind(const ContentCatalog *catalog, const char *id)
{
    if (!catalog || !id) return NULL;
    for (int i = 0; i < catalog->mapCount; i++)
        if (strcmp(catalog->maps[i].id, id) == 0) return &catalog->maps[i];
    return NULL;
}

const MapDefinition *MapCatalogSelected(const ContentCatalog *catalog)
{
    if (!catalog || catalog->mapCount <= 0) return NULL;
    int selected = catalog->selectedMap;
    if (selected < 0 || selected >= catalog->mapCount) selected = 0;
    return &catalog->maps[selected];
}

bool MapCatalogLoad(ContentCatalog *catalog, const char *manifestPath,
                    char *message, int messageSize)
{
    if (!catalog || !manifestPath)
    {
        SetMessage(message, messageSize, "catalog and manifest path are required");
        return false;
    }

    FILE *file = fopen(manifestPath, "r");
    if (!file)
    {
        SetMessage(message, messageSize, "cannot open map manifest");
        return false;
    }

    char base[MAP_PATH_SIZE] = ".";
    const char *slash = strrchr(manifestPath, '/');
    if (slash)
    {
        size_t length = (size_t)(slash - manifestPath);
        if (length >= sizeof(base)) length = sizeof(base) - 1;
        memcpy(base, manifestPath, length);
        base[length] = '\0';
    }

    MapDefinition loaded[MAX_MAPS] = { 0 };
    int count = 0;
    int version = 0;
    char defaultId[MAP_ID_SIZE] = { 0 };
    char line[MAP_LINE_SIZE];
    while (fgets(line, sizeof(line), file))
    {
        char *entry = Trim(line);
        if (!entry[0] || entry[0] == '#') continue;
        char *equals = strchr(entry, '=');
        if (!equals)
        {
            fclose(file);
            SetMessage(message, messageSize, "map manifest entry is missing '='");
            return false;
        }
        *equals = '\0';
        char *key = Trim(entry);
        char *value = Trim(equals + 1);
        if (strcmp(key, "version") == 0) version = atoi(value);
        else if (strcmp(key, "default") == 0) snprintf(defaultId, sizeof(defaultId), "%s", value);
        else if (strcmp(key, "map") == 0)
        {
            if (count >= MAX_MAPS)
            {
                fclose(file);
                SetMessage(message, messageSize, "map manifest exceeds MAX_MAPS");
                return false;
            }
            char directory[MAP_PATH_SIZE];
            if (!JoinPath(directory, sizeof(directory), base, value) ||
                !MapDefinitionLoad(directory, &loaded[count], message, messageSize))
            {
                fclose(file);
                return false;
            }
            if (strcmp(loaded[count].id, value) != 0)
            {
                fclose(file);
                SetMessage(message, messageSize, "map directory and declared id differ");
                return false;
            }
            for (int prior = 0; prior < count; prior++)
            {
                if (strcmp(loaded[prior].id, loaded[count].id) == 0)
                {
                    fclose(file);
                    SetMessage(message, messageSize, "map manifest contains a duplicate id");
                    return false;
                }
            }
            count++;
        }
        else
        {
            fclose(file);
            SetMessage(message, messageSize, "map manifest contains an unknown key");
            return false;
        }
    }
    fclose(file);

    if (version != MAP_FORMAT_VERSION || count == 0 || !defaultId[0])
    {
        SetMessage(message, messageSize, "map manifest is incomplete or unsupported");
        return false;
    }

    int selected = -1;
    for (int i = 0; i < count; i++)
        if (strcmp(loaded[i].id, defaultId) == 0) selected = i;
    if (selected < 0)
    {
        SetMessage(message, messageSize, "default map id is not in the manifest");
        return false;
    }

    memcpy(catalog->maps, loaded, sizeof(MapDefinition)*(size_t)count);
    catalog->mapCount = count;
    catalog->selectedMap = selected;
    SetMessage(message, messageSize, "map catalog loaded");
    return true;
}
