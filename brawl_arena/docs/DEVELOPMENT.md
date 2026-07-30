# Brawl Arena development guide

Last code-verified: 2026-07-29

## Build targets

```bash
make                    # architecture check + optimized game
make run                # build and launch
make debug              # clean debug build
make check-architecture # dependency-policy check
make check-ui           # Arena Ink ownership/font/procedural-resource policy check
make ui-assets          # report the procedural UI asset policy
make character-assets   # bake mesh library + shared animations into runtime GLBs
make check-character-assets # validate rigs, clips, outputs, and 1K textures
make validate-config    # validate tracked canonical project values
make test               # Python asset-pipeline tests + seventeen C test executables
make sanitize           # clean sanitizer headless run
make ios-bootstrap      # fetch/verify the pinned raylib-iOS source and apply its patch
make ios-project        # generate the Xcode project
make ios-simulator      # build for the named simulator (default: iPhone 17 Pro)
make ios-device         # signed device build; requires BRAWL_DEVELOPMENT_TEAM
make clean
```

Requires raylib 5.5 or newer. The current macOS build uses `pkg-config` plus OpenGL,
Cocoa, IOKit, and CoreVideo frameworks.

The iPhone target requires Xcode and CMake, targets iOS 15.6+, and uses a pinned
raylib-iOS fork with a tracked compatibility patch. It packages project content into the
app bundle and maps writable player files to Application Support. See
[`../apple/README.md`](../apple/README.md) for bootstrap, signing, install, smoke launch,
and pin-update procedures.

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
| Touch contacts, virtual-stick mapping, safe areas, platform paths | `src/app` |
| Camera, assets, shaders, effects, visuals, environment | `src/presentation` |
| Menu or HUD | `src/ui` |
| Shared comic geometry and decorative motifs | `src/ui/ui_skin.[ch]` |
| Menu stage/preview/sticker compositing | `src/presentation/menu_scene.[ch]` |
| Command-center UI | `src/devtools` |
| Map packages | `data/maps` |
| Character asset manifest | `data/characters/asset_manifest.json` |
| Mesh-only characters and reusable clips | `resources/characters` |
| Character import, retargeting, and validation | `tools/character_pipeline`, `tools/*.py` |
| Canonical numeric defaults | `config/gameplay.cfg` |
| iPhone bridge, bundle metadata, dependency patch, Xcode generator | `apple`, `tools/*ios*.sh` |

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

1. Extend semantic colors and shared control behavior in `ui_theme`/`ui_system`.
2. Put reusable comic geometry—panels, buttons, bars, bursts, halftone, or speed
   lines—in `ui_skin`; screen code should compose those primitives, not fork a second
   style.
3. Keep the black contour, paper keyline, and scale-aware shape contract at every
   supported viewport. Preserve 44-reference-pixel targets and explicit focus rings.
4. Keep decorative density around hero/action moments. Data-rich HUD and developer
   panels use the same pigments with less ornament.
5. Barlow/IBM Plex, code-drawn icons, and live catalog values remain authoritative.
6. If a future UI texture is genuinely necessary, document its source and license under
   `resources/ui/`, load it only through the owning UI/presentation module, and preserve
   a procedural fallback.
7. Update `tools/check_ui_assets.py`, run `make check-ui`, and complete the graphical
   screen/viewport pass.

Mechanic-derived character motifs and impact copy belong to immutable content metadata.
Menu/HUD modules consume the typed style; they do not switch on character IDs.

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
raylib keyboard/mouse/gamepad/touch state → PlayerInput → PlayerUpdate → GameContext API
```

Touch code belongs in the app layer. It may use camera orientation to map a virtual
stick into world movement, but it must emit ordinary `PlayerInput`; deterministic
simulation cannot read platform contacts or UI geometry.

Do not bypass these routes for convenience; the dependency check and headless replay
test exist to keep simulation usable without a window.

## Tests

| Executable | Coverage |
|---|---|
| `test_config` | source layering, validation, save/promotion/reset, profile persistence, migration |
| `test_healer` | typed Guardian content and rain/Resonance timing/outcomes |
| `test_no_attack_shake` | every kit plus impact/damage/death camera isolation |
| `test_arena` | catalog, two map packages, runtime dimensions/cover/reachability, circle clearance, swept wall/crate collision, and wall sliding |
| `test_gameplay` | identical input replay, simulation event/presentation isolation, slow-frame velocity clamping, ally/enemy actor pass-through, and an integrated Helios-9 bot route |
| `test_regeneration` | max-health recovery delay/cadence, combat resets, symmetry, caps, and disable state |
| `test_tank` | actual-damage self-heal, snapshotting, mobility timing/collision, and Charge regression |
| `test_longshot` | twin-shot count, combined fallback damage/charge, tight parallel spacing, centered trajectory, and both-bolt hit behavior |
| `test_scrapper` | Ripsaw/Wrecking Disc legs, cover, ownership, Shell absorption/healing/recharge/break/rearm, and Fight-bot prediction/release |
| `test_secondaries` | Longshot Grapple timing/action lock/cover/cooldown/displacement cancellation and Mortar Mine arming/team/damage/knockback/replacement/line-of-sight/cleanup rules |
| `test_ui` | viewport/safe-area layouts, 44-point targets, focus, IDs, easing/reduced motion, character motifs, result actions, contrast, procedural-skin lifetime, mobile-control placement/camera mapping/touch language, and the shared showcase |
| `test_character_animation` | match clip direction/rate/death selection, stationary-fire isolation from bush reveal, and explicit main/Shell/Grapple/Mine action contracts and blend timing |
| `test_vfx` | recipe catalog validation, flipbook timing, priority eviction, and reduced-motion behavior |
| `test_vfx_events` | all-kit cast/action mappings, rig socket attachment, Scrapper saw/Shell, Longshot Grapple, Mortar Mine, Tank reclaim/jets, and Guardian rain feedback |
| `test_camera` | live match-camera distance, original default framing, fixed pitch, follow, and aim-lead separation |
| `test_character_pipeline.py` | rig rejection, merged-source rest-pose fallback, bind-relative math, deterministic baking, canonical clips, raylib-safe mesh indices, and 1K model/output validation |
| `test_vfx_pipeline.py` | deterministic atlas generation, source provenance, and manifest validation |

Tests use path overrides and temporary fixtures. They must not touch the developer's
ignored local tuning/profile files.

## Interactive smoke checklist

Use [UI_SMOKE_CHECKLIST.md](UI_SMOKE_CHECKLIST.md) for the complete viewport/input
matrix. At minimum, after changes to runtime/presentation:

- Main menu, Brawlers, Practice, Play, Escape/back, and fade transitions.
- All five kit previews and both main/ultimate firing.
- Shared home/roster showcase framing while rapidly changing candidates.
- Scrapper Ripsaw/Wrecking Disc two-leg hits, cover interaction, held Shell,
  absorb healing/recharge/break recovery, and Fight-bot raise/lower timing.
- Tank Reclamation healing, Shift Shoulder Jets cooldown/cover stop, and Charge.
- Longshot Mag-Line Grapple hold/release input, exact range/path/endpoint preview,
  invalid and cover-limited colors, traveling hook tip, cover stop, cable/pose phases,
  cooldown, and external-displacement cancellation.
- Mortar Concussion Mine placement, arm cue, trigger/blast rings, team/cover rules,
  replacement, detonation, and Fight-bot use.
- Guardian rain growth/pulses and Resonance ally/enemy statuses.
- Three-second out-of-combat regeneration, one-second pulses, and attack/damage resets.
- Static, Roam, and Fight bots.
- Gem spawn, pickup, death drop, countdown stamp/pulse, and all three result actions.
- Command-center sliders, reset, project promotion, scrolling, and pointer capture.
- Keyboard/gamepad menu focus, modal close/restore, and glyph switching.
- Arena Ink controls, per-character motifs, rounded sticker contour, entrance motion,
  and raw-preview fallback if sticker resources fail.
- Resize at 960×600, 1280×800, 1920×1080, and 2560×1440.
- Helios-9 and Training Court selection/rebuild.
- Imported models, animation direction, grass, station props, shaders, post effects.
- Confirm attacks never shake either user's camera.
- On separate fresh launches, confirm Play and Practice each begin with the command
  center closed, `TAB` opens it, and the WORLD match-camera slider changes framing live
  without changing pitch or clipping the arena at either endpoint.
- In the iPhone simulator, check the landscape launch deck and a direct match smoke
  launch (`BRAWL_IOS_SMOKE_MATCH=1`) at a notched safe area. Confirm the desktop-only
  Studio, Quit, and command center controls are absent, the backdrop fills the display,
  imported CPU-skinned characters are stable, and every touch control stays clear of
  the objective and pause.
- On a physical iPhone, install and launch the signed bundle, then exercise simultaneous
  move/aim, tap auto-aim, attack release, Super release, each kit's Skill interaction,
  pause/background/resume, rotation lock, and sustained-play responsiveness.

Report when the graphical checklist was not run; passing headless tests does not compile
GPU shaders or validate visual alignment.

## Documentation obligation

Update project-local docs and root `docs/PROJECT_OVERVIEW.md` when structure, ownership,
commands, controls, content formats, gameplay, assets, or verification changes. `AGENTS.md`
directs future coding agents to these files and requires documentation to change with the
code.
