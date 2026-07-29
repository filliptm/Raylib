# Brawl Arena project configuration

`gameplay.cfg` is the tracked, canonical source for Brawl Arena's designer-facing
gameplay and presentation values. A clean checkout loads this file before constructing
the first match.

The runtime layers are:

```text
compiled recovery values
        ↓
config/gameplay.cfg       tracked project truth
        ↓
tuning.local.cfg          ignored authoring draft
        ↓
profile.cfg               ignored profile values only
```

Compiled values exist so a malformed installation can show a configuration error, not
as an alternate normal source of defaults. The command center displays
`CONFIG RECOVERY MODE` if the canonical file is missing or invalid.

## Authoring workflow

Command-center sliders take effect immediately and autosave to `tuning.local.cfg` after
0.6 seconds. Project-scoped values are written there only while they differ from
`gameplay.cfg`.

Use:

- `SAVE KIT + SHOWCASE AS PROJECT DEFAULT` on KIT or PREVIEW / UI to promote the active
  kit and the shared menu showcase.
- `SAVE ALL AS PROJECT DEFAULTS` on the WORLD tab to promote every live project-scoped
  value.
- `Reset kit + showcase to PROJECT` to discard the active kit/showcase draft.
- `Discard draft / restore PROJECT` to restore every project value.

Promotion validates the full candidate and atomically rewrites `gameplay.cfg`. This
creates a normal tracked Git modification; it does not automatically create a Git
commit.

`profile.cfg` owns the selected kit, win/loss/KO statistics, UI scale, reduced motion,
high-contrast combat cues, tutorial visibility/completion, and input-glyph preference.
Cheats and the debug overlay are local-only values. Neither category is written into
project defaults.

## Format and validation

The canonical format is version 3 deterministic `key value` text with stable kit
identifiers. Main/super kinds include `projectile`, `lob`, `rain`, `dash`,
`sound_wave`, and `returning`; secondary kinds are `none`, `dash`, `shield`,
`grapple`, or `mine`.
Global out-of-combat recovery is authored with
`gameplay.health_regen_delay`, `gameplay.health_regen_interval`, and
`gameplay.health_regen_max_ratio`; a zero ratio disables passive regeneration.
`presentation.match_camera_distance` is the project-scoped 20–60-unit distance from the
match camera to its smoothed player/aim focus. It scales the established camera offset
without changing its pitch.
Each kit declares `main.self_heal_ratio`, `main.return_speed`, a complete
`secondary.*` block, and returning-super speed/pull/knockback fields. The shared
secondary record includes cooldown/duration/speed plus `range`, `delay`, `radius`,
`trigger_radius`, `damage`, and `knockback`; fields unused by a behavior remain zero.
Behavior-specific
validation requires one disc and positive outbound/return speeds for returning attacks;
positive cooldown/duration/speed for dash secondaries; and positive capacity, movement
multiplier, recharge delay/rate, and break lockout plus a bounded healing ratio for
shield secondaries. Grapples require positive cooldown, pull duration, range, and
non-negative launch delay. Mines require positive cooldown, arm delay, trigger/blast
radii, damage, and non-negative knockback.

One eleven-key `preview.showcase.*` record frames every non-rotating menu model:

- `yaw_degrees`, `scale`, `offset_x`, `offset_y`
- `camera_position_x`, `camera_position_y`, `camera_position_z`
- `camera_target_x`, `camera_target_y`, `camera_target_z`
- `vertical_fov`

The shared project-scoped record is validated with the same all-or-nothing transaction
as gameplay. Profile-only keys are `profile.ui_scale`,
`profile.reduced_motion`, `profile.high_contrast`, `profile.tutorial_hints`,
`profile.input_glyph_mode`, and `profile.tutorial_flags`.

The loader rejects:

- Missing or duplicate canonical keys.
- Unknown canonical keys.
- Invalid types, enum names, NaN, or infinity.
- Values outside the field registry's declared range.
- Incomplete projectile or returning abilities.
- Incomplete or internally inconsistent dash, shield, grapple, or mine secondaries.
- Invalid rain or sound-wave duration/tick relationships.

Loading is transactional, so a rejected local draft cannot partially alter runtime
state.

Validate and test with:

```bash
make -C brawl_arena validate-config
make -C brawl_arena test
```

Test/runtime paths can be isolated with:

```bash
cd brawl_arena
BRAWL_PROJECT_CONFIG=/tmp/gameplay.cfg \
BRAWL_TUNING=/tmp/tuning.local.cfg \
BRAWL_PROFILE=/tmp/profile.cfg \
BRAWL_LEGACY_TUNING=/tmp/legacy.cfg \
./build/brawl_arena
```

## Legacy import

When `tuning.local.cfg` is absent, the normal runtime reads an existing version-1 or
version-2 `tuning.cfg` once and splits it into the new draft/profile files. The original
file is preserved. Version-1 Tank `mobility.*` values become its dash secondary.
Version-1 Scrapper weapon values are ignored so obsolete shotgun/Buckshot tuning cannot
replace Ripsaw, Wrecking Disc, or Magnetic Scrap Shell; Guardian
projectile/Sanctuary values are likewise ignored because their meanings are
incompatible. The shared showcase is derived from Scrapper's old home yaw, scale,
offset, target height, and distance. Typed version-2 files retain compatible kit and
showcase values, translate the old `guard` kind to `shield`, preserve its capacity and
movement multiplier, discard arc/counterblast/timed-hold values, and seed the new Shell
healing/recharge defaults. A later promotion/save always emits version 3.
