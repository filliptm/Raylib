#include "environment.h"

#include "arena.h"
#include "render.h"
#include "render_state.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <string.h>

// Helios-9 keeps the gameplay silhouettes readable with dark structural cores, while
// Kenney's atlas supplies the brighter modular panels and orange/purple identity.
static const Color FLOOR_TINT = { 112, 126, 154, 255 };
static const Color WALL_CORE = { 58, 68, 92, 255 };
static const Color WALL_BASE = { 40, 47, 66, 255 };
static const Color STATION_WHITE = { 238, 242, 250, 255 };
static const Color CARGO_DARK = { 132, 106, 86, 255 };

typedef struct FaceDirection {
    int dx;
    int dz;
    float yaw;
} FaceDirection;

static const FaceDirection WALL_FACES[4] = {
    {  0, -1,  0.0f },
    {  1,  0, -PI*0.5f },
    {  0,  1,  PI },
    { -1,  0,  PI*0.5f }
};

static Matrix EnvTRS(Vector3 scale, float yaw, Vector3 position)
{
    Matrix m = MatrixScale(scale.x, scale.y, scale.z);
    if (yaw != 0.0f) m = MatrixMultiply(m, MatrixRotateY(yaw));
    return MatrixMultiply(m, MatrixTranslate(position.x, position.y, position.z));
}

static float AlignModelTop(float sourceHeight, float scaleY)
{
    return ARENA_INLAY_TOP_Y - sourceHeight*scaleY;
}

static Color MixColor(Color a, Color b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r)*t),
        (unsigned char)(a.g + (b.g - a.g)*t),
        (unsigned char)(a.b + (b.b - a.b)*t),
        (unsigned char)(a.a + (b.a - a.a)*t)
    };
}

static unsigned int TileHash(int tx, int tz)
{
    unsigned int h = (unsigned int)(tx*374761393u) ^ (unsigned int)(tz*668265263u);
    h = (h ^ (h >> 13))*1274126177u;
    return h ^ (h >> 16);
}

static StationPalette TilePalette(const Arena *arena, int tx, int tz)
{
    return arena->visual[tz][tx].palette == MAP_PALETTE_PURPLE
         ? STATION_PALETTE_PURPLE : STATION_PALETTE_ORANGE;
}

static bool IsWall(const Arena *arena, int tx, int tz)
{
    // Outside the map behaves as more station hull. This draws only the inward-facing
    // side of border tiles instead of wasting geometry on an invisible exterior face.
    if (!ArenaInBounds(arena, tx, tz)) return true;
    return arena->tiles[tz][tx].type == TILE_WALL;
}

static void DrawGroundGlow(Assets *assets, Vector3 position, float radius, Color tint)
{
    // Queued into the renderer's shared decal pass rather than paying a
    // batch-flushing depth-write bracket per decal.
    (void)assets;
    RenderQueueGroundGlow(position, radius, tint);
}

typedef struct GroundRect GroundRect;
static bool TileVisible(const GroundRect *view, const Arena *arena, int tx, int tz);

static void DrawDeck(const Arena *arena, Assets *assets, const GroundRect *view)
{
    float worldW = arena->width*arena->tileSize;
    float worldH = arena->height*arena->tileSize;

    Matrix floor = EnvTRS((Vector3){ worldW, 1.0f, worldH }, 0.0f,
                          (Vector3){ 0.0f, ARENA_DECK_Y, 0.0f });
    DrawLit(assets, assets->plane, floor, assets->texFloor, FLOOR_TINT,
            (Vector2){ (float)arena->width, (float)arena->height }, 0.0f);

    for (int tz = 0; tz < arena->height; tz++)
    {
        for (int tx = 0; tx < arena->width; tx++)
        {
            if (arena->visual[tz][tx].kind != MAP_VISUAL_FLOOR_ACCENT) continue;
            if (!TileVisible(view, arena, tx, tz)) continue;
            Vector3 center = ArenaTileCenter(arena, tx, tz);
            Vector3 scale = { 1.95f, 1.95f, 1.95f };
            Matrix accent = EnvTRS(scale, ((tx + tz) & 1) ? PI*0.5f : 0.0f,
                                   (Vector3){ center.x, AlignModelTop(0.3250001f, scale.y),
                                              center.z });
            AssetsDrawStationModel(assets, STATION_FLOOR_DETAIL, accent,
                                   TilePalette(arena, tx, tz),
                                   (Color){ 214, 220, 242, 255 }, 0.03f);
        }
    }
}

static void DrawSpawnPads(const Arena *arena, Assets *assets)
{
    for (int team = TEAM_PLAYER; team <= TEAM_ENEMY; team++)
    {
        const Vector3 *spawns = team == TEAM_PLAYER
                              ? arena->playerSpawns : arena->enemySpawns;
        int count = team == TEAM_PLAYER
                  ? arena->playerSpawnCount : arena->enemySpawnCount;
        Color glow = team == TEAM_PLAYER
                   ? (Color){ 76, 156, 255, 42 } : (Color){ 255, 92, 108, 38 };

        for (int i = 0; i < count; i++)
        {
            Vector3 p = spawns[i];
            Vector3 padScale = { 2.25f, 1.8f, 2.25f };
            Matrix pad = EnvTRS(padScale, 0.0f,
                                (Vector3){ p.x, AlignModelTop(0.125f, padScale.y), p.z });
            AssetsDrawStationModel(assets, STATION_STRUCTURE_PANEL, pad,
                                   STATION_PALETTE_ORANGE, STATION_WHITE, 0.02f);
            DrawGroundGlow(assets, p, 1.05f, glow);
        }
    }
}

static StationModelId WallFaceModel(const Arena *arena, int tx, int tz,
                                    StationPalette palette)
{
    bool border = tx == 0 || tz == 0 || tx == arena->width - 1 || tz == arena->height - 1;
    MapVisualKind kind = (MapVisualKind)arena->visual[tz][tx].kind;

    if (kind == MAP_VISUAL_DOOR) return STATION_DOOR_DOUBLE_CLOSED;
    if (kind == MAP_VISUAL_DISPLAY) return STATION_DISPLAY_WALL;
    if (border && ((tx + tz) % 5 == 0)) return STATION_WALL_WINDOW;
    if (palette == STATION_PALETTE_PURPLE && ((tx*3 + tz) & 3) == 0)
        return STATION_WALL_BANNER;
    if (((tx + tz) % 7) == 0) return STATION_WALL_PILLAR;
    return STATION_WALL;
}

static void DrawWallFace(const Arena *arena, Assets *assets, int tx, int tz, Vector3 center,
                         FaceDirection face, StationPalette palette)
{
    StationModelId id = WallFaceModel(arena, tx, tz, palette);
    float tileSize = arena->tileSize;
    float inset = 0.15f;
    Vector3 position = {
        center.x + face.dx*(tileSize*0.5f - inset),
        0.0f,
        center.z + face.dz*(tileSize*0.5f - inset)
    };
    Vector3 scale = { tileSize, WALL_HEIGHT, 1.0f };

    if (id == STATION_DOOR_DOUBLE_CLOSED)
    {
        scale = (Vector3){ 3.2f, 3.35f, 2.0f };
        inset = 0.05f;
        position.x = center.x + face.dx*(tileSize*0.5f - inset);
        position.z = center.z + face.dz*(tileSize*0.5f - inset);
    }
    else if (id == STATION_DISPLAY_WALL)
    {
        scale = (Vector3){ 4.4f, 4.8f, 2.0f };
        position.y = 0.04f;
    }
    else if (id == STATION_WALL_PILLAR)
        scale = (Vector3){ tileSize, WALL_HEIGHT, 0.62f };

    Matrix model = EnvTRS(scale, face.yaw, position);
    if (AssetsDrawStationModel(assets, id, model, palette, STATION_WHITE, 0.0f))
        return;

    // A missing GLB must never create an invisible solid edge.
    Vector3 fallback = { position.x, WALL_HEIGHT*0.5f, position.z };
    Matrix panel = EnvTRS((Vector3){ tileSize, WALL_HEIGHT, 0.18f },
                          face.yaw, fallback);
    DrawLit(assets, assets->cube, panel, assets->texWall,
            (Color){ 120, 134, 166, 255 }, (Vector2){ 1.0f, 1.0f }, 0.0f);
}

static void DrawWallTile(const Arena *arena, Assets *assets, int tx, int tz)
{
    Vector3 c = ArenaTileCenter(arena, tx, tz);
    StationPalette palette = TilePalette(arena, tx, tz);
    float tileSize = arena->tileSize;

    // Collision still occupies the full tile, while the visible structural core is
    // recessed behind the imported wall skins. Their shallow volumetric intersection
    // closes bevel gaps without placing broad surfaces at nearly equal depths.
    Matrix core = EnvTRS((Vector3){ tileSize*0.86f, WALL_HEIGHT*0.94f, tileSize*0.86f },
                         0.0f, (Vector3){ c.x, WALL_HEIGHT*0.47f, c.z });
    DrawLit(assets, assets->cube, core, assets->texMetal, WALL_CORE,
            (Vector2){ 1.0f, 1.0f }, 0.0f);

    // Keep each plinth strictly inside its tile. The previous 1.04 footprint made
    // neighboring wall cubes overlap by 4% of a tile, leaving coplanar top strips
    // for the depth buffer to alternate between as the camera moved.
    Matrix plinth = EnvTRS((Vector3){ tileSize*0.98f, 0.26f, tileSize*0.98f },
                           0.0f, (Vector3){ c.x, 0.13f, c.z });
    DrawLit(assets, assets->cube, plinth, assets->texMetal, WALL_BASE,
            (Vector2){ 1.0f, 1.0f }, 0.0f);

    Matrix cap = EnvTRS((Vector3){ 2.32f, 1.65f, 2.32f },
                        ((tx + tz) & 1) ? PI*0.5f : 0.0f,
                        (Vector3){ c.x, WALL_HEIGHT - 0.20f, c.z });
    if (!AssetsDrawStationModel(assets, STATION_STRUCTURE_PANEL, cap,
                                palette, STATION_WHITE, 0.0f))
    {
        Matrix fallback = EnvTRS((Vector3){ tileSize, 0.18f, tileSize }, 0.0f,
                                 (Vector3){ c.x, WALL_HEIGHT - 0.09f, c.z });
        DrawLit(assets, assets->cube, fallback, assets->texWall,
                (Color){ 126, 140, 172, 255 }, (Vector2){ 1.0f, 1.0f }, 0.0f);
    }

    for (int i = 0; i < 4; i++)
    {
        FaceDirection face = WALL_FACES[i];
        if (!IsWall(arena, tx + face.dx, tz + face.dz))
            DrawWallFace(arena, assets, tx, tz, c, face, palette);
    }
}

static void DrawCrateTile(const Arena *arena, const Tile *tile, Assets *assets,
                          int tx, int tz, int maxHealth)
{
    Vector3 c = ArenaTileCenter(arena, tx, tz);
    unsigned int variant = TileHash(tx, tz);
    StationPalette palette = TilePalette(arena, tx, tz);
    MapVisualKind style = (MapVisualKind)arena->visual[tz][tx].kind;
    StationModelId id;
    Vector3 scale;

    // The left half reads as cargo reclamation. The right half uses computers and power
    // banks with the same destructible gameplay contract, preserving lane fairness.
    if (style == MAP_VISUAL_CARGO ||
        (style != MAP_VISUAL_TECH && tx < arena->width/2))
    {
        switch (variant % 3)
        {
            case 1:
                id = STATION_CONTAINER_WIDE;
                scale = (Vector3){ 2.9f, 2.7f, 2.9f };
                break;
            case 2:
                id = STATION_CONTAINER_TALL;
                scale = (Vector3){ 2.8f, 2.1f, 2.8f };
                break;
            default:
                id = STATION_CONTAINER;
                scale = (Vector3){ 3.0f, 3.0f, 3.0f };
                break;
        }
    }
    else
    {
        id = (variant & 1) ? STATION_COMPUTER_SYSTEM : STATION_COMPUTER_WIDE;
        scale = (id == STATION_COMPUTER_SYSTEM)
              ? (Vector3){ 2.0f, 3.0f, 2.0f }
              : (Vector3){ 2.2f, 3.6f, 2.2f };
    }

    if (maxHealth < 1) maxHealth = 1;
    float hp = (float)tile->health/(float)maxHealth;
    if (hp < 0.0f) hp = 0.0f;
    if (hp > 1.0f) hp = 1.0f;
    Color tint = MixColor(CARGO_DARK, STATION_WHITE, 0.52f + hp*0.48f);
    if (tile->hitFlash > 0.0f) tint = MixColor(tint, WHITE, tile->hitFlash);

    Vector3 baseScale = { 2.12f, 1.35f, 2.12f };
    Matrix base = EnvTRS(baseScale,
                         (variant & 1) ? PI*0.5f : 0.0f,
                         (Vector3){ c.x, AlignModelTop(0.125f, baseScale.y), c.z });
    AssetsDrawStationModel(assets, STATION_STRUCTURE_PANEL, base,
                           palette, tint, 0.0f);

    float yaw = (float)(variant & 3u)*PI*0.5f;
    Matrix model = EnvTRS(scale, yaw,
                          (Vector3){ c.x, ARENA_INLAY_TOP_Y, c.z });
    bool drawn = AssetsDrawStationModel(assets, id, model, palette, tint,
                                        tile->hitFlash*0.18f);
    if (!drawn)
    {
        float size = arena->tileSize*0.9f;
        Matrix fallback = EnvTRS((Vector3){ size, CRATE_HEIGHT, size }, 0.0f,
                                 (Vector3){ c.x, ARENA_INLAY_TOP_Y + CRATE_HEIGHT*0.5f, c.z });
        DrawLit(assets, assets->cube, fallback, assets->texCrate, tint,
                (Vector2){ 1.0f, 1.0f }, 0.0f);
    }

    DrawGroundGlow(assets, c, arena->tileSize*0.62f, (Color){ 0, 0, 0, 85 });
}

static void DrawHydroponicTile(const Arena *arena, Assets *assets, int tx, int tz)
{
    Vector3 c = ArenaTileCenter(arena, tx, tz);
    StationPalette palette = TilePalette(arena, tx, tz);
    Vector3 bedScale = { 2.25f, 1.8f, 2.25f };
    Matrix bed = EnvTRS(bedScale,
                        ((tx + tz) & 1) ? PI*0.5f : 0.0f,
                        (Vector3){ c.x, AlignModelTop(0.125f, bedScale.y), c.z });
    AssetsDrawStationModel(assets, STATION_STRUCTURE_PANEL, bed,
                           palette, (Color){ 202, 220, 210, 255 }, 0.0f);
    DrawGroundGlow(assets, c, arena->tileSize*0.66f, (Color){ 12, 38, 24, 145 });
}

typedef struct PropAsset {
    const char *name;
    StationModelId model;
} PropAsset;

static const PropAsset PROP_ASSETS[] = {
    { "floor_panel", STATION_FLOOR_PANEL },
    { "floor_detail", STATION_FLOOR_DETAIL },
    { "structure_panel", STATION_STRUCTURE_PANEL },
    { "structure", STATION_STRUCTURE },
    { "structure_barrier", STATION_STRUCTURE_BARRIER },
    { "wall", STATION_WALL },
    { "wall_corner", STATION_WALL_CORNER },
    { "wall_pillar", STATION_WALL_PILLAR },
    { "wall_window", STATION_WALL_WINDOW },
    { "wall_banner", STATION_WALL_BANNER },
    { "door_double_closed", STATION_DOOR_DOUBLE_CLOSED },
    { "container", STATION_CONTAINER },
    { "container_wide", STATION_CONTAINER_WIDE },
    { "container_tall", STATION_CONTAINER_TALL },
    { "computer_system", STATION_COMPUTER_SYSTEM },
    { "computer_wide", STATION_COMPUTER_WIDE },
    { "display_wall", STATION_DISPLAY_WALL },
    { "pipe", STATION_PIPE },
    { "pipe_bend", STATION_PIPE_BEND },
    { "rail", STATION_RAIL },
    { "table_display_planet", STATION_TABLE_DISPLAY_PLANET },
    { "skip", STATION_SKIP }
};

static bool ResolvePropAsset(const char *name, StationModelId *out)
{
    for (int i = 0; i < (int)(sizeof(PROP_ASSETS)/sizeof(PROP_ASSETS[0])); i++)
    {
        if (strcmp(PROP_ASSETS[i].name, name) != 0) continue;
        *out = PROP_ASSETS[i].model;
        return true;
    }
    return false;
}

static void DrawMapProps(const Arena *arena, Assets *assets)
{
    for (int i = 0; i < arena->propCount; i++)
    {
        const MapPropDefinition *prop = &arena->props[i];
        StationModelId model;
        if (!ResolvePropAsset(prop->asset, &model)) continue;
        StationPalette palette = prop->palette == MAP_PALETTE_PURPLE
                               ? STATION_PALETTE_PURPLE : STATION_PALETTE_ORANGE;
        AssetsDrawStationModel(assets, model,
                               EnvTRS(prop->scale, prop->yawDegrees*DEG2RAD, prop->position),
                               palette, STATION_WHITE, prop->emissive);
    }
}

// World-space ground rectangle visible to the top-down camera, from the four
// corner rays intersected with the floor and wall-top planes. Falls back to
// "everything" if a ray runs parallel to the ground.
struct GroundRect {
    float minX, maxX, minZ, maxZ;
    bool valid;
};

static GroundRect VisibleGroundRect(const Camera3D *camera)
{
    GroundRect r = { 1e9f, -1e9f, 1e9f, -1e9f, true };
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    Vector2 corners[4] = { { 0, 0 }, { w, 0 }, { 0, h }, { w, h } };
    float planes[2] = { 0.0f, WALL_HEIGHT };

    for (int i = 0; i < 4; i++)
    {
        Ray ray = GetScreenToWorldRay(corners[i], *camera);
        if (fabsf(ray.direction.y) < 0.0001f) { r.valid = false; return r; }
        for (int p = 0; p < 2; p++)
        {
            float t = (planes[p] - ray.position.y)/ray.direction.y;
            if (t < 0.0f) { r.valid = false; return r; }
            float x = ray.position.x + ray.direction.x*t;
            float z = ray.position.z + ray.direction.z*t;
            if (x < r.minX) r.minX = x;
            if (x > r.maxX) r.maxX = x;
            if (z < r.minZ) r.minZ = z;
            if (z > r.maxZ) r.maxZ = z;
        }
    }
    return r;
}

static bool TileVisible(const GroundRect *view, const Arena *arena, int tx, int tz)
{
    if (!view->valid) return true;
    Vector3 c = ArenaTileCenter(arena, tx, tz);
    float margin = arena->tileSize*1.5f;
    return c.x >= view->minX - margin && c.x <= view->maxX + margin &&
           c.z >= view->minZ - margin && c.z <= view->maxZ + margin;
}

// The station floats in space: a starfield plane and a planet disc sit well below
// deck level, so the map's edges open onto sky instead of a flat clear colour and
// camera movement gives them a little parallax. Both are fullbright (emissive 1);
// the distance fog still dims them slightly, which reads as depth.
#define BACKDROP_Y (-16.0f)

static void DrawBackdrop(const App *w, const Arena *arena, Assets *assets)
{
    float glow = w->tune.backdropBrightness;
    if (glow <= 0.01f) return;

    float worldW = arena->width*arena->tileSize;
    float worldH = arena->height*arena->tileSize;
    float span = fmaxf(worldW, worldH)*4.0f;
    unsigned char level = (unsigned char)Clamp(glow*255.0f, 0.0f, 255.0f);
    Color tint = { level, level, level, 255 };

    Matrix sky = EnvTRS((Vector3){ span, 1.0f, span }, 0.0f,
                        (Vector3){ 0.0f, BACKDROP_Y, 0.0f });
    DrawLit(assets, assets->plane, sky, assets->texStarfield, tint,
            (Vector2){ 3.0f, 3.0f }, 1.0f);

    // One large planet past the north-west corner, slowly drifting into view when
    // fights reach that side of the station.
    float planetSize = span*0.17f;
    Matrix planet = EnvTRS((Vector3){ planetSize, 1.0f, planetSize }, 0.6f,
                           (Vector3){ -worldW*0.9f, BACKDROP_Y + 2.0f, worldH*0.85f });
    DrawLit(assets, assets->plane, planet, assets->texPlanet, tint,
            (Vector2){ 1.0f, 1.0f }, 1.0f);
}

void EnvironmentDraw(const App *w, Assets *assets)
{
    const Arena *arena = &w->session.arena;
    GroundRect view = VisibleGroundRect(&w->presentation.camera);

    AssetsSetDither(assets, 0.0f);
    DrawBackdrop(w, arena, assets);
    DrawDeck(arena, assets, &view);
    DrawSpawnPads(arena, assets);

    for (int tz = 0; tz < arena->height; tz++)
    {
        for (int tx = 0; tx < arena->width; tx++)
        {
            const Tile *tile = &arena->tiles[tz][tx];
            if (tile->type == TILE_FLOOR) continue;
            // Off-screen structures are the bulk of a large map's draw calls.
            if (!TileVisible(&view, arena, tx, tz)) continue;
            if (tile->type == TILE_WALL) DrawWallTile(arena, assets, tx, tz);
            else if (tile->type == TILE_CRATE)
                DrawCrateTile(arena, tile, assets, tx, tz, w->tune.crateHealth);
            else if (tile->type == TILE_BUSH) DrawHydroponicTile(arena, assets, tx, tz);
        }
    }

    DrawMapProps(arena, assets);
}
