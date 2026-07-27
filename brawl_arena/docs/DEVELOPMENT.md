# Brawl Arena development guide

Last code-verified: 2026-07-27

## Build targets

```bash
make                    # architecture check + optimized game
make run                # build and launch
make debug              # clean debug build
make check-architecture # dependency-policy check
make validate-config    # validate tracked canonical project values
make test               # six headless behavior/integration executables
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
| Command-center UI | `src/devtools` |
| Map packages | `data/maps` |
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
5. Import/convert a rigged model using
   [CHARACTER_PIPELINE.md](CHARACTER_PIPELINE.md).
6. Update asset loading/fallback selection.
7. Add tests for behavior, tuning validation, no combat shake, and summaries.

Animations can be reused only for a compatible skeleton/rest pose. raylib does not
retarget arbitrary rigs.

## Adding a map

Follow [MAPS.md](MAPS.md). Keep collision (`terrain.layer`), match markers
(`gameplay.layer`), visual hints (`visual.layer`), and free props (`props.cfg`) separate.
Every catalog entry is loaded at startup, so one invalid map rejects the catalog.

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
| `test_tank` | actual-damage self-heal, snapshotting, mobility timing/collision, and Charge regression |

Tests use path overrides and temporary fixtures. They must not touch the developer's
ignored local tuning/profile files.

## Interactive smoke checklist

After changes to runtime/presentation:

- Main menu, Brawlers, Practice, Play, Escape/back, and fade transitions.
- All five kit previews and both main/ultimate firing.
- Tank Reclamation healing, Shift Shoulder Jets cooldown/cover stop, and Charge.
- Guardian rain growth/pulses and Resonance ally/enemy statuses.
- Static, Roam, and Fight bots.
- Gem spawn, pickup, death drop, countdown, win/result return.
- Command-center sliders, reset, project promotion, scrolling, and pointer capture.
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
