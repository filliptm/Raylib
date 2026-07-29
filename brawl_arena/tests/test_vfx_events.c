#include "brawler.h"
#include "content_catalog.h"
#include "game_events.h"
#include "game_random.h"
#include "weapons.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static void SetupOpenArena(Arena *arena)
{
    memset(arena, 0, sizeof(*arena));
    arena->width = 17;
    arena->height = 17;
    arena->tileSize = 2.0f;
    for (int z = 0; z < arena->height; z++)
        for (int x = 0; x < arena->width; x++)
            arena->tiles[z][x].type =
                (x == 0 || z == 0 ||
                 x == arena->width - 1 || z == arena->height - 1)
                    ? TILE_WALL : TILE_FLOOR;
}

static void SetupBrawler(const ContentCatalog *content, Brawler *brawler,
                         Team team, BrawlerClass cls, Vector3 position)
{
    *brawler = (Brawler){
        .position = position,
        .team = team,
        .cls = cls,
        .health = ContentCharacter(content, cls)->maxHealth,
        .maxHealth = ContentCharacter(content, cls)->maxHealth,
        .ammo = 3.0f,
        .alive = true,
        .visible = true,
        .spawnScale = 1.0f,
        .dashAbility = -1,
        .grappleAbility = -1,
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
                      BrawlerClass cls)
{
    memset(session, 0, sizeof(*session));
    SetupOpenArena(&session->arena);
    GameRandomSeed(&session->random, 0x7f18a52du);
    session->brawlerCount = 2;
    session->playerIdx = 0;
    SetupBrawler(content, &session->brawlers[0], TEAM_PLAYER, cls,
                 (Vector3){ 0.0f, 0.0f, 0.0f });
    SetupBrawler(content, &session->brawlers[1], TEAM_ENEMY, CLASS_SHOTGUNNER,
                 (Vector3){ 0.0f, 0.0f, 5.0f });
    session->brawlers[0].isPlayer = true;
    session->brawlers[0].aimAngle = 0.0f;
}

static bool HasVfx(const GameSession *session, VfxEffectId id)
{
    for (int i = 0; i < session->events.count; i++)
        if (session->events.items[i].type == GAME_EVENT_VFX &&
            session->events.items[i].vfxId == id)
            return true;
    return false;
}

static const GameEvent *FindVfx(const GameSession *session, VfxEffectId id)
{
    for (int i = 0; i < session->events.count; i++)
        if (session->events.items[i].type == GAME_EVENT_VFX &&
            session->events.items[i].vfxId == id)
            return &session->events.items[i];
    return NULL;
}

static int CountFloatText(const GameSession *session)
{
    int count = 0;
    for (int i = 0; i < session->events.count; i++)
        if (session->events.items[i].type == GAME_EVENT_FLOAT_TEXT)
            count++;
    return count;
}

static bool HasFloatText(const GameSession *session, const char *text,
                         int sourceBrawler, int targetBrawler)
{
    for (int i = 0; i < session->events.count; i++)
    {
        const GameEvent *event = &session->events.items[i];
        if (event->type == GAME_EVENT_FLOAT_TEXT &&
            event->sourceBrawler == sourceBrawler &&
            event->targetBrawler == targetBrawler &&
            strcmp(event->text, text) == 0)
            return true;
    }
    return false;
}

static bool HasAction(const GameSession *session, CharacterActionId action)
{
    for (int i = 0; i < session->events.count; i++)
        if (session->events.items[i].type == GAME_EVENT_CHARACTER_ACTION &&
            session->events.items[i].characterAction == action &&
            session->events.items[i].sourceBrawler == 0)
            return true;
    return false;
}

static void SpawnIncomingProjectile(GameSession *session)
{
    session->projectiles[0] = (Projectile){
        .position = { 0.0f, 0.75f, 3.0f },
        .origin = { 0.0f, 0.75f, 3.0f },
        .velocity = { 0.0f, 0.0f, -20.0f },
        .range = 20.0f,
        .damage = 300,
        .radius = 0.18f,
        .team = TEAM_ENEMY,
        .owner = 1,
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

    const VfxEffectId mainCast[CLASS_COUNT] = {
        VFX_SCRAPPER_CAST,
        VFX_LONGSHOT_CAST,
        VFX_MORTAR_CAST,
        VFX_TANK_CAST,
        VFX_GUARDIAN_RAIN_CAST
    };
    const VfxEffectId superCast[CLASS_COUNT] = {
        VFX_SCRAPPER_SUPER_CAST,
        VFX_LONGSHOT_SUPER_CAST,
        VFX_MORTAR_SUPER_CAST,
        VFX_TANK_CHARGE_START,
        VFX_GUARDIAN_RESONANCE_CAST
    };

    for (int cls = 0; cls < CLASS_COUNT; cls++)
    {
        SetupDuel(&session, &content, (BrawlerClass)cls);
        GameContext game = { &session, &tuning, &content };
        WeaponsFire(game, 0, false, 5.0f);
        CHECK(HasVfx(&session, mainCast[cls]),
              "a main attack did not emit its stable cast VFX ID");
        CHECK(HasAction(&session, cls == CLASS_HEALER
                                     ? CHARACTER_ACTION_CAST
                                     : CHARACTER_ACTION_MAIN),
              "a main attack did not emit its character one-shot");
        const GameEvent *mainEvent = FindVfx(&session, mainCast[cls]);
        CHECK(mainEvent && mainEvent->sourceBrawler == 0 &&
              mainEvent->startSocket != VFX_SOCKET_NONE,
              "a main cast was not attached to its source rig");

        session.events.count = 0;
        memset(session.projectiles, 0, sizeof(session.projectiles));
        memset(session.abilityFields, 0, sizeof(session.abilityFields));
        session.brawlers[0].dashTimer = 0.0f;
        session.brawlers[0].dashAbility = -1;
        WeaponsFire(game, 0, true, 5.0f);
        CHECK(HasVfx(&session, superCast[cls]),
              "a super did not emit its stable cast VFX ID");
        CHECK(HasAction(&session, cls == CLASS_BRUISER
                                     ? CHARACTER_ACTION_MOBILITY
                                     : CHARACTER_ACTION_SUPER),
              "a super did not emit its character one-shot");
    }

    // Reclamation Rounds emit a travel path from the actual impact to Tank only
    // when damage produced real healing.
    content.weapons[CLASS_BRUISER].pellets = 1;
    content.weapons[CLASS_BRUISER].spreadDeg = 0.0f;
    ContentCatalogRebuildTyped(&content);
    SetupDuel(&session, &content, CLASS_BRUISER);
    session.brawlers[1].position.z = 2.2f;
    session.brawlers[0].health -= 500;
    GameContext game = { &session, &tuning, &content };
    WeaponsFire(game, 0, false, 2.2f);
    for (int frame = 0; frame < 60; frame++)
        ProjectilesUpdate(game, 1.0f/120.0f);
    CHECK(HasVfx(&session, VFX_TANK_RECLAIM),
          "Tank damage healing did not emit the reclaim path");
    const GameEvent *reclaim = FindVfx(&session, VFX_TANK_RECLAIM);
    CHECK(reclaim && reclaim->targetBrawler == 0 &&
          reclaim->endSocket == VFX_SOCKET_CHEST,
          "Tank reclaim did not terminate on the current chest socket");
    CHECK(HasVfx(&session, VFX_TANK_IMPACT),
          "Tank projectile hit did not emit its impact recipe");

    // Shoulder Jets and the damaging Charge use different start/trail identities.
    SetupDuel(&session, &content, CLASS_BRUISER);
    game.session = &session;
    session.brawlerCount = 1;
    CHECK(BrawlerTryMobility(game, 0, (Vector3){ 0, 0, 1 }),
          "Shoulder Jets did not activate in the VFX test");
    CHECK(HasVfx(&session, VFX_TANK_JETS_START),
          "Shoulder Jets did not emit its non-damaging start");
    const GameEvent *jets = FindVfx(&session, VFX_TANK_JETS_START);
    CHECK(jets && jets->sourceBrawler == 0 &&
          (jets->startSocket == VFX_SOCKET_LEFT_SHOULDER ||
           jets->startSocket == VFX_SOCKET_RIGHT_SHOULDER),
          "Shoulder Jets did not attach to a shoulder socket");
    CHECK(HasAction(&session, CHARACTER_ACTION_MOBILITY),
          "Shoulder Jets did not emit its mobility pose");
    session.events.count = 0;
    BrawlersUpdate(game, 0.06f);
    CHECK(HasVfx(&session, VFX_TANK_JETS_TRAIL),
          "Shoulder Jets did not emit its throttled trail");

    // Scrapper's saw legs and held shell have distinct event identities so the
    // renderer does not have to infer ability phases from projectile state.
    SetupDuel(&session, &content, CLASS_SHOTGUNNER);
    game.session = &session;
    WeaponsFire(game, 0, false, 5.0f);
    for (int frame = 0; frame < 180; frame++)
        ProjectilesUpdate(game, 1.0f/120.0f);
    CHECK(HasVfx(&session, VFX_SCRAPPER_RETURN),
          "Ripsaw did not emit its outbound-to-return transition");
    CHECK(HasVfx(&session, VFX_SCRAPPER_CATCH),
          "Ripsaw did not emit its owner catch");

    SetupDuel(&session, &content, CLASS_SHOTGUNNER);
    game.session = &session;
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0, 0, 1 }),
          "Magnetic Scrap Shell did not activate in the VFX test");
    CHECK(HasVfx(&session, VFX_SCRAPPER_SHIELD_START),
          "Magnetic Scrap Shell did not emit its start recipe");
    CHECK(HasAction(&session, CHARACTER_ACTION_GUARD),
          "Magnetic Scrap Shell did not emit its braced pose");
    session.events.count = 0;
    SpawnIncomingProjectile(&session);
    for (int frame = 0; frame < 30; frame++)
        ProjectilesUpdate(game, 1.0f/120.0f);
    CHECK(HasVfx(&session, VFX_SCRAPPER_SHIELD_HIT),
          "Magnetic Scrap Shell did not emit hit feedback");
    session.events.count = 0;
    session.brawlers[0].shieldCharge = 100.0f;
    BrawlerApplyDamage(game, 0, 200, 1, session.brawlers[0].position);
    CHECK(HasVfx(&session, VFX_SCRAPPER_SHIELD_BREAK),
          "Magnetic Scrap Shell did not emit collapse feedback");
    session.events.count = 0;
    BrawlersUpdate(game, 5.10f);
    CHECK(HasVfx(&session, VFX_SCRAPPER_SHIELD_RESTORE),
          "Magnetic Scrap Shell did not emit restore feedback");

    // Grapple and mine use phase-specific recipes and bespoke action identities.
    SetupDuel(&session, &content, CLASS_SNIPER);
    game.session = &session;
    session.brawlerCount = 1;
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0, 0, 1 }),
          "Mag-Line Grapple did not activate in the VFX test");
    CHECK(HasVfx(&session, VFX_LONGSHOT_GRAPPLE_FIRE) &&
          HasAction(&session, CHARACTER_ACTION_GRAPPLE),
          "Mag-Line Grapple did not emit its launch recipe/action");
    BrawlersUpdate(game, 0.26f);
    CHECK(HasVfx(&session, VFX_LONGSHOT_GRAPPLE_HOOK) &&
          HasVfx(&session, VFX_LONGSHOT_GRAPPLE_PULL),
          "Mag-Line Grapple did not emit its hook/pull phases");
    BrawlersUpdate(game, 0.50f);
    CHECK(HasVfx(&session, VFX_LONGSHOT_GRAPPLE_LAND),
          "Mag-Line Grapple did not emit its landing phase");

    SetupDuel(&session, &content, CLASS_LOBBER);
    game.session = &session;
    CHECK(BrawlerTrySecondary(game, 0, (Vector3){ 0 }),
          "Concussion Mine did not deploy in the VFX test");
    CHECK(HasVfx(&session, VFX_MORTAR_MINE_PLACE) &&
          HasAction(&session, CHARACTER_ACTION_MINE_DEPLOY),
          "Concussion Mine did not emit its placement recipe/action");
    ProjectilesUpdate(game, 0.56f);
    CHECK(HasVfx(&session, VFX_MORTAR_MINE_ARM),
          "Concussion Mine did not emit its armed phase");
    session.brawlers[1].position = (Vector3){ 0.0f, 0.0f, 1.5f };
    ProjectilesUpdate(game, 0.02f);
    CHECK(HasVfx(&session, VFX_MORTAR_MINE_DETONATE),
          "Concussion Mine did not emit its detonation recipe");

    // A rain field emits the authoritative field pulse plus target feedback.
    SetupDuel(&session, &content, CLASS_HEALER);
    game.session = &session;
    WeaponsFire(game, 0, false, 5.0f);
    session.events.count = 0;
    ProjectilesUpdate(game, 0.02f);
    CHECK(HasVfx(&session, VFX_GUARDIAN_RAIN_PULSE),
          "Guardian rain tick did not emit its pulse recipe");
    CHECK(HasVfx(&session, VFX_GUARDIAN_RAIN_DAMAGE),
          "Guardian rain did not emit target damage feedback");

    // Combat numbers only enter the presentation queue when the local player is
    // their source or target. Generic labels remain available to non-combat systems.
    SetupDuel(&session, &content, CLASS_SHOTGUNNER);
    session.brawlerCount = 4;
    SetupBrawler(&content, &session.brawlers[1], TEAM_PLAYER, CLASS_HEALER,
                 (Vector3){ -2.0f, 0.0f, 1.0f });
    SetupBrawler(&content, &session.brawlers[2], TEAM_ENEMY, CLASS_BRUISER,
                 (Vector3){ 2.0f, 0.0f, 1.0f });
    SetupBrawler(&content, &session.brawlers[3], TEAM_ENEMY, CLASS_HEALER,
                 (Vector3){ 3.0f, 0.0f, 1.0f });
    game.session = &session;

    GameEventsClear(&session);
    BrawlerApplyDamage(game, 2, 100, 1, session.brawlers[2].position);
    CHECK(CountFloatText(&session) == 0,
          "ally-versus-enemy damage leaked unrelated combat text");

    session.brawlers[3].health -= 200;
    GameEventsClear(&session);
    BrawlerApplyHealing(game, 3, 100, 2, session.brawlers[3].position);
    CHECK(CountFloatText(&session) == 0,
          "bot-only healing leaked unrelated combat text");

    GameEventsClear(&session);
    BrawlerApplyDamage(game, 2, 123, 0, session.brawlers[2].position);
    CHECK(CountFloatText(&session) == 1 &&
          HasFloatText(&session, "123", 0, 2),
          "player damage did not retain its outgoing combat number");

    GameEventsClear(&session);
    BrawlerApplyDamage(game, 0, 124, 2, session.brawlers[0].position);
    CHECK(CountFloatText(&session) == 1 &&
          HasFloatText(&session, "124", 2, 0),
          "incoming player damage did not retain its combat number");

    GameEventsClear(&session);
    BrawlerApplyDamage(game, 0, 125, -1, session.brawlers[0].position);
    CHECK(CountFloatText(&session) == 1 &&
          HasFloatText(&session, "125", -1, 0),
          "environmental player damage did not retain its combat number");

    session.brawlers[1].health -= 200;
    GameEventsClear(&session);
    BrawlerApplyHealing(game, 1, 126, 0, session.brawlers[1].position);
    CHECK(CountFloatText(&session) == 1 &&
          HasFloatText(&session, "+126", 0, 1),
          "player-provided healing did not retain its combat number");

    session.brawlers[0].health -= 200;
    GameEventsClear(&session);
    BrawlerApplyHealing(game, 0, 127, 1, session.brawlers[0].position);
    CHECK(CountFloatText(&session) == 1 &&
          HasFloatText(&session, "+127", 1, 0),
          "healing received by the player did not retain its combat number");

    SetupBrawler(&content, &session.brawlers[1], TEAM_PLAYER,
                 CLASS_SHOTGUNNER, (Vector3){ -2.0f, 0.0f, 1.0f });
    session.brawlers[1].shieldActive = true;
    session.brawlers[1].health -= 200;
    GameEventsClear(&session);
    BrawlerApplyDamage(game, 1, 100, 2, session.brawlers[1].position);
    CHECK(CountFloatText(&session) == 0,
          "an AI-only shield exchange leaked absorb or healing text");

    SetupBrawler(&content, &session.brawlers[0], TEAM_PLAYER,
                 CLASS_SHOTGUNNER, (Vector3){ 0.0f, 0.0f, 0.0f });
    session.brawlers[0].isPlayer = true;
    session.brawlers[0].shieldActive = true;
    session.brawlers[0].health -= 200;
    GameEventsClear(&session);
    BrawlerApplyDamage(game, 0, 100, 2, session.brawlers[0].position);
    CHECK(CountFloatText(&session) == 2 &&
          HasFloatText(&session, "SH -100", 2, 0) &&
          HasFloatText(&session, "+30", 0, 0),
          "the player's shield did not retain absorb and self-heal text");

    SetupBrawler(&content, &session.brawlers[2], TEAM_ENEMY,
                 CLASS_SHOTGUNNER, (Vector3){ 2.0f, 0.0f, 1.0f });
    session.brawlers[2].shieldActive = true;
    session.brawlers[2].health -= 200;
    GameEventsClear(&session);
    BrawlerApplyDamage(game, 2, 100, 0, session.brawlers[2].position);
    CHECK(CountFloatText(&session) == 1 &&
          HasFloatText(&session, "SH -100", 0, 2),
          "the player's shield damage lost its absorb number or exposed AI healing");

    GameEventsClear(&session);
    GameEmitFloatText(&session, session.brawlers[0].position, "READY", WHITE);
    CHECK(CountFloatText(&session) == 1 &&
          HasFloatText(&session, "READY", -1, -1),
          "combat filtering changed generic floating labels");

    printf("VFX event tests passed: kit effects and player-relevant combat text are mapped\n");
    return 0;
}
