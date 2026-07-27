#define _POSIX_C_SOURCE 200809L
#include "../src/types.h"
#include "../src/brawler.h"
#include "../src/config.h"
#include "../src/effects.h"
#include "../src/weapons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void SetupBrawler(Brawler *b, Team team, BrawlerClass cls,
                         float x, float z, bool isPlayer)
{
    *b = (Brawler){ 0 };
    b->position = (Vector3){ x, 0.0f, z };
    b->team = team;
    b->cls = cls;
    b->isPlayer = isPlayer;
    b->health = WEAPONS[cls].maxHealth;
    b->maxHealth = WEAPONS[cls].maxHealth;
    b->ammo = (float)WEAPONS[cls].maxAmmo;
    b->alive = true;
    b->spawnScale = 1.0f;
    b->aiTarget = -1;
}

static int ExpectStill(const World *w, const char *action)
{
    if (w->shake <= 0.0f) return 0;
    fprintf(stderr, "%s produced camera shake: %.3f\n", action, w->shake);
    return 1;
}

int main(void)
{
    char local[160], profile[160], legacy[160];
    snprintf(local, sizeof(local), "/tmp/brawl-shake-test-%ld.local", (long)getpid());
    snprintf(profile, sizeof(profile), "/tmp/brawl-shake-test-%ld.profile", (long)getpid());
    snprintf(legacy, sizeof(legacy), "/tmp/brawl-shake-test-%ld.legacy", (long)getpid());
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

    for (int cls = 0; cls < CLASS_COUNT; cls++)
    {
        memset(w.projectiles, 0, sizeof(w.projectiles));
        memset(w.abilityFields, 0, sizeof(w.abilityFields));
        w.brawlerCount = 2;
        w.playerIdx = 0;
        SetupBrawler(&w.brawlers[0], TEAM_PLAYER, (BrawlerClass)cls, 0, 0, true);
        SetupBrawler(&w.brawlers[1], TEAM_ENEMY, CLASS_SHOTGUNNER, 0, 5, false);

        w.shake = 0.0f;
        WeaponsFire(&w, 0, false, 5.0f);
        if (ExpectStill(&w, "main attack")) return 2;

        w.shake = 0.0f;
        WeaponsFire(&w, 0, true, 5.0f);
        if (ExpectStill(&w, "super attack")) return 3;
    }

    w.shake = 0.0f;
    FxExplosion(&w, (Vector3){ 0, 0, 0 }, 2.6f, WHITE);
    if (ExpectStill(&w, "attack explosion")) return 4;

    w.shake = 0.0f;
    FxCrateBreak(&w, (Vector3){ 0, 0, 0 });
    if (ExpectStill(&w, "attack crate break")) return 5;

    w.shake = 0.0f;
    FxDeathBurst(&w, (Vector3){ 0, 0, 0 }, WHITE);
    if (ExpectStill(&w, "attack elimination")) return 6;

    w.shake = 0.0f;
    w.brawlers[0].health = w.brawlers[0].maxHealth;
    w.brawlers[0].alive = true;
    BrawlerApplyDamage(&w, 0, 1, 1, w.brawlers[0].position);
    if (ExpectStill(&w, "player damage")) return 7;

    printf("Combat camera test passed: all %d kits, impacts, damage, and eliminations stay still\n",
           CLASS_COUNT);
    return 0;
}
