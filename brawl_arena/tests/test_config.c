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

static bool FileContains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char line[320];
    bool found = false;
    while (fgets(line, sizeof(line), file))
    {
        if (strstr(line, needle))
        {
            found = true;
            break;
        }
    }
    fclose(file);
    return found;
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
    CHECK(first.content.weapons[CLASS_BRUISER].secondaryKind == SECONDARY_DASH &&
          first.content.weapons[CLASS_BRUISER].selfHealRatio > 0.0f &&
          first.content.weapons[CLASS_BRUISER].mobilityCooldown == 2.50f &&
          first.content.weapons[CLASS_BRUISER].mobilityDuration == 0.18f &&
          first.content.weapons[CLASS_BRUISER].mobilitySpeed == 22.0f,
          "Tank sustain/secondary did not load from canonical config");
    CHECK(first.content.weapons[CLASS_SHOTGUNNER].mainKind == ATTACK_RETURNING &&
          first.content.weapons[CLASS_SHOTGUNNER].superKind == SUPER_RETURNING &&
          first.content.weapons[CLASS_SHOTGUNNER].secondaryKind == SECONDARY_SHIELD &&
          first.content.weapons[CLASS_SHOTGUNNER].secondaryCapacity == 1200 &&
          first.content.weapons[CLASS_SHOTGUNNER].secondaryHealRatio == 0.30f &&
          first.content.weapons[CLASS_SHOTGUNNER].secondaryRechargeDelay == 3.0f &&
          first.content.weapons[CLASS_SHOTGUNNER].secondaryRechargeRate == 300.0f &&
          first.content.weapons[CLASS_SHOTGUNNER].secondaryBreakLockout == 5.0f,
          "Scrapper rework did not load from canonical config");
    CHECK(first.content.weapons[CLASS_SNIPER].secondaryKind ==
              SECONDARY_GRAPPLE &&
          first.content.weapons[CLASS_SNIPER].mobilityCooldown == 7.5f &&
          first.content.weapons[CLASS_SNIPER].mobilityDuration == 0.45f &&
          first.content.weapons[CLASS_SNIPER].secondaryRange == 10.0f &&
          first.content.weapons[CLASS_SNIPER].secondaryDelay == 0.15f,
          "Longshot Grapple did not load from canonical config");
    CHECK(first.content.weapons[CLASS_LOBBER].secondaryKind ==
              SECONDARY_MINE &&
          first.content.weapons[CLASS_LOBBER].mobilityCooldown == 8.0f &&
          first.content.weapons[CLASS_LOBBER].secondaryDelay == 0.55f &&
          first.content.weapons[CLASS_LOBBER].secondaryTriggerRadius == 2.4f &&
          first.content.weapons[CLASS_LOBBER].secondaryRadius == 3.2f &&
          first.content.weapons[CLASS_LOBBER].secondaryDamage == 400 &&
          first.content.weapons[CLASS_LOBBER].secondaryKnockback == 4.5f,
          "Mortar Concussion Mine did not load from canonical config");
    CHECK(first.content.showcase.yawDegrees == 180.0f &&
          first.content.showcase.scale == 0.90f &&
          first.content.showcase.cameraPosition.y == 2.70f &&
          first.content.showcase.cameraPosition.z == -7.60f &&
          first.content.showcase.cameraTarget.y == 1.40f &&
          first.content.showcase.verticalFov == 40.0f,
          "shared showcase did not load from canonical config");
    CHECK(first.tune.healthRegenDelay == 3.0f &&
          first.tune.healthRegenInterval == 1.0f &&
          first.tune.healthRegenRatio == 0.13f,
          "health regeneration did not load from canonical config");
    CHECK(ConfigProjectOverrideCount(&first) == 0, "clean load reported draft overrides");

    int projectGuardianDamage =
        first.content.weapons[CLASS_HEALER].damage;
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
    CHECK(draftReload.content.weapons[CLASS_HEALER].damage ==
              projectGuardianDamage,
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
    promotedReload.tune.healthRegenRatio = 0.18f;
    promotedReload.tune.godMode = true;
    promotedReload.content.weapons[CLASS_LOBBER].maxAmmo = 4;
    ConfigMarkDirty();
    ConfigFlush(&promotedReload);
    CHECK(ConfigPromoteAll(&promotedReload), "full project promotion failed");

    App allReload = { 0 };
    CHECK(ConfigInitialize(&allReload), "full promotion reload failed");
    CHECK(allReload.tune.moveSpeed == 12.25f &&
          allReload.tune.healthRegenRatio == 0.18f &&
          allReload.content.weapons[CLASS_LOBBER].maxAmmo == 4,
          "full promotion was not reproducible from project config");
    CHECK(allReload.tune.godMode,
          "local-only state was lost while promoting project values");
    CHECK(ConfigProjectOverrideCount(&allReload) == 0,
          "full promotion left project overrides");

    allReload.tune.statWins = 9;
    allReload.uiPreferences.scale = 1.30f;
    allReload.uiPreferences.reducedMotion = true;
    allReload.uiPreferences.highContrast = true;
    allReload.uiPreferences.showTutorialHints = false;
    allReload.uiPreferences.inputGlyphMode = 2;
    allReload.uiPreferences.tutorialFlags = 37;
    ConfigMarkDirty();
    ConfigFlush(&allReload);
    App profileReload = { 0 };
    CHECK(ConfigInitialize(&profileReload), "profile reload failed");
    CHECK(profileReload.tune.statWins == 9, "profile state was not isolated/persisted");
    CHECK(profileReload.uiPreferences.scale == 1.30f &&
          profileReload.uiPreferences.reducedMotion &&
          profileReload.uiPreferences.highContrast &&
          !profileReload.uiPreferences.showTutorialHints &&
          profileReload.uiPreferences.inputGlyphMode == 2 &&
          profileReload.uiPreferences.tutorialFlags == 37,
          "UI preferences did not round-trip through profile scope");
    CHECK(ConfigProjectOverrideCount(&profileReload) == 0,
          "profile state polluted project provenance");

    CHECK(WriteText(local,
        "format_version 3\n"
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
    CHECK(WriteText(invalidProject, "format_version 3\ngameplay.move_speed 11\n"),
          "could not create invalid project fixture");
    CHECK(!ConfigValidateProjectFile(invalidProject, validation, sizeof(validation)),
          "incomplete canonical file unexpectedly validated");

    char duplicateVersion[512];
    snprintf(duplicateVersion, sizeof(duplicateVersion), "%s/duplicate-version.cfg", directory);
    CHECK(WriteText(duplicateVersion, "format_version 3\nformat_version 3\n"),
          "could not create duplicate-version fixture");
    CHECK(!ConfigValidateProjectFile(duplicateVersion, validation, sizeof(validation)),
          "duplicate canonical format version unexpectedly validated");

    char v1Project[512], v1Local[512], v1Profile[512];
    snprintf(v1Project, sizeof(v1Project), "%s/v1-project.cfg", directory);
    snprintf(v1Local, sizeof(v1Local), "%s/v1-local.cfg", directory);
    snprintf(v1Profile, sizeof(v1Profile), "%s/v1-profile.cfg", directory);
    CHECK(WriteText(v1Project,
        "format_version 1\n"
        "gameplay.move_speed 7.250000\n"
        "preview.scrapper.home_yaw_degrees 171.000000\n"
        "preview.scrapper.select_yaw_degrees 120.000000\n"
        "preview.scrapper.home_scale 0.880000\n"
        "preview.scrapper.select_scale 1.200000\n"
        "preview.scrapper.home_offset_x 0.400000\n"
        "preview.scrapper.home_offset_y -0.200000\n"
        "preview.scrapper.select_offset_x 3.000000\n"
        "preview.scrapper.select_offset_y 1.000000\n"
        "preview.scrapper.camera_target_y 1.550000\n"
        "preview.scrapper.camera_distance 8.200000\n"
        "preview.tank.home_yaw_degrees 205.000000\n"
        "kit.tank.mobility.cooldown 2.750000\n"
        "kit.tank.mobility.duration 0.200000\n"
        "kit.tank.mobility.speed 24.000000\n"
        "kit.longshot.main.damage 1111\n"
        "kit.scrapper.main.kind projectile\n"),
        "could not create v1 typed migration fixture");
    SetPaths(v1Project, v1Local, v1Profile, absent);
    App migrated = { 0 };
    CHECK(ConfigInitialize(&migrated), "v1 typed project migration failed");
    CHECK(migrated.tune.moveSpeed == 7.25f &&
          migrated.content.weapons[CLASS_SNIPER].damage == 1111,
          "v1 migration did not preserve unrelated project values");
    CHECK(migrated.content.weapons[CLASS_BRUISER].secondaryKind ==
              SECONDARY_DASH &&
          migrated.content.weapons[CLASS_BRUISER].mobilityCooldown == 2.75f &&
          migrated.content.weapons[CLASS_BRUISER].mobilityDuration == 0.20f &&
          migrated.content.weapons[CLASS_BRUISER].mobilitySpeed == 24.0f,
          "v1 Tank mobility did not migrate to a dash secondary");
    CHECK(migrated.content.weapons[CLASS_SHOTGUNNER].mainKind ==
              ATTACK_RETURNING &&
          migrated.content.weapons[CLASS_SHOTGUNNER].secondaryKind ==
              SECONDARY_SHIELD,
          "obsolete v1 Scrapper weapon values replaced the reworked kit");
    CHECK(migrated.content.showcase.yawDegrees == 171.0f &&
          migrated.content.showcase.scale == 0.88f &&
          migrated.content.showcase.offset.x == 0.40f &&
          migrated.content.showcase.offset.y == -0.20f &&
          migrated.content.showcase.cameraTarget.y == 1.55f &&
          migrated.content.showcase.cameraPosition.z == -8.20f,
          "v1 Scrapper home framing did not derive the global showcase");
    CHECK(ConfigPromoteAll(&migrated),
          "migrated v1 project could not be saved as v3");
    CHECK(FileContains(v1Project, "format_version 3"),
          "save after v1 migration did not emit schema v3");

    char v2Project[512], v2Local[512], v2Profile[512];
    snprintf(v2Project, sizeof(v2Project), "%s/v2-project.cfg", directory);
    snprintf(v2Local, sizeof(v2Local), "%s/v2-local.cfg", directory);
    snprintf(v2Profile, sizeof(v2Profile), "%s/v2-profile.cfg", directory);
    CHECK(WriteText(v2Project,
        "format_version 2\n"
        "gameplay.move_speed 8.500000\n"
        "kit.longshot.main.damage 1337\n"
        "kit.scrapper.secondary.kind guard\n"
        "kit.scrapper.secondary.cooldown 4.000000\n"
        "kit.scrapper.secondary.duration 0.750000\n"
        "kit.scrapper.secondary.speed 0.000000\n"
        "kit.scrapper.secondary.capacity 900\n"
        "kit.scrapper.secondary.arc_degrees 110.000000\n"
        "kit.scrapper.secondary.move_multiplier 0.700000\n"
        "kit.scrapper.secondary.counter_range 3.250000\n"
        "kit.scrapper.secondary.counter_damage_min 100\n"
        "kit.scrapper.secondary.counter_damage_max 340\n"
        "kit.scrapper.secondary.counter_knockback_min 1.500000\n"
        "kit.scrapper.secondary.counter_knockback_max 3.000000\n"),
        "could not create v2 typed migration fixture");
    SetPaths(v2Project, v2Local, v2Profile, absent);
    App migratedV2 = { 0 };
    CHECK(ConfigInitialize(&migratedV2), "v2 typed project migration failed");
    CHECK(migratedV2.tune.moveSpeed == 8.50f &&
          migratedV2.content.weapons[CLASS_SNIPER].damage == 1337,
          "v2 migration did not preserve unrelated project values");
    CHECK(migratedV2.content.weapons[CLASS_SHOTGUNNER].secondaryKind ==
              SECONDARY_SHIELD &&
          migratedV2.content.weapons[CLASS_SHOTGUNNER].secondaryCapacity == 900 &&
          migratedV2.content.weapons[CLASS_SHOTGUNNER].secondaryMoveMultiplier ==
              0.70f &&
          migratedV2.content.weapons[CLASS_SHOTGUNNER].secondaryHealRatio ==
              0.30f &&
          migratedV2.content.weapons[CLASS_SHOTGUNNER].secondaryRechargeDelay ==
              3.0f &&
          migratedV2.content.weapons[CLASS_SHOTGUNNER].secondaryRechargeRate ==
              300.0f &&
          migratedV2.content.weapons[CLASS_SHOTGUNNER].secondaryBreakLockout ==
              5.0f,
          "v2 Guard values did not migrate to the renewable shield");
    CHECK(ConfigPromoteAll(&migratedV2),
          "migrated v2 project could not be saved as v3");
    CHECK(FileContains(v2Project, "format_version 3") &&
          FileContains(v2Project, "kit.scrapper.secondary.kind shield") &&
          FileContains(v2Project,
                       "kit.scrapper.secondary.recharge_rate 300.000000"),
          "save after v2 migration did not emit canonical shield schema");

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

    // The runtime load path tolerates a project file that predates newly added
    // keys: present keys apply, missing keys fall back to compiled defaults, and
    // the status names the situation. The explicit validator stays strict.
    char staleProject[512], staleLocal[512], staleProfile[512];
    snprintf(staleProject, sizeof(staleProject), "%s/stale-project.cfg", directory);
    snprintf(staleLocal, sizeof(staleLocal), "%s/stale.local.cfg", directory);
    snprintf(staleProfile, sizeof(staleProfile), "%s/stale.profile.cfg", directory);
    CHECK(WriteText(staleProject, "format_version 3\ngameplay.move_speed 11\n"),
          "could not create stale project fixture");
    SetPaths(staleProject, staleLocal, staleProfile, absent);
    App tolerant = { 0 };
    CHECK(ConfigInitialize(&tolerant),
          "stale project file dropped runtime startup to recovery defaults");
    CHECK(!tolerant.config.recoveryDefaults && tolerant.config.projectLoaded,
          "tolerant load was not marked as a loaded project");
    CHECK(tolerant.tune.moveSpeed == 11.0f,
          "tolerant load ignored a present project key");
    CHECK(tolerant.tune.playerRespawn > 0.0f,
          "tolerant load left a missing key without its compiled default");
    CHECK(strstr(ConfigStatus(&tolerant), "defaulted") != NULL,
          "tolerant load did not surface the defaulted-keys notice");

    // A legacy file with an out-of-range profile value must be rejected instead of
    // handing the menu an unindexable selected kit.
    char badLegacy[512], badLocal[512], badProfile[512];
    snprintf(badLegacy, sizeof(badLegacy), "%s/bad-legacy.cfg", directory);
    snprintf(badLocal, sizeof(badLocal), "%s/bad.local.cfg", directory);
    snprintf(badProfile, sizeof(badProfile), "%s/bad.profile.cfg", directory);
    CHECK(WriteText(badLegacy, "version 2\nselected_kit 7\n"),
          "could not create out-of-range legacy fixture");
    SetPaths(project, badLocal, badProfile, badLegacy);
    App guarded = { 0 };
    CHECK(ConfigInitialize(&guarded),
          "out-of-range legacy file broke canonical startup");
    CHECK(!guarded.config.legacyImported,
          "out-of-range legacy file was imported anyway");
    CHECK(guarded.tune.selectedKit >= 0 && guarded.tune.selectedKit < CLASS_COUNT,
          "selected kit escaped its valid range");

    puts("configuration v3 layering, promotion, rejection, profile, and migrations passed");
    return 0;
}
