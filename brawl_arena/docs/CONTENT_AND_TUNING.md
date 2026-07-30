# Brawl Arena content and tuning

Last code-verified: 2026-07-29

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

Command-center sliders edit `App.tune`, `App.content.weapons`, the shared character
showcase, and personal UI preferences immediately. The typed catalog is rebuilt only
when an active weapon authoring record actually changes, so gameplay and UI read current
ability values without redundant per-frame rebuilds.

The WORLD tab's `presentation.match_camera_distance` is project-scoped and applies live.
Its 20–60-unit range scales the complete follow-camera offset, preserving the established
pitch, aim lead, and smoothing. The tracked project value is 31.999340 units. The
compiled 38.013156 recovery value reproduces the original `{0, 31, -22}` offset. The
iPhone runtime leaves the project value unchanged and uses an effective distance equal
to 80% of it, clamped to 20 units, for phone-readable framing.

The VISUAL tab's `presentation.render_scale` is also project-scoped and applies after a
short target-recreation debounce. Its 1.0×–2.0× range scales the world color/depth
target from the drawable framebuffer independently of logical UI coordinates. The
tracked 1.5× default leaves authored screen-space post-effect sizes unchanged and keeps
the intended sampling ratio on platforms where drawable and logical dimensions differ.
The iPhone runtime does not rewrite or fork this project value. It applies a platform
policy that caps the effective scale at 1.0× and disables post-processing while the
authored 1.5× value remains available to desktop and future mobile profiling.

After 0.6 seconds of inactivity:

- Project-scoped values that differ from `config/gameplay.cfg` are written to the sparse
  ignored `tuning.local.cfg`.
- Local-only cheats/debug state is written there regardless of project equality.
- Selected kit, win/loss/KO statistics, UI preferences, and completed tutorial actions
  are written to ignored `profile.cfg`.

Explicit actions:

- `SAVE KIT + SHOWCASE AS PROJECT DEFAULT` copies the active kit and shared showcase
  into the canonical snapshot, validates the complete result, and atomically
  rewrites `config/gameplay.cfg`.
- `SAVE ALL AS PROJECT DEFAULTS` promotes every live project-scoped value.
- `Reset kit + showcase to PROJECT` restores one kit and the shared showcase from the
  loaded canonical snapshot.
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

On iPhone, `config/gameplay.cfg` is read from the signed app's bundled `BrawlAssets`
directory. The ignored draft, profile, and legacy-import paths live under the app's
Application Support directory, where ordinary autosave remains writable. Environment
path overrides still take precedence when explicitly supplied for a development smoke
run.

## Legacy migration

When `tuning.local.cfg` is absent, an existing version-1 or version-2 `tuning.cfg` may be
imported once. Compatible values are split into the new local draft/profile files and the
original remains untouched. Version-1 Tank mobility becomes a dash secondary, while
obsolete Scrapper shotgun/Buckshot and Guardian bolt/Sanctuary values are ignored. The
shared showcase is derived from the old Scrapper home profile. A later save emits the
complete version-3 format. Typed version-2 files migrate `guard` to `shield`, retain
capacity/movement values, discard the obsolete arc/timed-counter fields, and seed the
new healing and recharge fields from compiled recovery values.

## Runtime catalog

`ContentCatalog` contains:

- `weapons[CLASS_COUNT]`: compatibility authoring records used by the config schema and
  command-center Kit tab.
- `characters[CLASS_COUNT]`: typed character identity, display, role, model, health,
  ammo, and ability handles.
- `abilities[MAX_ABILITIES]`: typed main/ultimate definitions plus optional secondary
  definitions.
- `maps[MAX_MAPS]`: validated map definitions and selected-map index.
- `showcase`: one project-owned model transform and camera used by every character and
  menu screen.

Each runtime ability has:

- Stable ID and display name.
- `AbilityBehavior` tag.
- Range, radius, damage, healing, cooldown/reload, and ultimate-gain fields.
- An optional projectile self-heal ratio based on actual health removed.
- Exactly one relevant typed payload: projectile, area, dash, returning, shield,
  grapple, or mine.

Implemented reusable behaviors are projectile, lob, rain, dash, healing burst, sound
wave, returning disc, shield, grapple, and mine. Game, AI, menu summaries, HUD, and aim previews resolve
abilities through
`ContentMainAbility()`, `ContentSuperAbility()`, and
`ContentSecondaryAbility()`. The current catalog has fourteen active definitions in a
fifteen-slot fixed array.

## Global combat recovery

Out-of-combat regeneration is project-scoped global tuning rather than character
content:

- `gameplay.health_regen_delay`: quiet time after combat before the first pulse.
- `gameplay.health_regen_interval`: time between later pulses.
- `gameplay.health_regen_max_ratio`: fraction of each brawler's maximum health restored
  per pulse; zero disables the mechanic.

Tracked defaults are 3.0 seconds, 1.0 second, and 0.13. The rule applies symmetrically to
every living brawler. Successful main attacks and ultimates reset combat time; attacks
that fail validation do not. Actual health loss resets it, including periodic damage.
Movement, aiming, optional non-attacking secondaries, and received healing do not. Recovery
uses the normal capped healing API and cannot revive.

These values are editable under WORLD / COMBAT RECOVERY and participate in the normal
sparse-draft and Save All promotion flow.

## Tank

Tank is the mobile sustain character:

- Stable ID: `tank`.
- Role: tank.
- Model asset ID: `tank` (the Ironclad Guardian runtime model).
- Main behavior: six-projectile Reclamation Rounds in the tracked project config.
- Secondary behavior: Shoulder Jets.
- Ultimate behavior: Charge.

Each Reclamation Rounds projectile snapshots `main.self_heal_ratio` when fired. On an
enemy hit it restores that fraction of health actually removed, rounded to the nearest
whole health point. This prevents overhealing, overkill farming, live-retuning of
in-flight shots, crate healing, and Charge healing.

Shoulder Jets uses `secondary.cooldown`, `secondary.duration`, and `secondary.speed`. The
tracked defaults are 2.5 seconds, 0.18 seconds, and 22 world units per second. It stops on
solid cover and carries zero damage, knockback, and crate-breaking capability. Charge
uses the same reusable dash executor with its own damage, knockback, duration, global
Charge speed, and crate-breaking policy.

## Longshot and Mortar secondaries

Longshot's typed Grapple reads `secondary.cooldown`, `secondary.duration` (pull time),
`secondary.range`, and `secondary.delay` (launch time). Its tracked values are 7.5,
0.45, 10.0, and 0.15. The executor derives a body-safe endpoint with the normal arena
circle sweep and locks main/super actions until traversal ends or external displacement
cancels it.

Mortar's typed Mine reads `secondary.cooldown`, `secondary.delay` (arm time),
`secondary.trigger_radius`, `secondary.radius` (blast), `secondary.damage`, and
`secondary.knockback`. Tracked values are 8.0, 0.55, 2.4, 3.2, 400, and 4.5. It is a
persistent fixed-pool ability field with one active instance per owner; detection and
blast both use team and arena line-of-sight rules.

The compatibility authoring record is a deliberate migration seam. A future fully
external character manifest can populate the typed catalog directly; until then, adding
a sixth character still requires extending `BrawlerClass`, fixed ID/model/role arrays,
compiled recovery values, and canonical config keys.

## Scrapper

Scrapper is the returning-disc damage character:

- Stable ID: `scrapper`.
- Role: damage.
- Model asset ID: `scrapper` (the Sentinel runtime model).
- Main behavior: returning Ripsaw.
- Secondary behavior: Magnetic Scrap Shell.
- Ultimate behavior: returning Wrecking Disc.

Ripsaw and Wrecking Disc snapshot their authored values at cast time. A returning
projectile records its owner's class, turns after its outbound range or a solid
collision, clears its per-target hit mask at that transition, and homes toward the
owner's current position. Each target can therefore be hit once outbound and once on
return. Death/class change invalidates the projectile. Ordinary Ripsaw does not damage
crates; Wrecking Disc breaks/passes through crates, pulls outbound, and knocks back on
return.

Magnetic Scrap Shell is a held 360-degree defense. It slows movement and disables main
and ultimate attacks while raised. The centralized damage gateway spends one charge per
hostile damage point before health, heals Scrapper for the authored fraction of damage
absorbed, and passes only overflow to health. Shield contact still triggers normal
hit-confirm, super-gain, pull, and knockback behavior, while health-damage-only sustain
uses only overflow.

Releasing preserves partial charge. Damage resets the authored recharge delay; once
quiet and lowered, charge refills continuously at the authored rate. A full break forces
the shield down for its lockout, then restores it to capacity. The release-to-rearm latch
prevents a held control from raising it immediately after restoration. The tracked
defaults are 1,200 capacity, 65% movement, 30% absorb healing, a three-second recharge
delay, 300 charge per second, and a five-second break lockout.

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

## Presentation and player UI state

One complete tracked `preview.showcase.*` record owns yaw, scale, two-dimensional model
offset, camera position/target, and vertical FOV for every character on both home and
roster screens. It loads from `config/gameplay.cfg`, can be drafted locally, and is
promoted with a kit or Save All. The tracked framing is 180° yaw, 0.90 scale, zero
offset, camera `(0, 2.7, -7.6)`, target `(0, 1.4, 0)`, and 40° vertical FOV.
Candidate changes do not reset the independently advancing comic-stage clock.

Personal presentation choices are never project truth. `profile.ui_scale`,
`profile.reduced_motion`, `profile.high_contrast`, `profile.tutorial_hints`,
`profile.input_glyph_mode`, and `profile.tutorial_flags` live only in the ignored
profile. Settings and the Preview / UI command-center tab edit them immediately.

The match camera is separate from this menu showcase. Its project-owned
`presentation.match_camera_distance` value lives in `Tuning` and is edited on the WORLD
tab. The project-owned `presentation.render_scale` also lives in `Tuning` and is edited
on the VISUAL tab.

## Validation commands

```bash
make validate-config
make test
```

Use the command center for authoring. Edit `config/gameplay.cfg` by hand only when you
also run validation and understand that the file must contain every project-scoped key.

The lower-level field format and rejection rules are also summarized in
`config/README.md`.
