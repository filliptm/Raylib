#include "ai.h"
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
    arena->width = 21;
    arena->height = 21;
    arena->tileSize = 2.0f;
    for (int z = 0; z < arena->height; z++)
        for (int x = 0; x < arena->width; x++)
            arena->tiles[z][x].type =
                (x == 0 || z == 0 ||
                 x == arena->width - 1 || z == arena->height - 1)
                ? TILE_WALL : TILE_FLOOR;
}

static void SetupBrawler(const ContentCatalog *content, Brawler *brawler,
                         Team team, BrawlerClass cls, Vector3 position,
                         bool player)
{
    *brawler = (Brawler){
        .position = position,
        .team = team,
        .cls = cls,
        .isPlayer = player,
        .health = ContentCharacter(content, cls)->maxHealth,
        .maxHealth = ContentCharacter(content, cls)->maxHealth,
        .ammo = (float)ContentCharacter(content, cls)->maxAmmo,
        .alive = true,
        .visible = true,
        .spawnScale = 1.0f,
        .dashAbility = -1,
        .shieldAbility = -1,
        .aiTarget = -1
    };
    const AbilityDefinition *secondary =
        ContentSecondaryAbility(content, cls);
    if (secondary && secondary->behavior == ABILITY_BEHAVIOR_SHIELD)
    {
        brawler->shieldAbility =
            ContentCharacter(content, cls)->mobilityAbility;
        brawler->shieldCharge = (float)secondary->data.shield.capacity;
    }
}

static void SetupDuel(GameSession *session, const ContentCatalog *content,
                      BrawlerClass enemyClass, Vector3 enemyPosition)
{
    memset(session, 0, sizeof(*session));
    SetupOpenArena(&session->arena);
    GameRandomSeed(&session->random, 0x51a9c4d2u);
    session->brawlerCount = 2;
    session->playerIdx = 0;
    SetupBrawler(content, &session->brawlers[0], TEAM_PLAYER,
                 CLASS_SHOTGUNNER, (Vector3){ 0, 0, 0 }, true);
    SetupBrawler(content, &session->brawlers[1], TEAM_ENEMY,
                 enemyClass, enemyPosition, false);
    session->brawlers[0].aimAngle = 0.0f;
    session->brawlers[1].aimAngle = PI;
}

static void AdvanceProjectiles(GameContext game, float seconds)
{
    int frames = (int)ceilf(seconds*240.0f);
    for (int frame = 0; frame < frames; frame++)
        ProjectilesUpdate(game, 1.0f/240.0f);
}

static Projectile *FirstProjectile(GameSession *session)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
        if (session->projectiles[i].active) return &session->projectiles[i];
    return NULL;
}

static bool HasVfx(const GameSession *session, VfxEffectId id)
{
    for (int i = 0; i < session->events.count; i++)
        if (session->events.items[i].type == GAME_EVENT_VFX &&
            session->events.items[i].vfxId == id)
            return true;
    return false;
}

static void SpawnIncoming(GameSession *session, int damage,
                          Vector3 position, Vector3 velocity)
{
    Projectile *projectile = &session->projectiles[0];
    *projectile = (Projectile){
        .position = position,
        .origin = position,
        .velocity = velocity,
        .range = 30.0f,
        .damage = damage,
        .selfHealRatio = 0.20f,
        .radius = 0.18f,
        .team = TEAM_ENEMY,
        .owner = 1,
        .piercing = true,
        .active = true
    };
}

int main(void)
{
    Tuning tuning;
    ContentCatalog content = { 0 };
    GameSession session;
    TuningSetDefaults(&tuning);
    ContentCatalogResetAll(&content);

    const AbilityDefinition *main =
        ContentMainAbility(&content, CLASS_SHOTGUNNER);
    const AbilityDefinition *super =
        ContentSuperAbility(&content, CLASS_SHOTGUNNER);
    const AbilityDefinition *shield =
        ContentSecondaryAbility(&content, CLASS_SHOTGUNNER);
    CHECK(main && main->behavior == ABILITY_BEHAVIOR_RETURNING &&
          main->damage == 700 && main->range == 13.0f &&
          main->radius == 0.42f &&
          main->data.returning.outboundSpeed == 26.0f &&
          main->data.returning.returnSpeed == 32.0f &&
          main->superPerHit == 0.14f,
          "Ripsaw typed content changed");
    CHECK(super && super->behavior == ABILITY_BEHAVIOR_RETURNING &&
          super->damage == 1100 && super->range == 18.0f &&
          super->radius == 0.75f &&
          super->data.returning.outboundSpeed == 22.0f &&
          super->data.returning.returnSpeed == 30.0f &&
          super->data.returning.outboundPull == 1.25f &&
          super->data.returning.returnKnockback == 2.25f &&
          super->data.returning.breaksCrates,
          "Wrecking Disc typed content changed");
    CHECK(shield && shield->behavior == ABILITY_BEHAVIOR_SHIELD &&
          shield->data.shield.capacity == 1200 &&
          shield->data.shield.moveMultiplier == 0.65f &&
          shield->data.shield.healRatio == 0.30f &&
          shield->data.shield.rechargeDelay == 3.0f &&
          shield->data.shield.rechargeRate == 300.0f &&
          shield->data.shield.breakLockout == 5.0f,
          "Magnetic Scrap Shell typed content changed");

    SetupDuel(&session, &content, CLASS_SHOTGUNNER,
              (Vector3){ 0, 0, 6.0f });
    GameContext game = { &session, &tuning, &content };
    int enemyStart = session.brawlers[1].health;
    WeaponsFire(game, 0, false, 6.0f);
    Projectile *saw = FirstProjectile(&session);
    CHECK(saw && saw->motion == PROJECTILE_MOTION_RETURNING &&
          saw->outbound && saw->piercing,
          "Ripsaw did not spawn as a piercing outbound disc");
    AdvanceProjectiles(game, 1.25f);
    CHECK(session.brawlers[1].health == enemyStart - 1400,
          "Ripsaw did not damage the target once on each leg");
    CHECK(!session.projectiles[0].active,
          "Ripsaw did not catch at its moving owner");
    CHECK(fabsf(session.brawlers[0].superCharge - 0.28f) < 0.001f,
          "Ripsaw did not award super charge per damaging pass");

    // The return homes toward the owner's live position rather than the cast origin.
    SetupDuel(&session, &content, CLASS_SHOTGUNNER,
              (Vector3){ 8, 0, 8 });
    game.session = &session;
    WeaponsFire(game, 0, false, 8.0f);
    AdvanceProjectiles(game, 0.55f);
    session.brawlers[0].position.x = 4.0f;
    AdvanceProjectiles(game, 0.70f);
    CHECK(!session.projectiles[0].active &&
          HasVfx(&session, VFX_SCRAPPER_CATCH),
          "Ripsaw did not home to and catch at the owner's moved position");

    // Ordinary Ripsaw turns on cover without damaging it.
    SetupDuel(&session, &content, CLASS_SHOTGUNNER,
              (Vector3){ 6, 0, 6 });
    game.session = &session;
    int crateX = ArenaTileX(&session.arena, 0.0f);
    int crateZ = ArenaTileZ(&session.arena, 4.0f);
    session.arena.tiles[crateZ][crateX] =
        (Tile){ .type = TILE_CRATE, .health = tuning.crateHealth };
    WeaponsFire(game, 0, false, 8.0f);
    AdvanceProjectiles(game, 0.70f);
    CHECK(session.arena.tiles[crateZ][crateX].type == TILE_CRATE &&
          session.arena.tiles[crateZ][crateX].health == tuning.crateHealth,
          "Ripsaw damaged destructible cover");
    CHECK(!session.projectiles[0].active,
          "cover-turn Ripsaw did not return and catch");

    // Wrecking Disc punches through crates, pulls on the outbound hit, then knocks
    // along its return direction.
    SetupDuel(&session, &content, CLASS_SHOTGUNNER,
              (Vector3){ 0.90f, 0, 6.0f });
    game.session = &session;
    crateX = ArenaTileX(&session.arena, 0.0f);
    crateZ = ArenaTileZ(&session.arena, 4.0f);
    session.arena.tiles[crateZ][crateX] =
        (Tile){ .type = TILE_CRATE, .health = 3000 };
    enemyStart = session.brawlers[1].health;
    WeaponsFire(game, 0, true, 8.0f);
    bool outboundHit = false;
    float pulledX = session.brawlers[1].position.x;
    for (int frame = 0; frame < 240 && !outboundHit; frame++)
    {
        ProjectilesUpdate(game, 1.0f/240.0f);
        if (session.brawlers[1].health == enemyStart - 1100)
        {
            outboundHit = true;
            pulledX = session.brawlers[1].position.x;
        }
    }
    CHECK(outboundHit && fabsf(pulledX) < 0.90f,
          "Wrecking Disc outbound hit did not pull toward its line");
    CHECK(session.arena.tiles[crateZ][crateX].type == TILE_FLOOR,
          "Wrecking Disc did not break and pass through a crate");
    float zBeforeReturn = session.brawlers[1].position.z;
    AdvanceProjectiles(game, 1.20f);
    CHECK(session.brawlers[1].health == enemyStart - 2200,
          "Wrecking Disc did not hit once per leg");
    CHECK(session.brawlers[1].position.z < zBeforeReturn - 1.5f,
          "Wrecking Disc return did not knock along travel");

    // The shell catches hostile damage from every direction, heals from the amount
    // absorbed, and gives the attacker ordinary hit super without letting Tank's
    // health-damage lifesteal feed on the shield.
    SetupDuel(&session, &content, CLASS_BRUISER,
              (Vector3){ 0, 0, 5.0f });
    game.session = &session;
    Brawler *scrapper = &session.brawlers[0];
    Brawler *attacker = &session.brawlers[1];
    scrapper->health -= 600;
    attacker->health -= 500;
    int scrapperStart = scrapper->health;
    int attackerStart = attacker->health;
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0, 0, 1 }),
          "ready Scrap Shell did not activate");
    SpawnIncoming(&session, 700, (Vector3){ 0, 0.75f, -3.0f },
                  (Vector3){ 0, 0, 20.0f });
    AdvanceProjectiles(game, 0.20f);
    CHECK(scrapper->health == scrapperStart + 210 &&
          scrapper->shieldCharge == 500.0f &&
          attacker->health == attackerStart &&
          attacker->superCharge > 0.0f &&
          scrapper->shieldActive,
          "360-degree Scrap Shell absorption/healing rules changed");

    SpawnIncoming(&session, 700, (Vector3){ 0, 0.75f, 3.0f },
                  (Vector3){ 0, 0, -20.0f });
    AdvanceProjectiles(game, 0.20f);
    CHECK(scrapper->health == scrapperStart + 160,
          "Scrap Shell overflow did not heal from absorption then apply excess");
    CHECK(attacker->health == attackerStart + 40 &&
          attacker->superCharge > 0.0f,
          "overflow did not preserve attacker sustain/super rules");
    CHECK(!scrapper->shieldActive &&
          scrapper->shieldCharge == 0.0f &&
          scrapper->shieldRearmRequired &&
          fabsf(scrapper->shieldBrokenTimer - 5.0f) < 0.001f &&
          HasVfx(&session, VFX_SCRAPPER_SHIELD_BREAK),
          "capacity break did not enter the five-second shell lockout");

    // Partial charge is retained on release, waits three clean seconds, and then
    // recharges at 300 points per second.
    SetupDuel(&session, &content, CLASS_BRUISER,
              (Vector3){ 0, 0, 5.0f });
    game.session = &session;
    scrapper = &session.brawlers[0];
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0, 0, 1 }),
          "recharge test shell did not activate");
    BrawlerDamageResult partial = BrawlerApplyDamageDetailed(
        game, 0, 600, 1, scrapper->position);
    CHECK(partial.shieldAbsorbed == 600 && partial.healthRemoved == 0,
          "central damage result did not distinguish shield from health");
    BrawlerReleaseShield(game, 0);
    BrawlersUpdate(game, 2.90f);
    CHECK(scrapper->shieldCharge == 600.0f,
          "shell recharged before its damage-free delay");
    BrawlersUpdate(game, 0.11f);
    BrawlersUpdate(game, 1.00f);
    CHECK(fabsf(scrapper->shieldCharge - 900.0f) < 0.01f,
          "partial shell did not recharge at 300 points per second");
    session.events.count = 0;
    BrawlersUpdate(game, 1.00f);
    CHECK(scrapper->shieldCharge == 1200.0f &&
          HasVfx(&session, VFX_SCRAPPER_SHIELD_RESTORE),
          "partial shell did not cap and emit full-restore feedback");

    // A broken shell restores to full after five seconds, but a held input cannot
    // immediately raise it again until release clears the rearm latch.
    SetupDuel(&session, &content, CLASS_BRUISER,
              (Vector3){ 0, 0, 2.5f });
    game.session = &session;
    scrapper = &session.brawlers[0];
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0, 0, 1 }),
          "break-lockout shell did not activate");
    BrawlerApplyDamageDetailed(game, 0, 1200, 1, scrapper->position);
    BrawlersUpdate(game, 5.10f);
    CHECK(scrapper->shieldCharge == 1200.0f &&
          scrapper->shieldBrokenTimer == 0.0f &&
          scrapper->shieldRearmRequired &&
          !BrawlerTrySecondary(game, 0, (Vector3){ 0, 0, 1 }),
          "broken shell ignored full restore or release-to-rearm");
    BrawlerReleaseShield(game, 0);
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0, 0, 1 }),
          "released full shell did not rearm");
    BrawlersUpdate(game, 10.0f);
    CHECK(scrapper->shieldActive,
          "held shell timed out instead of remaining active");

    // Fight bots react to a threat from either direction and lower the shell once the
    // projectile is gone so the resource can recharge.
    SetupDuel(&session, &content, CLASS_BRUISER,
              (Vector3){ 0, 0, 5.0f });
    session.playerIdx = 1;
    session.brawlers[0].isPlayer = false;
    session.brawlers[1].isPlayer = true;
    game.session = &session;
    SpawnIncoming(&session, 300, (Vector3){ 0, 0.75f, -3.0f },
                  (Vector3){ 0, 0, 10.0f });
    CHECK(BrawlerProjectileThreat(game, 0, 0.40f),
          "360-degree collision prediction missed an imminent projectile");
    AIUpdate(game, 1.0f/60.0f);
    CHECK(session.brawlers[0].shieldActive,
          "Fight bot did not react to a predicted projectile");
    session.projectiles[0].active = false;
    AIUpdate(game, 1.0f/60.0f);
    CHECK(!session.brawlers[0].shieldActive,
          "Fight bot did not lower its shell after the threat passed");

    // Returning shots are owned by the casting life/class and cannot survive a swap.
    SetupDuel(&session, &content, CLASS_BRUISER,
              (Vector3){ 0, 0, 8.0f });
    game.session = &session;
    WeaponsFire(game, 0, false, 8.0f);
    session.brawlers[0].cls = CLASS_SNIPER;
    ProjectilesUpdate(game, 1.0f/60.0f);
    CHECK(!session.projectiles[0].active,
          "Ripsaw survived an owner class swap");

    puts("Scrapper Ripsaw, Wrecking Disc, renewable Shell, AI, cover, and ownership passed");
    return 0;
}
