/*******************************************************************************************
*   CONFIG
*
*   Persists everything the command center can edit to a plain text file next to the
*   binary, and reloads it on launch so a tuning session survives a restart.
*
*   The field table is built at runtime from live pointers, so save and load can never
*   drift apart: adding a control means adding exactly one row.
********************************************************************************************/
#include "config.h"
#include "weapons.h"
#include <stdio.h>
#include <string.h>

#define MAX_FIELDS 128
#define SAVE_DELAY 0.6f

typedef enum { F_FLOAT = 0, F_INT, F_BOOL } FieldType;

typedef struct Field {
    char key[40];
    FieldType type;
    void *ptr;
} Field;

static bool g_dirty = false;
static float g_timer = 0.0f;

//------------------------------------------------------------------------------------
static int AddField(Field *fields, int count, const char *key, FieldType type, void *ptr)
{
    if (count >= MAX_FIELDS) return count;
    snprintf(fields[count].key, sizeof(fields[count].key), "%s", key);
    fields[count].type = type;
    fields[count].ptr = ptr;
    return count + 1;
}

// One place that knows every persisted value.
static int BuildFields(World *w, Field *f)
{
    Tuning *t = &w->tune;
    int n = 0;

    n = AddField(f, n, "move_speed",     F_FLOAT, &t->moveSpeed);
    n = AddField(f, n, "move_accel",     F_FLOAT, &t->moveAccel);
    n = AddField(f, n, "dash_speed",     F_FLOAT, &t->dashSpeed);
    n = AddField(f, n, "bush_reveal",    F_FLOAT, &t->bushReveal);
    n = AddField(f, n, "fire_reveal",    F_FLOAT, &t->fireReveal);
    n = AddField(f, n, "player_respawn", F_FLOAT, &t->playerRespawn);
    n = AddField(f, n, "enemy_respawn",  F_FLOAT, &t->enemyRespawn);
    n = AddField(f, n, "time_scale",     F_FLOAT, &t->timeScale);
    n = AddField(f, n, "super_mult",     F_FLOAT, &t->superMult);
    n = AddField(f, n, "god_mode",       F_BOOL,  &t->godMode);
    n = AddField(f, n, "infinite_ammo",  F_BOOL,  &t->infiniteAmmo);
    n = AddField(f, n, "show_debug",     F_BOOL,  &t->showDebug);
    n = AddField(f, n, "post_fx",        F_BOOL,  &t->postFx);
    n = AddField(f, n, "bloom",          F_FLOAT, &t->bloom);
    n = AddField(f, n, "grass_height",   F_FLOAT, &t->grassHeight);
    n = AddField(f, n, "wind_strength",  F_FLOAT, &t->windStrength);
    n = AddField(f, n, "wind_speed",     F_FLOAT, &t->windSpeed);
    n = AddField(f, n, "grass_bend_r",   F_FLOAT, &t->grassBendRadius);
    n = AddField(f, n, "grass_bend_s",   F_FLOAT, &t->grassBendStrength);
    n = AddField(f, n, "conceal_dither", F_FLOAT, &t->concealDither);
    n = AddField(f, n, "bot_mode",       F_INT,   &t->botMode);
    n = AddField(f, n, "bot_count",      F_INT,   &t->botCount);
    n = AddField(f, n, "bot_kit",        F_INT,   &t->botKit);
    n = AddField(f, n, "bot_mixed",      F_BOOL,  &t->botMixedKits);

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        WeaponDef *k = &WEAPONS[i];
        char key[40];

        #define KIT_FIELD(name, type, member) \
            snprintf(key, sizeof(key), "kit%d_%s", i, name); \
            n = AddField(f, n, key, type, &k->member);

        KIT_FIELD("health",      F_INT,   maxHealth)
        KIT_FIELD("pellets",     F_INT,   pellets)
        KIT_FIELD("damage",      F_INT,   damage)
        KIT_FIELD("spread",      F_FLOAT, spreadDeg)
        KIT_FIELD("speed",       F_FLOAT, speed)
        KIT_FIELD("range",       F_FLOAT, range)
        KIT_FIELD("projradius",  F_FLOAT, projRadius)
        KIT_FIELD("cooldown",    F_FLOAT, cooldown)
        KIT_FIELD("reload",      F_FLOAT, reloadPerAmmo)
        KIT_FIELD("superperhit", F_FLOAT, superPerHit)
        KIT_FIELD("sdamage",     F_INT,   sDamage)
        KIT_FIELD("spellets",    F_INT,   sPellets)
        KIT_FIELD("sspread",     F_FLOAT, sSpreadDeg)
        KIT_FIELD("srange",      F_FLOAT, sRange)

        #undef KIT_FIELD
    }

    return n;
}

//------------------------------------------------------------------------------------
void ConfigSave(const World *w)
{
    Field fields[MAX_FIELDS];
    int count = BuildFields((World *)w, fields);

    FILE *file = fopen(CONFIG_PATH, "w");
    if (!file) return;

    fprintf(file, "# Brawl Arena tuning - written automatically by the command center.\n");
    fprintf(file, "# Delete this file to go back to the built-in defaults.\n");
    fprintf(file, "version 1\n");

    for (int i = 0; i < count; i++)
    {
        switch (fields[i].type)
        {
            case F_FLOAT: fprintf(file, "%s %.4f\n", fields[i].key, *(float *)fields[i].ptr); break;
            case F_INT:   fprintf(file, "%s %d\n",   fields[i].key, *(int *)fields[i].ptr);   break;
            case F_BOOL:  fprintf(file, "%s %d\n",   fields[i].key, *(bool *)fields[i].ptr ? 1 : 0); break;
        }
    }

    fclose(file);
}

bool ConfigLoad(World *w)
{
    FILE *file = fopen(CONFIG_PATH, "r");
    if (!file) return false;

    Field fields[MAX_FIELDS];
    int count = BuildFields(w, fields);

    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == '#' || line[0] == '\n') continue;

        char key[64];
        double value;
        if (sscanf(line, "%63s %lf", key, &value) != 2) continue;
        if (strcmp(key, "version") == 0) continue;

        for (int i = 0; i < count; i++)
        {
            if (strcmp(key, fields[i].key) != 0) continue;

            switch (fields[i].type)
            {
                case F_FLOAT: *(float *)fields[i].ptr = (float)value; break;
                case F_INT:   *(int *)fields[i].ptr = (int)value;     break;
                case F_BOOL:  *(bool *)fields[i].ptr = (value != 0);  break;
            }
            break;
        }
    }

    fclose(file);

    // Guard against a hand-edited or corrupted file putting the game in a broken state.
    Tuning *t = &w->tune;
    if (t->moveSpeed < 0.5f) t->moveSpeed = 0.5f;
    if (t->moveAccel < 1.0f) t->moveAccel = 1.0f;
    if (t->timeScale < 0.01f) t->timeScale = 0.01f;
    if (t->playerRespawn < 0.1f) t->playerRespawn = 0.1f;
    if (t->enemyRespawn < 0.1f) t->enemyRespawn = 0.1f;
    if (t->grassHeight < 0.1f) t->grassHeight = 0.1f;
    if (t->grassBendRadius < 0.05f) t->grassBendRadius = 0.05f;
    if (t->concealDither < 0.0f) t->concealDither = 0.0f;
    if (t->concealDither > 0.95f) t->concealDither = 0.95f;
    if (t->botCount < 0) t->botCount = 0;
    if (t->botCount > MAX_BRAWLERS - 1) t->botCount = MAX_BRAWLERS - 1;
    if (t->botMode < 0 || t->botMode >= BOT_MODE_COUNT) t->botMode = BOT_STATIC;
    if (t->botKit < 0 || t->botKit >= CLASS_COUNT) t->botKit = CLASS_SHOTGUNNER;

    for (int i = 0; i < CLASS_COUNT; i++)
    {
        if (WEAPONS[i].maxHealth < 1) WEAPONS[i].maxHealth = 1;
        if (WEAPONS[i].pellets < 1) WEAPONS[i].pellets = 1;
        if (WEAPONS[i].cooldown < 0.02f) WEAPONS[i].cooldown = 0.02f;
        if (WEAPONS[i].reloadPerAmmo < 0.05f) WEAPONS[i].reloadPerAmmo = 0.05f;
        if (WEAPONS[i].speed < 1.0f) WEAPONS[i].speed = 1.0f;
        if (WEAPONS[i].range < 0.5f) WEAPONS[i].range = 0.5f;
    }

    return true;
}

void ConfigMarkDirty(void)
{
    g_dirty = true;
    g_timer = 0.0f;
}

void ConfigAutoSave(World *w, float realDt)
{
    if (!g_dirty) return;

    g_timer += realDt;
    if (g_timer < SAVE_DELAY) return;

    ConfigSave(w);
    g_dirty = false;
    g_timer = 0.0f;
}
