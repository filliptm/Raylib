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

- `SAVE KIT + FRAMING AS PROJECT DEFAULT` on KIT or PREVIEW / UI to promote the active
  kit and its menu presentation profile.
- `SAVE ALL AS PROJECT DEFAULTS` on the WORLD tab to promote every live project-scoped
  value.
- `Reset kit to PROJECT default` to discard the active kit's draft.
- `Discard draft / restore PROJECT` to restore every project value.

Promotion validates the full candidate and atomically rewrites `gameplay.cfg`. This
creates a normal tracked Git modification; it does not automatically create a Git
commit.

`profile.cfg` owns the selected kit, win/loss/KO statistics, UI scale, reduced motion,
high-contrast combat cues, tutorial visibility/completion, and input-glyph preference.
Cheats and the debug overlay are local-only values. Neither category is written into
project defaults.

## Format and validation

The format is deterministic `key value` text with stable kit identifiers. Ability kinds
are named values such as `projectile`, `lob`, `rain`, `dash`, and `sound_wave`.
Global out-of-combat recovery is authored with
`gameplay.health_regen_delay`, `gameplay.health_regen_interval`, and
`gameplay.health_regen_max_ratio`; a zero ratio disables passive regeneration.
Each kit also declares `main.self_heal_ratio` and the complete optional mobility triplet:
`mobility.cooldown`, `mobility.duration`, and `mobility.speed`. All three mobility values
must be zero to disable the ability, or all three must be positive. Tank's tracked
mobility values create Shoulder Jets; other kits currently keep the triplet at zero.

Every character also requires ten `preview.<stable-kit-id>.*` keys:

- `home_yaw_degrees`, `select_yaw_degrees`
- `home_scale`, `select_scale`
- `home_offset_x`, `home_offset_y`
- `select_offset_x`, `select_offset_y`
- `camera_target_y`, `camera_distance`

These project-scoped values frame the non-rotating menu model and are validated with the
same all-or-nothing transaction as gameplay. Profile-only keys are `profile.ui_scale`,
`profile.reduced_motion`, `profile.high_contrast`, `profile.tutorial_hints`,
`profile.input_glyph_mode`, and `profile.tutorial_flags`.

The loader rejects:

- Missing or duplicate canonical keys.
- Unknown canonical keys.
- Invalid types, enum names, NaN, or infinity.
- Values outside the field registry's declared range.
- Incomplete projectile abilities.
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
file is preserved. Version-1 Guardian projectile/Sanctuary values are not applied to the
new rain/Resonance kit because their meanings are incompatible.
