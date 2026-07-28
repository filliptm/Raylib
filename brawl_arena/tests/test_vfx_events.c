#include "brawler.h"
#include "content_catalog.h"
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
        .aiTarget = -1
    };
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

static bool HasAction(const GameSession *session, CharacterActionId action)
{
    for (int i = 0; i < session->events.count; i++)
        if (session->events.items[i].type == GAME_EVENT_CHARACTER_ACTION &&
            session->events.items[i].characterAction == action &&
            session->events.items[i].sourceBrawler == 0)
            return true;
    return false;
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

    printf("VFX event tests passed: all kits, reclaim, jets, and rain are mapped\n");
    return 0;
}
