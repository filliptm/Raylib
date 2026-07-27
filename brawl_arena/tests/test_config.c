#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "weapons.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); return 1; } \
} while (0)

static bool CopyFile(const char *source, const char *destination)
{
    FILE *in = fopen(source, "rb");
    if (!in) return false;
    FILE *out = fopen(destination, "wb");
    if (!out) { fclose(in); return false; }

    char buffer[4096];
    size_t count;
    bool ok = true;
    while ((count = fread(buffer, 1, sizeof(buffer), in)) > 0)
        if (fwrite(buffer, 1, count, out) != count) { ok = false; break; }

    if (ferror(in)) ok = false;
    if (fclose(in) != 0) ok = false;
    if (fclose(out) != 0) ok = false;
    return ok;
}

static bool WriteText(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    if (!file) return false;
    bool ok = fputs(text, file) >= 0;
    if (fclose(file) != 0) ok = false;
    return ok;
}

static void SetPaths(const char *project, const char *local,
                     const char *profile, const char *legacy)
{
    setenv("BRAWL_PROJECT_CONFIG", project, 1);
    setenv("BRAWL_TUNING", local, 1);
    setenv("BRAWL_PROFILE", profile, 1);
    setenv("BRAWL_LEGACY_TUNING", legacy, 1);
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--validate") == 0)
    {
        char message[256] = { 0 };
        if (!ConfigValidateProjectFile(argv[2], message, sizeof(message)))
        {
            fprintf(stderr, "%s\n", message);
            return 1;
        }
        printf("%s is valid\n", argv[2]);
        return 0;
    }

    char validation[256] = { 0 };
    CHECK(ConfigValidateProjectFile("config/gameplay.cfg", validation, sizeof(validation)),
          validation);

    char directory[128];
    snprintf(directory, sizeof(directory), "/tmp/brawl-config-test-%ld", (long)getpid());
    CHECK(mkdir(directory, 0700) == 0 || errno == EEXIST,
          "could not create temporary test directory");

    char project[512], local[512], profile[512], legacy[512], absent[512];
    snprintf(project, sizeof(project), "%s/gameplay.cfg", directory);
    snprintf(local, sizeof(local), "%s/tuning.local.cfg", directory);
    snprintf(profile, sizeof(profile), "%s/profile.cfg", directory);
    snprintf(legacy, sizeof(legacy), "%s/legacy.cfg", directory);
    snprintf(absent, sizeof(absent), "%s/no-legacy.cfg", directory);
    CHECK(CopyFile("config/gameplay.cfg", project), "could not copy canonical fixture");
    SetPaths(project, local, profile, absent);

    App first = { 0 };
    CHECK(ConfigInitialize(&first), "canonical configuration did not initialize");
    CHECK(first.config.projectLoaded, "project source was not marked loaded");
    CHECK(first.content.weapons[CLASS_HEALER].mainKind == ATTACK_RAIN,
          "Guardian main kind did not load");
    CHECK(first.content.weapons[CLASS_HEALER].superKind == SUPER_SOUND_WAVE,
          "Guardian super kind did not load");
    CHECK(first.content.weapons[CLASS_HEALER].duration == 1.35f &&
          first.content.weapons[CLASS_HEALER].sTickRate == 0.35f,
          "Guardian timing did not load from canonical config");
    CHECK(first.content.weapons[CLASS_BRUISER].selfHealRatio == 0.20f &&
          first.content.weapons[CLASS_BRUISER].mobilityCooldown == 2.50f &&
          first.content.weapons[CLASS_BRUISER].mobilityDuration == 0.18f &&
          first.content.weapons[CLASS_BRUISER].mobilitySpeed == 22.0f,
          "Tank sustain/mobility did not load from canonical config");
    CHECK(ConfigProjectOverrideCount(&first) == 0, "clean load reported draft overrides");

    first.content.weapons[CLASS_HEALER].damage = 123;
    ConfigMarkDirty();
    ConfigFlush(&first);
    CHECK(ConfigKitOverrideCount(&first, CLASS_HEALER) == 1,
          "edited Guardian value was not recognized as a draft");

    App draftReload = { 0 };
    CHECK(ConfigInitialize(&draftReload), "draft reload failed");
    CHECK(draftReload.content.weapons[CLASS_HEALER].damage == 123,
          "draft did not override canonical value");
    CHECK(ConfigKitOverrideCount(&draftReload, CLASS_HEALER) == 1,
          "reloaded draft provenance was lost");

    ConfigResetKitToProject(&draftReload, CLASS_HEALER);
    ConfigFlush(&draftReload);
    CHECK(draftReload.content.weapons[CLASS_HEALER].damage == 100,
          "kit reset did not restore project value");
    CHECK(ConfigKitOverrideCount(&draftReload, CLASS_HEALER) == 0,
          "kit reset left a project override");

    draftReload.content.weapons[CLASS_HEALER].damage = 137;
    ConfigMarkDirty();
    ConfigFlush(&draftReload);
    CHECK(ConfigPromoteKit(&draftReload, CLASS_HEALER), "kit promotion failed");
    CHECK(ConfigKitOverrideCount(&draftReload, CLASS_HEALER) == 0,
          "promoted kit still differs from project baseline");

    App promotedReload = { 0 };
    CHECK(ConfigInitialize(&promotedReload), "promoted project reload failed");
    CHECK(promotedReload.content.weapons[CLASS_HEALER].damage == 137,
          "promoted value was not reproducible from project config");
    CHECK(ConfigProjectOverrideCount(&promotedReload) == 0,
          "promotion did not clear the matching draft override");

    promotedReload.tune.moveSpeed = 12.25f;
    promotedReload.tune.godMode = true;
    promotedReload.content.weapons[CLASS_LOBBER].maxAmmo = 4;
    ConfigMarkDirty();
    ConfigFlush(&promotedReload);
    CHECK(ConfigPromoteAll(&promotedReload), "full project promotion failed");

    App allReload = { 0 };
    CHECK(ConfigInitialize(&allReload), "full promotion reload failed");
    CHECK(allReload.tune.moveSpeed == 12.25f &&
          allReload.content.weapons[CLASS_LOBBER].maxAmmo == 4,
          "full promotion was not reproducible from project config");
    CHECK(allReload.tune.godMode,
          "local-only state was lost while promoting project values");
    CHECK(ConfigProjectOverrideCount(&allReload) == 0,
          "full promotion left project overrides");

    allReload.tune.statWins = 9;
    ConfigMarkDirty();
    ConfigFlush(&allReload);
    App profileReload = { 0 };
    CHECK(ConfigInitialize(&profileReload), "profile reload failed");
    CHECK(profileReload.tune.statWins == 9, "profile state was not isolated/persisted");
    CHECK(ConfigProjectOverrideCount(&profileReload) == 0,
          "profile state polluted project provenance");

    CHECK(WriteText(local,
        "format_version 1\n"
        "kit.guardian.main.tick_interval 9.0\n"),
        "could not create invalid draft");
    App rejectedDraft = { 0 };
    CHECK(ConfigInitialize(&rejectedDraft), "invalid draft damaged canonical startup");
    CHECK(rejectedDraft.content.weapons[CLASS_HEALER].damage == 137 &&
          rejectedDraft.content.weapons[CLASS_HEALER].tickRate == 0.15f,
          "invalid draft partially mutated effective configuration");
    CHECK(strstr(ConfigStatus(&rejectedDraft), "rain timing") != NULL,
          "invalid draft did not surface an actionable status");

    char invalidProject[512];
    snprintf(invalidProject, sizeof(invalidProject), "%s/invalid-project.cfg", directory);
    CHECK(WriteText(invalidProject, "format_version 1\ngameplay.move_speed 11\n"),
          "could not create invalid project fixture");
    CHECK(!ConfigValidateProjectFile(invalidProject, validation, sizeof(validation)),
          "incomplete canonical file unexpectedly validated");

    char duplicateVersion[512];
    snprintf(duplicateVersion, sizeof(duplicateVersion), "%s/duplicate-version.cfg", directory);
    CHECK(WriteText(duplicateVersion, "format_version 1\nformat_version 1\n"),
          "could not create duplicate-version fixture");
    CHECK(!ConfigValidateProjectFile(duplicateVersion, validation, sizeof(validation)),
          "duplicate canonical format version unexpectedly validated");

    char importedLocal[512], importedProfile[512];
    snprintf(importedLocal, sizeof(importedLocal), "%s/imported.local.cfg", directory);
    snprintf(importedProfile, sizeof(importedProfile), "%s/imported.profile.cfg", directory);
    CHECK(WriteText(legacy,
        "version 1\n"
        "kit4_damage 999\n"
        "stat_wins 7\n"),
        "could not create legacy fixture");
    SetPaths(project, importedLocal, importedProfile, legacy);

    App imported = { 0 };
    CHECK(ConfigInitialize(&imported), "legacy import initialization failed");
    CHECK(imported.config.legacyImported, "legacy source was not reported as imported");
    CHECK(imported.content.weapons[CLASS_HEALER].damage == 137,
          "version-1 Guardian values were not migrated to project semantics");
    CHECK(imported.tune.statWins == 7, "legacy profile statistics were not imported");
    CHECK(FileExists(importedLocal) && FileExists(importedProfile),
          "legacy import did not create split local/profile files");

    puts("configuration layering, promotion, reset, rejection, profile, and migration passed");
    return 0;
}
