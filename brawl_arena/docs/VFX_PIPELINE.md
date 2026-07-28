# Ability VFX pipeline

Brawl Arena keeps ability art separate from character models and skeletal animation.
Simulation refers to stable `VfxEffectId` values; the presentation layer expands each
ID into a recipe made from animated flipbooks, static particle shapes, procedural
particles, lights, and shockwaves.

The imported art is cosmetic. Aim cones, lob markers, area boundaries, collision,
damage, healing, timing, and visibility continue to come from deterministic gameplay
state.

## Tracked sources and generated outputs

```text
resources/vfx/sources/
        + resources/vfx/licenses/
        + data/vfx/asset_manifest.json
        |
        v  tools/build_vfx_assets.py
build/assets/vfx/*.png
```

The tracked library currently uses curated portions of two CC0 packs:

- Kenney Particle Pack:
  <https://www.kenney.nl/assets/particle-pack>
- RPG VFX Pack by System G6/Qoma:
  <https://opengameart.org/node/146141>

Only the selected PNG sources and notices are tracked. Downloaded ZIP/7z archives are
not source and should not be committed. Paid packs require a separate license review
before either source or derived files can enter the repository.

The build produces seven RGBA atlases totaling about 5.45 MiB:

| Atlas | Grid | Purpose |
|---|---:|---|
| `shapes.png` | 4×2 | muzzle, trace, flame, smoke, spark, magic, scorch, circle |
| `explosion.png` | 8×4 | Mortar impact flipbook |
| `water.png` | 8×4 | Guardian rain cast |
| `energy_loop.png` | 8×4 | rain pulses and Resonance marks |
| `air_burst.png` | 8×4 | rail, charge, and damage impacts |
| `divine_impact.png` | 8×2 | supers, healing, and reclaim arrival |
| `smoke_loop.png` | 8×8 | lingering Mortar smoke |

Character texture policy does not apply to VFX. Character textures remain exactly 1K;
VFX frames are 128, 256, or 512 pixels according to their screen footprint, and no
generated VFX atlas may exceed 2048×2048.

## Commands

Run from `brawl_arena/`:

```sh
make vfx-assets
make check-vfx-assets
python3 tests/test_vfx_pipeline.py
```

`make`, `make test`, and `make sanitize` include the appropriate build and validation
steps. Pillow is required by the atlas builder, as it is by the character pipeline.

The builder:

- Requires every pack to declare an HTTPS source, a tracked notice, and CC0-1.0.
- Converts palette or other supported PNG encodings to runtime RGBA.
- Bleeds nearby color into zero-alpha texels so bilinear filtering does not reveal a
  black source matte around soft particles.
- Validates frame counts, grid geometry, optional per-cell padding, source dimensions,
  and the 2048px limit.
- Packs the eight Kenney shapes in manifest order with an eight-pixel transparent guard;
  the source art is reduced within its existing 512px cell rather than enlarging the
  atlas.
- Re-packs validated flipbook cells without resampling when their padding is zero.
- Writes deterministic PNG output under `build/assets/vfx/`.

Generated files are ignored build products. Do not edit them directly.

## Runtime ownership

The boundary is:

```text
gameplay action
    -> GAME_EVENT_VFX + VfxEffectId + optional actor/socket metadata
    -> presentation/effects.c
    -> fixed VfxInstance pool
    -> recipe in presentation/vfx_catalog.c
    -> presentation/vfx.c render passes
```

`src/core/core_types.h` owns stable IDs because both simulation and presentation need
them. Simulation never imports a presentation header or texture concept.

`src/presentation/vfx_catalog.c` owns recipes. A recipe has up to three layers, and each
layer declares:

- Atlas and frame range.
- Playback rate, delay, duration, and loop behavior.
- Billboard, ground, or start-to-end beam orientation.
- Alpha or additive blending.
- Scale, opacity, rotation, anchor, and event-color policy.

`PresentationState` owns 192 `VfxInstance` slots. No per-frame heap allocation is used.
When saturated, a new layer may replace an older layer of equal or lower priority.
Gameplay telegraphs are not in this pool, so saturation cannot remove targeting or
damage-area information.

Missing atlases log warnings and skip only their imported layers. Existing procedural
muzzle particles, impacts, lights, shockwaves, fields, and aim previews remain active.

Attached events identify a source/target brawler plus `CENTER`, `CHEST`, left/right
hand, left/right shoulder, or left/right foot socket. `render.c` seeds approximate
positions for every brawler, and rigged character drawing replaces mapped sockets from
the final locomotion-plus-action pose. `vfx.c` resolves those positions every frame, so
brief casts, jets, beams, and healing returns stay attached while a character moves.

The WORLD command-center tab exposes the runtime observability used for interactive
checks: loaded atlas count, active/capacity/dropped layers, consumed event and spawned
layer totals, the last recipe, direct recipe spawning, and `MAIN`, `SUPER`, `CAST`, and
`MOBILITY` character-action previews with live action progress/blend readout.

## Current recipe coverage

There are 28 runtime recipes:

- Scrapper main/super cast and impact.
- Longshot main/super cast and impact.
- Mortar main/super cast and impact, including explosion, smoke, and scorch layers.
- Tank main cast/impact, Reclamation path, Shoulder Jets start/trail, and Charge
  start/trail/impact.
- Guardian rain cast/pulse/heal/damage and Resonance cast/heal/damage.
- Shared successful-healing feedback.

Reclamation is emitted only after enemy damage restores actual Tank health. Its event
carries both the hit location and Tank location so the presentation can draw the energy
return path. Shoulder Jets and Charge use separate IDs and colors so the non-damaging
boost never reads like the destructive super.

Persistent Guardian field boundaries remain procedural in `ability_visuals.c`. Imported
art decorates each cast and tick without replacing the true growing rain radius or
Resonance cone.

All imported recipe layers use the shared `VFX_RENDER_SCALE` value of `4.0`. This makes
billboard dimensions, flipbook dimensions, ground-art dimensions, smoke dimensions, and
beam widths four times their catalog-authored size at presentation time without changing
gameplay ranges, collision, field radii, or authoritative aim/area telegraphs.

## Depth and blend rules

World geometry, characters, and projectile bodies write depth before VFX.

- Alpha smoke is sorted back-to-front, depth-tested, and drawn without depth writes.
- Energy layers use additive blending, depth testing, and no depth writes.
- Every depth-write transition goes through `render_state.h`, which flushes raylib's
  active immediate-mode batch before disabling or restoring the OpenGL depth mask.
  Without both flushes, transparent billboard pixels can write a rectangular depth
  silhouette after the mask is restored, and the post-process ink pass outlines that
  invisible quad.
- Runtime frame UVs address half-texel-inset cell interiors, so bilinear filtering never
  samples the neighboring effect. Guarded static cells additionally require a fully
  transparent outer boundary.
- Ground shapes use `ARENA_DECAL_Y` plus separate recipe and per-instance offsets.
- Imported ground art remains below the `ARENA_PREVIEW_Y` telegraph layer.
- Every VFX pass restores texture binding, backface culling, flushed depth-mask state,
  and blend state before returning.

These rules prevent effects from showing through walls and keep overlapping floor art
from fighting with the station deck, inlays, passive decals, or targeting shapes.

## Adding or replacing an effect

1. Confirm redistribution terms. The tracked pipeline currently accepts CC0-1.0 packs.
2. Copy only the selected PNG source into `resources/vfx/sources/<pack>/`.
3. Preserve the pack notice under `resources/vfx/licenses/`.
4. Add the source, grid, frame size, and output to
   `data/vfx/asset_manifest.json`.
5. Run `make vfx-assets` and `make check-vfx-assets`.
6. Add or edit the recipe in `src/presentation/vfx_catalog.c`.
7. Reuse an existing `VfxEffectId` when replacing art. Add a new stable ID only for a
   genuinely new gameplay presentation event.
8. Emit the ID through `GameEmitVfx()` from the exact gameplay outcome.
9. Run `make test`, `make sanitize`, and an interactive Practice/Gem Grab check.

Do not infer hit or healing outcomes by inspecting presentation-side projectiles. Emit
the effect after simulation has resolved the real result.

## Verification

Automated checks cover:

- Deterministic atlas output.
- Manifest geometry, RGBA output, size limits, and license/source metadata.
- Recipe coverage for every stable ID.
- One-shot/looping frame selection and delayed layers.
- Fixed-pool expiry, saturation, and priority replacement.
- Cast IDs for all five kits.
- Tank reclaim, Shoulder Jets trail, Tank impact, and Guardian rain pulse events.

Interactive checks remain required for camera-facing alignment, occlusion, perceived
scale, post-processing, reduced motion, simultaneous 3v3 effects, and floor-depth
behavior.
