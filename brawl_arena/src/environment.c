#include "environment.h"

#include "arena.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>

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

static StationPalette TilePalette(int tx, int tz)
{
    int dx = abs(tx - ARENA_W/2);
    int dz = abs(tz - ARENA_H/2);
    return (dx <= 6 && dz <= 4) ? STATION_PALETTE_PURPLE : STATION_PALETTE_ORANGE;
}

static bool IsWall(const Arena *arena, int tx, int tz)
{
    // Outside the map behaves as more station hull. This draws only the inward-facing
    // side of border tiles instead of wasting geometry on an invisible exterior face.
    if (!ArenaInBounds(tx, tz)) return true;
    return arena->tiles[tz][tx].type == TILE_WALL;
}

static void DrawGroundGlow(Assets *assets, Vector3 position, float radius, Color tint)
{
    rlDisableDepthMask();
    Matrix m = EnvTRS((Vector3){ radius*2.0f, 1.0f, radius*2.0f }, 0.0f,
                      (Vector3){ position.x, ARENA_DECAL_Y, position.z });
    DrawLit(assets, assets->plane, m, assets->texGlow, tint,
            (Vector2){ 1.0f, 1.0f }, 1.0f);
    rlEnableDepthMask();
}

static void DrawDeck(Assets *assets)
{
    float worldW = ARENA_W*TILE_SIZE;
    float worldH = ARENA_H*TILE_SIZE;

    Matrix floor = EnvTRS((Vector3){ worldW, 1.0f, worldH }, 0.0f,
                          (Vector3){ 0.0f, ARENA_DECK_Y, 0.0f });
    DrawLit(assets, assets->plane, floor, assets->texFloor, FLOOR_TINT,
            (Vector2){ (float)ARENA_W, (float)ARENA_H }, 0.0f);

    // The central violet reactor plate establishes the objective before the first gem
    // appears. Every imported floor piece is top-aligned to the same inset layer, clear
    // of both the deck below and gameplay decals above.
    Vector3 coreScale = { 3.2f, 2.8f, 3.2f };
    Matrix core = EnvTRS((Vector3){ 3.2f, 2.8f, 3.2f }, PI*0.25f,
                         (Vector3){ 0.0f, AlignModelTop(0.3000001f, coreScale.y), 0.0f });
    AssetsDrawStationModel(assets, STATION_FLOOR_PANEL, core,
                           STATION_PALETTE_PURPLE, STATION_WHITE, 0.08f);

    const int coreAccents[][2] = {
        { 14, 10 }, { 16, 10 }, { 18, 10 },
        { 14, 12 }, { 16, 12 }, { 18, 12 }
    };
    for (int i = 0; i < (int)(sizeof(coreAccents)/sizeof(coreAccents[0])); i++)
    {
        Vector3 c = ArenaTileCenter(coreAccents[i][0], coreAccents[i][1]);
        Vector3 accentScale = { 1.95f, 1.95f, 1.95f };
        Matrix accent = EnvTRS(accentScale,
                               (i & 1) ? PI*0.5f : 0.0f,
                               (Vector3){ c.x, AlignModelTop(0.3250001f, accentScale.y), c.z });
        AssetsDrawStationModel(assets, STATION_FLOOR_DETAIL, accent,
                               STATION_PALETTE_PURPLE,
                               (Color){ 214, 220, 242, 255 }, 0.03f);
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

static StationModelId WallFaceModel(int tx, int tz, FaceDirection face,
                                    StationPalette palette)
{
    bool border = tx == 0 || tz == 0 || tx == ARENA_W - 1 || tz == ARENA_H - 1;
    bool shieldDoor = tx == ARENA_W/2 && (tz == 4 || tz == ARENA_H - 5);

    if (shieldDoor) return STATION_DOOR_DOUBLE_CLOSED;
    if (border && ((tx + tz) % 5 == 0)) return STATION_WALL_WINDOW;
    if (border && tx == ARENA_W - 1 && face.dx < 0 && (tz == 8 || tz == 14))
        return STATION_DISPLAY_WALL;
    if (palette == STATION_PALETTE_PURPLE && ((tx*3 + tz) & 3) == 0)
        return STATION_WALL_BANNER;
    if (((tx + tz) % 7) == 0) return STATION_WALL_PILLAR;
    return STATION_WALL;
}

static void DrawWallFace(Assets *assets, int tx, int tz, Vector3 center,
                         FaceDirection face, StationPalette palette)
{
    StationModelId id = WallFaceModel(tx, tz, face, palette);
    float inset = 0.15f;
    Vector3 position = {
        center.x + face.dx*(TILE_SIZE*0.5f - inset),
        0.0f,
        center.z + face.dz*(TILE_SIZE*0.5f - inset)
    };
    Vector3 scale = { TILE_SIZE, WALL_HEIGHT, 1.0f };

    if (id == STATION_DOOR_DOUBLE_CLOSED)
    {
        scale = (Vector3){ 3.2f, 3.35f, 2.0f };
        inset = 0.05f;
        position.x = center.x + face.dx*(TILE_SIZE*0.5f - inset);
        position.z = center.z + face.dz*(TILE_SIZE*0.5f - inset);
    }
    else if (id == STATION_DISPLAY_WALL)
    {
        scale = (Vector3){ 4.4f, 4.8f, 2.0f };
        position.y = 0.04f;
    }
    else if (id == STATION_WALL_PILLAR)
        scale = (Vector3){ TILE_SIZE, WALL_HEIGHT, 0.62f };

    Matrix model = EnvTRS(scale, face.yaw, position);
    if (AssetsDrawStationModel(assets, id, model, palette, STATION_WHITE, 0.0f))
        return;

    // A missing GLB must never create an invisible solid edge.
    Vector3 fallback = { position.x, WALL_HEIGHT*0.5f, position.z };
    Matrix panel = EnvTRS((Vector3){ TILE_SIZE, WALL_HEIGHT, 0.18f },
                          face.yaw, fallback);
    DrawLit(assets, assets->cube, panel, assets->texWall,
            (Color){ 120, 134, 166, 255 }, (Vector2){ 1.0f, 1.0f }, 0.0f);
}

static void DrawWallTile(const Arena *arena, Assets *assets, int tx, int tz)
{
    Vector3 c = ArenaTileCenter(tx, tz);
    StationPalette palette = TilePalette(tx, tz);

    // The structural core and plinth occupy the same full tile as collision. Modular
    // wall faces sit just inside its exposed edges, and the imported panel closes the top.
    Matrix core = EnvTRS((Vector3){ TILE_SIZE*0.94f, WALL_HEIGHT*0.94f, TILE_SIZE*0.94f },
                         0.0f, (Vector3){ c.x, WALL_HEIGHT*0.47f, c.z });
    DrawLit(assets, assets->cube, core, assets->texMetal, WALL_CORE,
            (Vector2){ 1.0f, 1.0f }, 0.0f);

    Matrix plinth = EnvTRS((Vector3){ TILE_SIZE*1.04f, 0.26f, TILE_SIZE*1.04f },
                           0.0f, (Vector3){ c.x, 0.13f, c.z });
    DrawLit(assets, assets->cube, plinth, assets->texMetal, WALL_BASE,
            (Vector2){ 1.0f, 1.0f }, 0.0f);

    Matrix cap = EnvTRS((Vector3){ 2.32f, 1.65f, 2.32f },
                        ((tx + tz) & 1) ? PI*0.5f : 0.0f,
                        (Vector3){ c.x, WALL_HEIGHT - 0.20f, c.z });
    if (!AssetsDrawStationModel(assets, STATION_STRUCTURE_PANEL, cap,
                                palette, STATION_WHITE, 0.0f))
    {
        Matrix fallback = EnvTRS((Vector3){ TILE_SIZE, 0.18f, TILE_SIZE }, 0.0f,
                                 (Vector3){ c.x, WALL_HEIGHT - 0.09f, c.z });
        DrawLit(assets, assets->cube, fallback, assets->texWall,
                (Color){ 126, 140, 172, 255 }, (Vector2){ 1.0f, 1.0f }, 0.0f);
    }

    for (int i = 0; i < 4; i++)
    {
        FaceDirection face = WALL_FACES[i];
        if (!IsWall(arena, tx + face.dx, tz + face.dz))
            DrawWallFace(assets, tx, tz, c, face, palette);
    }
}

static void DrawCrateTile(const Tile *tile, Assets *assets, int tx, int tz, int maxHealth)
{
    Vector3 c = ArenaTileCenter(tx, tz);
    unsigned int variant = TileHash(tx, tz);
    StationPalette palette = TilePalette(tx, tz);
    StationModelId id;
    Vector3 scale;

    // The left half reads as cargo reclamation. The right half uses computers and power
    // banks with the same destructible gameplay contract, preserving lane fairness.
    if (tx < ARENA_W/2)
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
        float size = TILE_SIZE*0.9f;
        Matrix fallback = EnvTRS((Vector3){ size, CRATE_HEIGHT, size }, 0.0f,
                                 (Vector3){ c.x, ARENA_INLAY_TOP_Y + CRATE_HEIGHT*0.5f, c.z });
        DrawLit(assets, assets->cube, fallback, assets->texCrate, tint,
                (Vector2){ 1.0f, 1.0f }, 0.0f);
    }

    DrawGroundGlow(assets, c, TILE_SIZE*0.62f, (Color){ 0, 0, 0, 85 });
}

static void DrawHydroponicTile(Assets *assets, int tx, int tz)
{
    Vector3 c = ArenaTileCenter(tx, tz);
    StationPalette palette = TilePalette(tx, tz);
    Vector3 bedScale = { 2.25f, 1.8f, 2.25f };
    Matrix bed = EnvTRS(bedScale,
                        ((tx + tz) & 1) ? PI*0.5f : 0.0f,
                        (Vector3){ c.x, AlignModelTop(0.125f, bedScale.y), c.z });
    AssetsDrawStationModel(assets, STATION_STRUCTURE_PANEL, bed,
                           palette, (Color){ 202, 220, 210, 255 }, 0.0f);
    DrawGroundGlow(assets, c, TILE_SIZE*0.66f, (Color){ 12, 38, 24, 145 });
}

static void DrawExteriorSetDressing(Assets *assets)
{
    float edgeX = ARENA_W*TILE_SIZE*0.5f;
    float edgeZ = ARENA_H*TILE_SIZE*0.5f;

    // Support frames and warning barriers extend the station silhouette beyond the
    // collision rectangle without entering playable floor.
    const Vector3 supportPos[4] = {
        { -1, 0, -1 }, { 1, 0, -1 }, { -1, 0, 1 }, { 1, 0, 1 }
    };
    for (int i = 0; i < 4; i++)
    {
        Vector3 p = {
            supportPos[i].x*(edgeX + 1.15f),
            0.0f,
            supportPos[i].z*(edgeZ + 1.15f)
        };
        Matrix frame = EnvTRS((Vector3){ 2.4f, 2.8f, 2.4f }, i*PI*0.5f, p);
        AssetsDrawStationModel(assets, STATION_STRUCTURE, frame,
                               STATION_PALETTE_ORANGE, STATION_WHITE, 0.0f);
    }

    // A small cargo apron to the west and a monitoring station to the east add story
    // detail without implying walkable space.
    Vector3 cargo = { -edgeX - 2.0f, 0.0f, 2.0f };
    Vector3 apronScale = { 3.0f, 1.8f, 3.0f };
    Matrix cargoPad = EnvTRS(apronScale, 0.0f,
                             (Vector3){ cargo.x, AlignModelTop(0.125f, apronScale.y), cargo.z });
    AssetsDrawStationModel(assets, STATION_STRUCTURE_PANEL, cargoPad,
                           STATION_PALETTE_ORANGE, STATION_WHITE, 0.0f);
    AssetsDrawStationModel(assets, STATION_SKIP,
                           EnvTRS((Vector3){ 1.45f, 1.45f, 1.45f }, PI*0.5f, cargo),
                           STATION_PALETTE_ORANGE, STATION_WHITE, 0.0f);

    Vector3 observatory = { edgeX + 2.0f, 0.0f, -1.0f };
    Matrix observePad = EnvTRS(apronScale, 0.0f,
                               (Vector3){ observatory.x, AlignModelTop(0.125f, apronScale.y),
                                          observatory.z });
    AssetsDrawStationModel(assets, STATION_STRUCTURE_PANEL, observePad,
                           STATION_PALETTE_PURPLE, STATION_WHITE, 0.0f);
    AssetsDrawStationModel(assets, STATION_TABLE_DISPLAY_PLANET,
                           EnvTRS((Vector3){ 2.0f, 2.0f, 2.0f }, -PI*0.5f, observatory),
                           STATION_PALETTE_PURPLE, STATION_WHITE, 0.12f);

    for (int side = -1; side <= 1; side += 2)
    {
        Vector3 pipeBase = { side*8.0f, WALL_HEIGHT, -edgeZ + 0.65f };
        AssetsDrawStationModel(assets, STATION_PIPE,
                               EnvTRS((Vector3){ 1.7f, 2.2f, 1.7f }, 0.0f, pipeBase),
                               STATION_PALETTE_ORANGE, STATION_WHITE, 0.0f);
        AssetsDrawStationModel(assets, STATION_PIPE_BEND,
                               EnvTRS((Vector3){ 1.7f, 1.7f, 1.7f },
                                      side < 0 ? PI : 0.0f,
                                      (Vector3){ pipeBase.x, WALL_HEIGHT + 1.08f, pipeBase.z }),
                               STATION_PALETTE_PURPLE, STATION_WHITE, 0.02f);

        Vector3 railPos = { side*(edgeX - 5.0f), WALL_HEIGHT, edgeZ - 0.85f };
        AssetsDrawStationModel(assets, STATION_RAIL,
                               EnvTRS((Vector3){ 3.2f, 1.4f, 1.4f }, 0.0f, railPos),
                               STATION_PALETTE_ORANGE, STATION_WHITE, 0.0f);
    }

    for (int side = -1; side <= 1; side += 2)
    {
        Vector3 warning = { side*(edgeX + 1.0f), 0.0f, -8.0f };
        AssetsDrawStationModel(assets, STATION_STRUCTURE_BARRIER,
                               EnvTRS((Vector3){ 1.8f, 1.8f, 1.8f }, side*PI*0.5f, warning),
                               STATION_PALETTE_ORANGE, STATION_WHITE, 0.0f);
    }
}

void EnvironmentDraw(const World *w, Assets *assets)
{
    const Arena *arena = &w->arena;

    AssetsSetDither(assets, 0.0f);
    DrawDeck(assets);
    DrawSpawnPads(arena, assets);

    for (int tz = 0; tz < ARENA_H; tz++)
    {
        for (int tx = 0; tx < ARENA_W; tx++)
        {
            const Tile *tile = &arena->tiles[tz][tx];
            if (tile->type == TILE_WALL) DrawWallTile(arena, assets, tx, tz);
            else if (tile->type == TILE_CRATE)
                DrawCrateTile(tile, assets, tx, tz, w->tune.crateHealth);
            else if (tile->type == TILE_BUSH) DrawHydroponicTile(assets, tx, tz);
        }
    }

    DrawExteriorSetDressing(assets);
}
