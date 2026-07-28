#include "game_commands.h"

#include "arena.h"
#include "brawler.h"
#include "gems.h"
#include "game_events.h"
#include "content_catalog.h"

static BrawlerClass BotKitFor(const App *app, int slot)
{
    return app->tune.botMixedKits
         ? (BrawlerClass)(slot%CLASS_COUNT) : app->tune.botKit;
}

static void SetBotCount(App *app, int count)
{
    GameContext game = AppGameContext(app);
    if (count < 0) count = 0;
    if (count > MAX_BRAWLERS - 1) count = MAX_BRAWLERS - 1;
    int current = app->session.brawlerCount - 1;
    if (count == current) return;

    if (count > current)
    {
        for (int slot = current; slot < count; slot++)
        {
            Vector3 position = ArenaSpawnFor(&app->session.arena, TEAM_ENEMY, slot);
            BrawlerSpawn(game, 1 + slot, TEAM_ENEMY, BotKitFor(app, slot),
                         position, false);
        }
    }
    else
    {
        // Despawned bots put their gems back on the floor so team totals stay honest.
        for (int i = count + 1; i <= current; i++)
            GemsDropFrom(game, i);
        for (int i = 0; i < MAX_PROJECTILES; i++)
            if (app->session.projectiles[i].active && app->session.projectiles[i].owner > count)
                app->session.projectiles[i].active = false;
        for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
            if (app->session.abilityFields[i].active &&
                app->session.abilityFields[i].owner > count)
                app->session.abilityFields[i].active = false;
        for (int i = 0; i <= count; i++)
            for (int effect = 0; effect < MAX_STATUS_EFFECTS; effect++)
                if (app->session.brawlers[i].statuses[effect].active &&
                    app->session.brawlers[i].statuses[effect].source > count)
                    app->session.brawlers[i].statuses[effect] = (StatusEffect){ 0 };
    }
    app->session.brawlerCount = count + 1;
}

static void RespawnBots(App *app)
{
    GameContext game = AppGameContext(app);
    for (int i = 1; i < app->session.brawlerCount; i++)
    {
        Vector3 position = ArenaSpawnFor(&app->session.arena, TEAM_ENEMY, i - 1);
        BrawlerSpawn(game, i, TEAM_ENEMY, BotKitFor(app, i - 1),
                     position, false);
    }
}

static bool SetPlayerClass(App *app, int classId)
{
    if (classId < 0 || classId >= CLASS_COUNT ||
        app->session.playerIdx < 0 ||
        app->session.playerIdx >= app->session.brawlerCount) return false;

    int index = app->session.playerIdx;
    Brawler *player = &app->session.brawlers[index];
    if (player->cls == (BrawlerClass)classId) return true;

    // A dead brawler only needs the class recorded; the pending respawn rebuilds
    // every derived stat, and swapping must not cancel the respawn timer.
    if (!player->alive)
    {
        player->cls = (BrawlerClass)classId;
        return true;
    }

    // Swapping kit is a loadout change, not a death: super and gems carry over, and
    // health carries as a ratio so the swap can never act as an instant heal. Recent
    // combat blocks it outside the sandbox for the same reason.
    if (!app->session.sandbox &&
        app->session.time - player->lastCombatTime < app->tune.classSwapLockout)
        return false;

    Vector3 position = player->position;
    float superCharge = player->superCharge;
    float healthRatio = player->maxHealth > 0
                      ? (float)player->health/(float)player->maxHealth : 1.0f;
    int gems = player->gems;
    int spawnSlot = player->spawnSlot;
    BrawlerSpawn(AppGameContext(app), index, TEAM_PLAYER,
                 (BrawlerClass)classId, position, true);
    Brawler *swapped = &app->session.brawlers[index];
    swapped->superCharge = superCharge;
    swapped->gems = gems;
    swapped->spawnSlot = spawnSlot;
    swapped->health = (int)(swapped->maxHealth*healthRatio);
    if (swapped->health < 1) swapped->health = 1;
    GameEmitFloatText(&app->session, position,
                      ContentCharacter(&app->content, (BrawlerClass)classId)->displayName,
                      (Color){ 255, 255, 255, 255 });
    return true;
}

static bool SyncClassHealth(App *app, int classId)
{
    if (classId < 0 || classId >= CLASS_COUNT) return false;
    int maximum =
        ContentCharacter(&app->content, (BrawlerClass)classId)->maxHealth;
    if (maximum < 1) maximum = 1;

    for (int i = 0; i < app->session.brawlerCount; i++)
    {
        Brawler *brawler = &app->session.brawlers[i];
        if (brawler->cls != (BrawlerClass)classId || brawler->maxHealth == maximum) continue;
        float ratio = brawler->maxHealth > 0
                    ? (float)brawler->health/(float)brawler->maxHealth : 1.0f;
        brawler->maxHealth = maximum;
        brawler->health = (int)(maximum*ratio);
        if (brawler->health < 1) brawler->health = 1;
    }
    return true;
}

bool GameCommandExecute(App *app, GameCommand command)
{
    if (!app) return false;
    GameContext game = AppGameContext(app);
    Brawler *player = 0;
    if (app->session.playerIdx >= 0 &&
        app->session.playerIdx < app->session.brawlerCount)
        player = &app->session.brawlers[app->session.playerIdx];

    switch (command.type)
    {
        case GAME_COMMAND_SET_BOT_COUNT:
            SetBotCount(app, command.value);
            return true;
        case GAME_COMMAND_RESPAWN_BOTS:
            RespawnBots(app);
            return true;
        case GAME_COMMAND_KILL_BOTS:
            // A debug wipe has no attacker: it must not bank KOs or award super.
            for (int i = 1; i < app->session.brawlerCount; i++)
                if (app->session.brawlers[i].alive)
                    BrawlerApplyDamage(game, i, app->session.brawlers[i].health,
                                       -1, app->session.brawlers[i].position);
            return true;
        case GAME_COMMAND_HEAL_BOTS:
            for (int i = 1; i < app->session.brawlerCount; i++)
                app->session.brawlers[i].health = app->session.brawlers[i].maxHealth;
            return true;
        case GAME_COMMAND_SET_PLAYER_CLASS:
            return SetPlayerClass(app, command.value);
        case GAME_COMMAND_CHARGE_PLAYER_SUPER:
            if (!player) return false;
            player->superCharge = 1.0f;
            return true;
        case GAME_COMMAND_HEAL_PLAYER:
            if (!player) return false;
            player->health = player->maxHealth;
            return true;
        case GAME_COMMAND_RESPAWN_PLAYER:
            if (!player) return false;
            BrawlerRespawn(game, app->session.playerIdx);
            return true;
        case GAME_COMMAND_RESET_OBJECTIVE:
            MatchReset(game);
            return true;
        case GAME_COMMAND_SPAWN_GEM:
            GemSpawnAt(game, app->session.arena.gemVent,
                       (Vector3){ 0.0f, 3.0f, 0.0f });
            return true;
        case GAME_COMMAND_CLEAR_GEMS:
            for (int i = 0; i < MAX_GEMS; i++) app->session.gems[i].active = false;
            return true;
        case GAME_COMMAND_SYNC_CLASS_HEALTH:
            return SyncClassHealth(app, command.value);
        case GAME_COMMAND_RESET_SCORE:
            app->session.kills = 0;
            app->session.deaths = 0;
            return true;
    }
    return false;
}
