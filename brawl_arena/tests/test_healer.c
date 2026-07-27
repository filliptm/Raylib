#define _POSIX_C_SOURCE 200809L
#include "../src/types.h"
#include "../src/config.h"
#include "../src/weapons.h"
#include "../src/brawler.h"
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

    World w;
    memset(&w, 0, sizeof(w));
    if (!ConfigInitialize(&w))
    {
        fprintf(stderr, "canonical configuration failed: %s\n", ConfigStatus(&w));
        return 1;
    }

    w.brawlerCount = 4;
    w.playerIdx = 0;
    SetupBrawler(&w.brawlers[0], TEAM_PLAYER, CLASS_HEALER, 0, 0, 3400, 3400);
    SetupBrawler(&w.brawlers[1], TEAM_PLAYER, CLASS_SHOTGUNNER, 0, 5, 1000, 3800);
    SetupBrawler(&w.brawlers[2], TEAM_ENEMY, CLASS_SHOTGUNNER, 0, 5, 3800, 3800);
    SetupBrawler(&w.brawlers[3], TEAM_ENEMY, CLASS_SHOTGUNNER, 7, 0, 3800, 3800);
    w.brawlers[0].aimAngle = 0.0f;

    const WeaponDef *guardian = &WEAPONS[CLASS_HEALER];
    WeaponsFire(&w, 0, false, 5.0f);
    int rainFrames = (int)ceilf((guardian->duration + 0.1f)*60.0f);
    for (int i = 0; i < rainFrames; i++) ProjectilesUpdate(&w, 1.0f/60.0f);

    int rainPulses = (int)ceilf(guardian->duration/guardian->tickRate - 0.0001f);
    int expectedRainDamage = rainPulses*guardian->damage;
    int expectedRainHealing = rainPulses*guardian->healing;
    int rainHealing = w.brawlers[1].health - 1000;
    int rainDamage = 3800 - w.brawlers[2].health;
    if (rainHealing != expectedRainHealing || rainDamage != expectedRainDamage)
    {
        fprintf(stderr, "rain mismatch: expected %d/%d, got %d/%d\n",
                expectedRainHealing, expectedRainDamage, rainHealing, rainDamage);
        return 2;
    }

    w.brawlers[1].health = 1000;
    w.brawlers[2].health = 3800;
    w.brawlers[3].health = 3800;
    WeaponsFire(&w, 0, true, 0.0f);
    if (w.brawlers[1].resonanceTimer <= 0.0f ||
        w.brawlers[2].resonanceTimer <= 0.0f ||
        w.brawlers[3].resonanceTimer > 0.0f)
    {
        fprintf(stderr, "sound-wave cone selected the wrong targets\n");
        return 3;
    }

    int waveTicks = (int)floorf(guardian->sDuration/guardian->sTickRate + 0.0001f);
    for (int i = 0; i < waveTicks; i++) BrawlersUpdate(&w, guardian->sTickRate);

    int waveHealing = w.brawlers[1].health - 1000;
    int waveDamage = 3800 - w.brawlers[2].health;
    if (waveHealing != waveTicks*guardian->sHealing ||
        waveDamage != waveTicks*guardian->sDamage)
    {
        fprintf(stderr, "wave mismatch: expected %d/%d, got %d/%d\n",
                waveTicks*guardian->sHealing, waveTicks*guardian->sDamage,
                waveHealing, waveDamage);
        return 4;
    }

    printf("Guardian config behavior passed: %d rain pulses, %d Resonance ticks\n",
           rainPulses, waveTicks);
    return 0;
}
