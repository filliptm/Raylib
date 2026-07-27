# Brawl Arena content and tuning

Last code-verified: 2026-07-27

This document explains which values are authoritative, how live tuning becomes a tracked
change, and how authoring records become typed runtime content.

## Sources and precedence

```text
WEAPON_DEFAULTS + TuningSetDefaults
        ↓ recovery only
config/gameplay.cfg
        ↓ tracked canonical project truth
tuning.local.cfg
        ↓ ignored sparse developer draft
profile.cfg
        ↓ ignored player/profile state
effective Tuning + ContentCatalog
```

Compiled values make a broken installation diagnosable and support isolated validation.
They are not a second normal source of project truth.

Startup transactionally loads the complete tracked project file. A missing, duplicate,
unknown, mistyped, non-finite, out-of-range, or behavior-inconsistent value rejects the
candidate without partially modifying live state. The command center displays
`CONFIG RECOVERY MODE` when the canonical file cannot be used.

## Live authoring and save actions

Command-center sliders edit `App.tune` and `App.content.weapons` immediately. The typed
catalog is rebuilt after the panel processes edits, so gameplay and UI read the current
values in the same session.

After 0.6 seconds of inactivity:

- Project-scoped values that differ from `config/gameplay.cfg` are written to the sparse
  ignored `tuning.local.cfg`.
- Local-only cheats/debug state is written there regardless of project equality.
- Selected kit and win/loss/KO statistics are written to ignored `profile.cfg`.

Explicit actions:

- `SAVE KIT AS PROJECT DEFAULT` copies the active kit into the canonical snapshot,
  validates the complete result, and atomically rewrites `config/gameplay.cfg`.
- `SAVE ALL AS PROJECT DEFAULTS` promotes every live project-scoped value.
- `Reset kit to PROJECT default` restores one kit from the loaded canonical snapshot.
- `Discard draft / restore PROJECT` restores all project-scoped live values.

Promotion writes a temporary file, flushes/closes it, then renames it over the tracked
file. It produces a Git working-tree modification but never invokes Git or creates a
commit.

Do not delete or overwrite a developer's `tuning.cfg`, `tuning.local.cfg`, or
`profile.cfg` during tests. Override paths instead:

```bash
BRAWL_PROJECT_CONFIG=/tmp/brawl-project.cfg \
BRAWL_TUNING=/tmp/brawl-local.cfg \
BRAWL_PROFILE=/tmp/brawl-profile.cfg \
BRAWL_LEGACY_TUNING=/tmp/brawl-legacy.cfg \
./build/brawl_arena
```

## Legacy migration

When `tuning.local.cfg` is absent, an existing version-1 or version-2 `tuning.cfg` may be
imported once. Compatible values are split into the new local draft/profile files and the
original remains untouched. Version-1 Guardian bolt/Sanctuary values are ignored because
they do not describe the rain/Resonance behavior.

## Runtime catalog

`ContentCatalog` contains:

- `weapons[CLASS_COUNT]`: compatibility authoring records used by the config schema and
  command-center Kit tab.
- `characters[CLASS_COUNT]`: typed character identity, display, role, model, health,
  ammo, and ability handles.
- `abilities[MAX_ABILITIES]`: typed main/ultimate definitions plus optional mobility
  definitions.
- `maps[MAX_MAPS]`: validated map definitions and selected-map index.

Each runtime ability has:

- Stable ID and display name.
- `AbilityBehavior` tag.
- Range, radius, damage, healing, cooldown/reload, and ultimate-gain fields.
- An optional projectile self-heal ratio based on actual health removed.
- Exactly one relevant typed payload: projectile, area, or dash.

Implemented reusable behaviors are projectile, lob, rain, dash, healing burst, and sound
wave. Game, AI, menu summaries, HUD, and aim previews resolve abilities through
`ContentMainAbility()`, `ContentSuperAbility()`, and
`ContentMobilityAbility()`. The current catalog has eleven active definitions in a
fifteen-slot fixed array.

## Tank

Tank is the mobile sustain character:

- Stable ID: `tank`.
- Role: tank.
- Model asset ID: `tank` (the Ironclad Guardian runtime model).
- Main behavior: four-projectile Reclamation Rounds.
- Optional mobility behavior: Shoulder Jets.
- Ultimate behavior: Charge.

Each Reclamation Rounds projectile snapshots `main.self_heal_ratio` when fired. On an
enemy hit it restores that fraction of health actually removed, rounded to the nearest
whole health point. This prevents overhealing, overkill farming, live-retuning of
in-flight shots, crate healing, and Charge healing.

Shoulder Jets uses `mobility.cooldown`, `mobility.duration`, and `mobility.speed`. The
tracked defaults are 2.5 seconds, 0.18 seconds, and 22 world units per second. It stops on
solid cover and carries zero damage, knockback, and crate-breaking capability. Charge
uses the same reusable dash executor with its own damage, knockback, duration, global
Charge speed, and crate-breaking policy.

The compatibility authoring record is a deliberate migration seam. A future fully
external character manifest can populate the typed catalog directly; until then, adding
a sixth character still requires extending `BrawlerClass`, fixed ID/model/role arrays,
compiled recovery values, and canonical config keys.

## Guardian

Guardian is the support character:

- Stable ID: `guardian`.
- Role: support.
- Model asset ID: `gaia_guardian`.
- Main behavior: rain.
- Ultimate behavior: sound wave.

Rain creates a short-lived field at the clamped aim landing point. The radius grows over
`growTime`; every `tickRate` it damages enemies and restores missing health on allies.
Useful damage/healing grants ultimate charge.

Resonance checks a large cone and line of sight at cast time. It applies a generic
team-aware periodic status to each hit target. The same status record heals a target on
the source team and damages a target on the opposing team. The traveling wave
visualization may end before the status duration.

The behavioral regression test derives expected pulse counts from the content values
instead of duplicating them as implementation constants.

## Validation commands

```bash
make validate-config
make test
```

Use the command center for authoring. Edit `config/gameplay.cfg` by hand only when you
also run validation and understand that the file must contain every project-scoped key.

The lower-level field format and rejection rules are also summarized in
`config/README.md`.
