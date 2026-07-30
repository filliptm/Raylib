#include "brawler.h"
#include "content_catalog.h"
#include "game_events.h"
#include "game_random.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static void Setup(Tuning *tuning, ContentCatalog *content, GameSession *session)
{
    TuningSetDefaults(tuning);
    ContentCatalogResetAll(content);
    memset(session, 0, sizeof(*session));
    GameRandomSeed(&session->random, 0x7e6e4a11u);
    session->playerIdx = 0;
    session->brawlerCount = 2;

    GameContext game = {
        .session = session,
        .tuning = tuning,
        .content = content
    };
    BrawlerSpawn(game, 0, TEAM_PLAYER, CLASS_BRUISER,
                 (Vector3){ 0.0f, 0.0f, 0.0f }, true);
    BrawlerSpawn(game, 1, TEAM_ENEMY, CLASS_SHOTGUNNER,
                 (Vector3){ 0.0f, 0.0f, 5.0f }, false);
    GameEventsClear(session);
}

static int RegenAmount(const Brawler *brawler, float ratio)
{
    int amount = (int)floorf(brawler->maxHealth*ratio + 0.5f);
    return amount < 1 ? 1 : amount;
}

int main(void)
{
    Tuning tuning;
    ContentCatalog content;
    GameSession session;
    Setup(&tuning, &content, &session);
    GameContext game = { &session, &tuning, &content };
    Brawler *player = &session.brawlers[0];
    Brawler *enemy = &session.brawlers[1];

    CHECK(tuning.healthRegenDelay == 3.0f &&
          tuning.healthRegenInterval == 1.0f &&
          tuning.healthRegenRatio == 0.13f,
          "compiled recovery defaults changed");

    int playerPulse = RegenAmount(player, tuning.healthRegenRatio);
    int enemyPulse = RegenAmount(enemy, tuning.healthRegenRatio);
    BrawlerApplyDamage(game, 0, 2500, 1, player->position);
    BrawlerApplyDamage(game, 1, 1800, 0, enemy->position);
    int playerHurt = player->health;
    int enemyHurt = enemy->health;

    session.time = 2.999f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == playerHurt && enemy->health == enemyHurt,
          "regeneration started before the three-second delay");

    session.time = 3.0f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == playerHurt + playerPulse &&
          enemy->health == enemyHurt + enemyPulse,
          "the first regeneration pulse did not heal every living brawler");

    session.time = 3.999f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == playerHurt + playerPulse,
          "a second regeneration pulse arrived early");

    session.time = 4.0f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == playerHurt + playerPulse*2,
          "one-second regeneration cadence changed");

    // Actual damage restarts the delay, including damage applied after regeneration.
    session.time = 4.25f;
    BrawlerApplyDamage(game, 0, 100, 1, player->position);
    int afterResetDamage = player->health;
    session.time = 7.249f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == afterResetDamage,
          "taking damage did not restart the regeneration delay");
    session.time = 7.25f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == afterResetDamage + playerPulse,
          "regeneration did not resume three seconds after damage");

    // A successful main attack interrupts recovery at cast time.
    Setup(&tuning, &content, &session);
    game = (GameContext){ &session, &tuning, &content };
    player = &session.brawlers[0];
    BrawlerApplyDamage(game, 0, 2500, 1, player->position);
    session.time = 2.9f;
    CHECK(BrawlerTryAttack(game, 0, 5.0f), "test main attack did not fire");
    int afterAttack = player->health;
    session.time = 5.899f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == afterAttack,
          "successful main attack did not restart regeneration");
    session.time = 5.9f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == afterAttack + RegenAmount(player, tuning.healthRegenRatio),
          "regeneration did not resume after a successful main attack");

    // Failed attacks are not combat actions and therefore leave the timer alone.
    Setup(&tuning, &content, &session);
    game = (GameContext){ &session, &tuning, &content };
    player = &session.brawlers[0];
    BrawlerApplyDamage(game, 0, 2500, 1, player->position);
    player->ammo = 0.0f;
    session.time = 2.9f;
    CHECK(!BrawlerTryAttack(game, 0, 5.0f), "zero-ammo attack unexpectedly fired");
    int beforeFailedAttackPulse = player->health;
    session.time = 3.0f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health ==
          beforeFailedAttackPulse + RegenAmount(player, tuning.healthRegenRatio),
          "failed attack incorrectly interrupted regeneration");

    // A successful ultimate follows the same interruption contract.
    Setup(&tuning, &content, &session);
    game = (GameContext){ &session, &tuning, &content };
    player = &session.brawlers[0];
    BrawlerApplyDamage(game, 0, 2500, 1, player->position);
    player->superCharge = 1.0f;
    session.time = 2.8f;
    CHECK(BrawlerTrySuper(game, 0, 5.0f), "test ultimate did not fire");
    int afterSuper = player->health;
    session.time = 5.799f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == afterSuper,
          "successful ultimate did not restart regeneration");
    session.time = 5.8f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == afterSuper + RegenAmount(player, tuning.healthRegenRatio),
          "regeneration did not resume after a successful ultimate");

    // Other healing sources do not count as combat against the recipient.
    Setup(&tuning, &content, &session);
    game = (GameContext){ &session, &tuning, &content };
    player = &session.brawlers[0];
    BrawlerApplyDamage(game, 0, 2500, 1, player->position);
    session.time = 2.0f;
    BrawlerApplyHealing(game, 0, 100, 1, player->position);
    float combatTimeBeforeHealing = player->lastCombatTime;
    int afterExternalHealing = player->health;
    session.time = 3.0f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->lastCombatTime == combatTimeBeforeHealing &&
          player->health ==
          afterExternalHealing + RegenAmount(player, tuning.healthRegenRatio),
          "receiving healing incorrectly interrupted passive regeneration");

    // Frame skips preserve every scheduled pulse instead of collapsing the cadence.
    Setup(&tuning, &content, &session);
    game = (GameContext){ &session, &tuning, &content };
    player = &session.brawlers[0];
    BrawlerApplyDamage(game, 0, 3000, 1, player->position);
    int beforeSkippedPulses = player->health;
    session.time = 5.0f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health ==
          beforeSkippedPulses + RegenAmount(player, tuning.healthRegenRatio)*3,
          "a large simulation step skipped scheduled regeneration pulses");

    // A pulse caps at maximum health without producing overheal.
    Setup(&tuning, &content, &session);
    game = (GameContext){ &session, &tuning, &content };
    player = &session.brawlers[0];
    BrawlerApplyDamage(game, 0, 100, 1, player->position);
    session.time = 3.0f;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == player->maxHealth,
          "passive regeneration exceeded or missed the maximum-health cap");

    // Regeneration never revives and zero ratio is a supported authoring off state.
    Setup(&tuning, &content, &session);
    game = (GameContext){ &session, &tuning, &content };
    player = &session.brawlers[0];
    BrawlerApplyDamage(game, 0, player->health, 1, player->position);
    session.time = 100.0f;
    BrawlersUpdateRegeneration(game);
    CHECK(!player->alive && player->health == 0,
          "passive regeneration revived a defeated brawler");

    Setup(&tuning, &content, &session);
    game = (GameContext){ &session, &tuning, &content };
    player = &session.brawlers[0];
    BrawlerApplyDamage(game, 0, 2500, 1, player->position);
    tuning.healthRegenRatio = 0.0f;
    session.time = 100.0f;
    int disabledHealth = player->health;
    BrawlersUpdateRegeneration(game);
    CHECK(player->health == disabledHealth,
          "zero regeneration ratio did not disable the mechanic");

    puts("Out-of-combat regeneration passed: delay, cadence, resets, symmetry, and caps");
    return 0;
}
