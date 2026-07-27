#include "arena.h"
#include "brawler.h"
#include "content_catalog.h"
#include "game_random.h"
#include "weapons.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static void SetupOpenArena(Arena *arena)
{
    memset(arena, 0, sizeof(*arena));
    arena->width = 11;
    arena->height = 11;
    arena->tileSize = 2.0f;

    for (int z = 0; z < arena->height; z++)
    {
        for (int x = 0; x < arena->width; x++)
        {
            bool edge = x == 0 || z == 0 ||
                        x == arena->width - 1 || z == arena->height - 1;
            arena->tiles[z][x].type = edge ? TILE_WALL : TILE_FLOOR;
        }
    }
}

static void SetupBrawler(const ContentCatalog *content, Brawler *brawler,
                         Team team, BrawlerClass character, Vector3 position,
                         bool player)
{
    *brawler = (Brawler){ 0 };
    brawler->position = position;
    brawler->team = team;
    brawler->cls = character;
    brawler->isPlayer = player;
    brawler->maxHealth = ContentCharacter(content, character)->maxHealth;
    brawler->health = brawler->maxHealth;
    brawler->ammo = (float)ContentCharacter(content, character)->maxAmmo;
    brawler->alive = true;
    brawler->visible = true;
    brawler->spawnScale = 1.0f;
    brawler->dashAbility = -1;
    brawler->aiTarget = -1;
}

static void SetupDuel(GameSession *session, const ContentCatalog *content)
{
    memset(session, 0, sizeof(*session));
    SetupOpenArena(&session->arena);
    GameRandomSeed(&session->random, 0x51a7c0deu);
    session->brawlerCount = 2;
    session->playerIdx = 0;
    SetupBrawler(content, &session->brawlers[0], TEAM_PLAYER, CLASS_BRUISER,
                 (Vector3){ 0.0f, 0.0f, 0.0f }, true);
    SetupBrawler(content, &session->brawlers[1], TEAM_ENEMY, CLASS_SHOTGUNNER,
                 (Vector3){ 0.0f, 0.0f, 2.2f }, false);
    session->brawlers[0].aimAngle = 0.0f;
}

static void AdvanceProjectiles(GameContext game, float seconds)
{
    int frames = (int)ceilf(seconds*120.0f);
    for (int i = 0; i < frames; i++)
        ProjectilesUpdate(game, 1.0f/120.0f);
}

static void AdvanceBrawlers(GameContext game, float seconds)
{
    int frames = (int)ceilf(seconds*100.0f);
    for (int i = 0; i < frames; i++)
        BrawlersUpdate(game, 0.01f);
}

int main(void)
{
    Tuning tuning;
    ContentCatalog content = { 0 };
    GameSession session;
    TuningSetDefaults(&tuning);
    ContentCatalogResetAll(&content);

    // A single pellet makes each sustain assertion independent of spread/randomness.
    content.weapons[CLASS_BRUISER].pellets = 1;
    content.weapons[CLASS_BRUISER].spreadDeg = 0.0f;
    ContentCatalogRebuildTyped(&content);

    const AbilityDefinition *main =
        ContentMainAbility(&content, CLASS_BRUISER);
    const AbilityDefinition *mobility =
        ContentMobilityAbility(&content, CLASS_BRUISER);
    CHECK(main && fabsf(main->selfHealRatio - 0.20f) < 0.0001f,
          "Tank main attack is missing its 20% self-heal");
    CHECK(mobility && mobility->behavior == ABILITY_BEHAVIOR_DASH &&
          fabsf(mobility->cooldown - 2.50f) < 0.0001f &&
          fabsf(mobility->data.dash.duration - 0.18f) < 0.0001f &&
          fabsf(mobility->data.dash.speed - 22.0f) < 0.0001f &&
          mobility->damage == 0 && !mobility->data.dash.breaksCrates,
          "Shoulder Jets typed content is incomplete");
    CHECK(ContentMobilityAbility(&content, CLASS_SNIPER) == NULL,
          "mobility was assigned to a kit without configured values");

    SetupDuel(&session, &content);
    GameContext game = { &session, &tuning, &content };
    Brawler *tank = &session.brawlers[0];
    Brawler *enemy = &session.brawlers[1];

    tank->health = 4000;
    WeaponsFire(game, 0, false, 2.2f);
    CHECK(session.projectiles[0].active &&
          fabsf(session.projectiles[0].selfHealRatio - 0.20f) < 0.0001f,
          "projectile did not snapshot its self-heal ratio");
    content.weapons[CLASS_BRUISER].selfHealRatio = 1.0f;
    ContentCatalogRebuildTyped(&content);
    AdvanceProjectiles(game, 0.25f);
    CHECK(enemy->health == enemy->maxHealth - 440,
          "Tank main projectile did not deal its configured damage");
    CHECK(tank->health == 4088,
          "in-flight projectile changed after live content rebuild");

    content.weapons[CLASS_BRUISER].selfHealRatio = 0.20f;
    ContentCatalogRebuildTyped(&content);

    // Healing is based on health actually removed, not nominal/overkill damage.
    memset(session.projectiles, 0, sizeof(session.projectiles));
    enemy->alive = true;
    enemy->health = 10;
    tank->health = 4000;
    WeaponsFire(game, 0, false, 2.2f);
    AdvanceProjectiles(game, 0.25f);
    CHECK(!enemy->alive && tank->health == 4002,
          "overkill damage produced excess self-healing");

    // Existing healing rules cap at max health and never overheal.
    memset(session.projectiles, 0, sizeof(session.projectiles));
    enemy->alive = true;
    enemy->health = enemy->maxHealth;
    tank->health = tank->maxHealth - 20;
    WeaponsFire(game, 0, false, 2.2f);
    AdvanceProjectiles(game, 0.25f);
    CHECK(tank->health == tank->maxHealth,
          "reclaiming rounds overhealed the Tank");

    // Shoulder Jets travel four world units in open space, cannot be chained during
    // cooldown, and do not damage actors.
    SetupDuel(&session, &content);
    game.session = &session;
    tank = &session.brawlers[0];
    session.brawlerCount = 1;
    CHECK(BrawlerTryMobility(game, 0, (Vector3){ 0.0f, 0.0f, 1.0f }),
          "ready Shoulder Jets did not activate");
    CHECK(!BrawlerTryMobility(game, 0, (Vector3){ 0.0f, 0.0f, 1.0f }),
          "Shoulder Jets activated while already in use");
    AdvanceBrawlers(game, 0.30f);
    CHECK(fabsf(tank->position.z - 3.96f) < 0.03f,
          "Shoulder Jets traveled the wrong open-space distance");
    CHECK(!BrawlerTryMobility(game, 0, (Vector3){ 0.0f, 0.0f, 1.0f }),
          "Shoulder Jets ignored their cooldown");
    AdvanceBrawlers(game, 2.21f);
    CHECK(BrawlerTryMobility(game, 0, (Vector3){ 0.0f, 0.0f, -1.0f }),
          "Shoulder Jets did not become ready after the configured cooldown");

    SetupDuel(&session, &content);
    game.session = &session;
    tank = &session.brawlers[0];
    enemy = &session.brawlers[1];
    enemy->position = (Vector3){ 0.0f, 0.0f, 2.0f };
    int enemyBefore = enemy->health;
    CHECK(BrawlerTryMobility(game, 0, (Vector3){ 0.0f, 0.0f, 1.0f }),
          "contact boost did not activate");
    AdvanceBrawlers(game, 0.20f);
    CHECK(enemy->health == enemyBefore,
          "Shoulder Jets damaged an enemy");

    // A boost stops on intact cover and leaves it untouched.
    SetupDuel(&session, &content);
    game.session = &session;
    tank = &session.brawlers[0];
    int crateX = ArenaTileX(&session.arena, 0.0f);
    int crateZ = ArenaTileZ(&session.arena, 2.0f);
    session.arena.tiles[crateZ][crateX] =
        (Tile){ .type = TILE_CRATE, .health = tuning.crateHealth };
    CHECK(BrawlerTryMobility(game, 0, (Vector3){ 0.0f, 0.0f, 1.0f }),
          "cover-stop boost did not activate");
    AdvanceBrawlers(game, 0.30f);
    CHECK(tank->position.z <= 0.36f && tank->dashTimer <= 0.0f,
          "Shoulder Jets did not stop at the crate");
    CHECK(session.arena.tiles[crateZ][crateX].type == TILE_CRATE &&
          session.arena.tiles[crateZ][crateX].health == tuning.crateHealth,
          "Shoulder Jets damaged or destroyed a crate");

    // Charge remains the heavy dash: it breaks the same crate and damages targets,
    // but its damage never feeds the main-attack self-heal.
    tank->superCharge = 1.0f;
    tank->health = 4000;
    tank->aimAngle = 0.0f;
    CHECK(BrawlerTrySuper(game, 0, 0.0f), "Tank Charge did not activate");
    AdvanceBrawlers(game, 0.50f);
    CHECK(session.arena.tiles[crateZ][crateX].type == TILE_FLOOR,
          "Charge no longer destroys crates");
    CHECK(tank->health == 4000,
          "Charge incorrectly triggered main-attack self-healing");

    SetupDuel(&session, &content);
    game.session = &session;
    tank = &session.brawlers[0];
    enemy = &session.brawlers[1];
    tank->health = 4000;
    tank->superCharge = 1.0f;
    tank->aimAngle = 0.0f;
    int chargeTargetBefore = enemy->health;
    CHECK(BrawlerTrySuper(game, 0, 0.0f), "contact Charge did not activate");
    AdvanceBrawlers(game, 0.20f);
    CHECK(enemy->health == chargeTargetBefore - 1200,
          "Charge contact damage changed during mobility refactor");
    CHECK(tank->health == 4000,
          "Charge contact damage produced self-healing");

    puts("Tank reclaiming rounds, Shoulder Jets, cover, cooldown, and Charge passed");
    return 0;
}
