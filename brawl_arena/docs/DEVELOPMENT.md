# Brawl Arena development guide

Last code-verified: 2026-07-27

## Build targets

```bash
make                    # architecture check + optimized game
make run                # build and launch
make debug              # clean debug build
make check-architecture # dependency-policy check
make check-ui           # Helios UI ownership/font/asset policy check
make ui-assets          # rebuild tracked tintable UI motifs from retained sources
make character-assets   # bake mesh library + shared animations into runtime GLBs
make check-character-assets # validate rigs, clips, outputs, and 1K textures
make validate-config    # validate tracked canonical project values
make test               # Python asset-pipeline tests + eleven C test executables
make sanitize           # clean sanitizer headless run
make clean
```

Requires raylib 5.5 or newer. The current macOS build uses `pkg-config` plus OpenGL,
Cocoa, IOKit, and CoreVideo frameworks.

On non-Darwin platforms the sanitizer target uses ASan and UBSan. The current Apple
clang/macOS 26 ASan runtime deadlocks during its own process initialization, so Darwin
uses strict UBSan (`halt_on_error=1`) until that toolchain issue is resolved.

The Makefile recursively discovers `src/**/*.c`, mirrors subsystem directories below
`build/obj/`, and emits `.d` dependency files. Adding a normal C module does not require
updating the application source list. A test with a deliberately narrow link set may
require adding its object to that target.

## Where changes belong

| Work | Primary location |
|---|---|
| Limits, IDs, deterministic utility | `src/core` |
| Designer values, character/ability definitions, map/config parsing | `src/content` |
| Match rules, AI, actors, weapons, statuses, objective | `src/game` |
| Main loop, local controller/input, authoring commands | `src/app` |
| Camera, assets, shaders, effects, visuals, environment | `src/presentation` |
| Menu or HUD | `src/ui` |
| Shared UI textures, slicing, and decorative motifs | `src/ui/ui_skin.[ch]`, `resources/ui` |
| Menu hangar/podium/preview scene | `src/presentation/menu_scene.[ch]` |
| Command-center UI | `src/devtools` |
| Map packages | `data/maps` |
| Character asset manifest | `data/characters/asset_manifest.json` |
| Mesh-only characters and reusable clips | `resources/characters` |
| Character import, retargeting, and validation | `tools/character_pipeline`, `tools/*.py` |
| Canonical numeric defaults | `config/gameplay.cfg` |

Read [ARCHITECTURE.md](ARCHITECTURE.md) before moving APIs between layers.

## Adding reusable gameplay

For a new attack/status behavior:

1. Add the smallest behavior tag and typed payload needed in content types.
2. Validate its field relationships in the config/content loader.
3. Implement deterministic simulation in `src/game`.
4. Emit events for presentation; do not call effects or draw.
5. Add aim/field visuals in `src/presentation/ability_visuals.c`.
6. Teach summaries/authoring UI only about the generic behavior, not a character name.
7. Add a headless behavior test and deterministic replay coverage.
8. Run architecture, config, normal, and sanitizer checks.

Use generic `StatusEffect` slots for periodic team-aware outcomes. Do not add
character-named timers/marks to `Brawler`.

## Adding or changing a character

Current character slots are fixed:

1. Extend `BrawlerClass` and capacity-derived arrays.
2. Add stable ID, model ID, and role metadata in `content_catalog.c`.
3. Add compiled recovery authoring values and complete canonical keys.
4. Prefer existing ability behaviors; add a new behavior only when the rule is genuinely
   different.
5. Import the rigged Meshy `Character_output` ZIP/directory/GLB or a compatible
   standalone merged-animation GLB using
   [CHARACTER_PIPELINE.md](CHARACTER_PIPELINE.md). The standard workflow needs only the
   rigged model; `import_character.py` repairs raylib's bind representation, records its
   animation rest pose, strips clips, enforces 1K textures, and partitions dense triangle
   meshes into raylib-safe 16-bit indexed primitives.
6. Add the model and generated output to `data/characters/asset_manifest.json`, then
   point `CHARACTER_MODEL_PATHS` at the generated `build/assets/characters/` file.
7. Run `make character-assets`. The build retargets the shared animation library by bone
   name and bind-relative motion. Add an animation-only override only when the character
   intentionally needs a unique clip.
8. Add tests for behavior, tuning validation, no combat shake, and summaries.

raylib still does not retarget rigs at runtime. The Python build pipeline performs the
retarget and emits self-contained runtime GLBs before the game starts.

## Adding a map

Follow [MAPS.md](MAPS.md). Keep collision (`terrain.layer`), match markers
(`gameplay.layer`), visual hints (`visual.layer`), and free props (`props.cfg`) separate.
Every catalog entry is loaded at startup, so one invalid map rejects the catalog.

## Adding or changing UI art

Keep world and UI resource ownership separate:

1. Put curated, runtime-loaded interface art under `resources/ui/<pack>/runtime/`.
2. Retain only the source files needed to reproduce it under the adjacent `source/`.
3. Add `LICENSE.txt` and `SOURCE.md` with the upstream URL, version/author, license,
   archive hash, included subset, and intentional exclusions.
4. Generate derivatives through `tools/build_ui_assets.py`; do not hand-edit tracked
   runtime crops.
5. Load/unload textures only in `ui_skin.c`. Screen code calls shared panel, control,
   progress, or decoration APIs and must preserve a geometry fallback.
6. Do not import pack fonts, fake telemetry, or decorative charts into player data
   surfaces. Barlow/IBM Plex and live catalog values remain authoritative.
7. Update `tools/check_ui_assets.py`, run `make ui-assets`, then `make check-ui`.

## Event and command discipline

Simulation-to-presentation:

```text
game mutation → GameEventQueue → FxConsumeGameEvents → PresentationState
```

Developer/UI-to-simulation:

```text
widget action → GameCommand → app command handler → GameContext API
```

Captured player input:

```text
raylib state → PlayerInput → PlayerUpdate → GameContext API
```

Do not bypass these routes for convenience; the dependency check and headless replay
test exist to keep simulation usable without a window.

## Tests

| Executable | Coverage |
|---|---|
| `test_config` | source layering, validation, save/promotion/reset, profile, migration |
| `test_healer` | typed Guardian content and rain/Resonance timing/outcomes |
| `test_no_attack_shake` | every kit plus impact/damage/death camera isolation |
| `test_arena` | catalog, two map packages, runtime dimensions/cover/reachability |
| `test_gameplay` | identical input replay and simulation event/presentation isolation |
| `test_regeneration` | max-health recovery delay/cadence, combat resets, symmetry, caps, and disable state |
| `test_tank` | actual-damage self-heal, snapshotting, mobility timing/collision, and Charge regression |
| `test_ui` | four viewport layouts, minimum targets, focus, IDs, motion, contrast, nine-slice metadata, and presentation profiles |
| `test_character_animation` | match clip direction/rate/death selection, stationary-fire isolation from bush reveal, and explicit action blend timing |
| `test_vfx` | recipe catalog validation, flipbook timing, priority eviction, and reduced-motion behavior |
| `test_vfx_events` | all-kit cast/action mappings, rig socket attachment, Tank reclaim/jets, and Guardian rain feedback |
| `test_character_pipeline.py` | rig rejection, merged-source rest-pose fallback, bind-relative math, deterministic baking, canonical clips, raylib-safe mesh indices, and 1K model/output validation |
| `test_vfx_pipeline.py` | deterministic atlas generation, source provenance, and manifest validation |

Tests use path overrides and temporary fixtures. They must not touch the developer's
ignored local tuning/profile files.

## Interactive smoke checklist

Use [UI_SMOKE_CHECKLIST.md](UI_SMOKE_CHECKLIST.md) for the complete viewport/input
matrix. At minimum, after changes to runtime/presentation:

- Main menu, Brawlers, Practice, Play, Escape/back, and fade transitions.
- All five kit previews and both main/ultimate firing.
- Tank Reclamation healing, Shift Shoulder Jets cooldown/cover stop, and Charge.
- Guardian rain growth/pulses and Resonance ally/enemy statuses.
- Three-second out-of-combat regeneration, one-second pulses, and attack/damage resets.
- Static, Roam, and Fight bots.
- Gem spawn, pickup, death drop, countdown, win/result return.
- Command-center sliders, reset, project promotion, scrolling, and pointer capture.
- Keyboard/gamepad menu focus, modal close/restore, and glyph switching.
- Kenney control surfaces, orbital/radar staging, and forced UI-texture fallbacks.
- Resize at 960×600, 1280×800, 1920×1080, and 2560×1440.
- Helios-9 and Training Court selection/rebuild.
- Imported models, animation direction, grass, station props, shaders, post effects.
- Confirm attacks never shake either user's camera.

Report when the graphical checklist was not run; passing headless tests does not compile
GPU shaders or validate visual alignment.

## Documentation obligation

Update project-local docs and root `docs/PROJECT_OVERVIEW.md` when structure, ownership,
commands, controls, content formats, gameplay, assets, or verification changes. `AGENTS.md`
directs future coding agents to these files and requires documentation to change with the
code.
