#define _POSIX_C_SOURCE 200809L
#include "app_types.h"
#include "config.h"
#include "weapons.h"
#include "brawler.h"
#include "arena.h"
#include "map_content.h"
#include "content_catalog.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void SetupBrawler(Brawler *b, Team team, BrawlerClass cls,
                         float x, float z, int health, int maxHealth)
{
    *b = (Brawler){ 0 };
    b->position = (Vector3){ x, 0.0f, z };
    b->team = team;
    b->cls = cls;
    b->health = health;
    b->maxHealth = maxHealth;
    b->alive = true;
    b->spawnScale = 1.0f;
    b->ammo = 3.0f;
    b->aiTarget = -1;
}

int main(void)
{
    char local[160], profile[160], legacy[160];
    snprintf(local, sizeof(local), "/tmp/brawl-healer-test-%ld.local", (long)getpid());
    snprintf(profile, sizeof(profile), "/tmp/brawl-healer-test-%ld.profile", (long)getpid());
    snprintf(legacy, sizeof(legacy), "/tmp/brawl-healer-test-%ld.legacy", (long)getpid());
    setenv("BRAWL_PROJECT_CONFIG", "config/gameplay.cfg", 1);
    setenv("BRAWL_TUNING", local, 1);
    setenv("BRAWL_PROFILE", profile, 1);
    setenv("BRAWL_LEGACY_TUNING", legacy, 1);

    App w;
    memset(&w, 0, sizeof(w));
    if (!ConfigInitialize(&w))
    {
        fprintf(stderr, "canonical configuration failed: %s\n", ConfigStatus(&w));
        return 1;
    }
    char mapMessage[256];
    if (!MapCatalogLoad(&w.content, "data/maps/manifest.cfg", mapMessage, sizeof(mapMessage)))
    {
        fprintf(stderr, "map catalog failed: %s\n", mapMessage);
        return 1;
    }
    ArenaLoad(&w.session.arena, MapCatalogSelected(&w.content), w.tune.crateHealth);

    w.session.brawlerCount = 4;
    w.session.playerIdx = 0;
    SetupBrawler(&w.session.brawlers[0], TEAM_PLAYER, CLASS_HEALER, 0, 0, 3400, 3400);
    SetupBrawler(&w.session.brawlers[1], TEAM_PLAYER, CLASS_SHOTGUNNER, 0, 5, 1000, 3800);
    SetupBrawler(&w.session.brawlers[2], TEAM_ENEMY, CLASS_SHOTGUNNER, 0, 5, 3800, 3800);
    SetupBrawler(&w.session.brawlers[3], TEAM_ENEMY, CLASS_SHOTGUNNER, 7, 0, 3800, 3800);
    w.session.brawlers[0].aimAngle = 0.0f;

    const CharacterDefinition *guardian =
        ContentCharacter(&w.content, CLASS_HEALER);
    const AbilityDefinition *rain =
        ContentMainAbility(&w.content, CLASS_HEALER);
    const AbilityDefinition *wave =
        ContentSuperAbility(&w.content, CLASS_HEALER);
    if (strcmp(guardian->id, "guardian") != 0 ||
        strcmp(guardian->modelAsset, "gaia_guardian") != 0 ||
        guardian->role != CHARACTER_ROLE_SUPPORT ||
        rain->behavior != ABILITY_BEHAVIOR_RAIN ||
        wave->behavior != ABILITY_BEHAVIOR_SOUND_WAVE)
    {
        fprintf(stderr, "Guardian typed content metadata is incomplete\n");
        return 2;
    }
    GameContext game = AppGameContext(&w);
    WeaponsFire(game, 0, false, 5.0f);
    int rainFrames = (int)ceilf((rain->data.area.duration + 0.1f)*60.0f);
    for (int i = 0; i < rainFrames; i++) ProjectilesUpdate(game, 1.0f/60.0f);

    int rainPulses = (int)ceilf(rain->data.area.duration/rain->data.area.tickRate - 0.0001f);
    int expectedRainDamage = rainPulses*rain->damage;
    int expectedRainHealing = rainPulses*rain->healing;
    int rainHealing = w.session.brawlers[1].health - 1000;
    int rainDamage = 3800 - w.session.brawlers[2].health;
    if (rainHealing != expectedRainHealing || rainDamage != expectedRainDamage)
    {
        fprintf(stderr, "rain mismatch: expected %d/%d, got %d/%d\n",
                expectedRainHealing, expectedRainDamage, rainHealing, rainDamage);
        return 2;
    }

    w.session.brawlers[1].health = 1000;
    w.session.brawlers[2].health = 3800;
    w.session.brawlers[3].health = 3800;
    WeaponsFire(game, 0, true, 0.0f);
    if (!w.session.brawlers[1].statuses[0].active ||
        !w.session.brawlers[2].statuses[0].active ||
        w.session.brawlers[3].statuses[0].active)
    {
        fprintf(stderr, "sound-wave cone selected the wrong targets\n");
        return 3;
    }

    int waveTicks = (int)floorf(wave->data.area.duration/wave->data.area.tickRate + 0.0001f);
    for (int i = 0; i < waveTicks; i++)
        BrawlersUpdate(game, wave->data.area.tickRate);

    int waveHealing = w.session.brawlers[1].health - 1000;
    int waveDamage = 3800 - w.session.brawlers[2].health;
    if (waveHealing != waveTicks*wave->healing ||
        waveDamage != waveTicks*wave->damage)
    {
        fprintf(stderr, "wave mismatch: expected %d/%d, got %d/%d\n",
                waveTicks*wave->healing, waveTicks*wave->damage,
                waveHealing, waveDamage);
        return 4;
    }

    printf("Guardian config behavior passed: %d rain pulses, %d Resonance ticks\n",
           rainPulses, waveTicks);
    return 0;
}
