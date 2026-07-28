# Brawl Arena architecture

Last code-verified: 2026-07-27

This is the implemented ownership and dependency contract for Brawl Arena. The project is
a modular C application with fixed-capacity simulation storage. It is not a reusable
engine or an entity-component system.

## Layer map

```text
core ──────┐
           ├──> content ───> game
           │                  │
           └──────────────────┤
                              v
                      app composition
                     /       |        \
             presentation    ui     devtools
```

Physical source ownership:

- `src/core`: limits, shared IDs/enums, raylib value types, deterministic random.
- `src/content`: tuning/content records, typed runtime catalog, configuration storage,
  external map loading and validation.
- `src/game`: deterministic arena, actors, AI, weapons, objective, statuses, and events.
- `src/app`: aggregate ownership, main loop, captured input/controller, application
  gameplay commands.
- `src/presentation`: assets, shaders, environment, camera, effects, ability visuals,
  menu scene, and world rendering.
- `src/ui`: shared UI system, player-facing menu, and HUD.
- `src/devtools`: command center and its immediate-mode widgets.

The C linker does not enforce layers on its own. `make check-architecture` runs
`tools/check_architecture.sh`, which rejects game/core imports of outer-layer headers,
`App *` simulation APIs, platform input reads, raylib random state, and direct rendering
or presentation calls.

## Owned state

`App` is the application aggregate:

```text
App
├── GameSession         match-lifetime deterministic state
├── PlayerController    local interaction/aim state
├── PresentationState   camera and transient visual pools
├── AppFlow             screens, fades, result banking, quit
├── Tuning              effective project/local/profile settings
├── ContentCatalog      authoring records + typed content + maps
├── ConfigState         canonical project snapshot and load status
└── UiPreferences       profile-scoped scale/motion/contrast/hints/glyphs
```

`Assets` has process lifetime and is owned next to `App` in `main.c`. It contains GPU and
imported resource handles rather than simulation data.

`UiSystem` also has process lifetime in `main.c`. It loads local font resources once,
builds a reference-canvas layout each frame, captures pointer/keyboard/gamepad UI
navigation, and owns the current/previous focus graph. It is not part of deterministic
simulation.

`GameSession` owns:

- Runtime arena and destructible tile state.
- Brawlers—including deterministic combat/recovery timestamps—projectiles, persistent
  ability fields, statuses, gems, and objective state.
- Match clock, player index, KO/death counters, and practice-session flag.
- Deterministic xorshift random state.
- The output `GameEventQueue`.

`ResetMatch()` clears only `GameSession`, `PlayerController`, and `PresentationState`.
Content, project defaults, local draft, profile, and screen-flow state remain intact
because their lifetime did not end.

## Simulation boundary

Game APIs accept `GameContext`:

```c
typedef struct GameContext {
    GameSession *session;
    Tuning *tuning;
    ContentCatalog *content;
} GameContext;
```

This is the entire mutation/read surface available to deterministic simulation. It has no
camera, particles, controller, UI, persistence, screen flow, or device state.

The app layer creates a context with `AppGameContext()`. Passing it by value is cheap and
keeps nested game calls on the same session/catalog.

Simulation constraints:

1. Never include application, presentation, UI, or developer-tool headers.
2. Never read raylib keyboard/mouse state.
3. Never call `GetRandomValue`; use the session's `GameRandom`.
4. Never draw, spawn presentation effects, or mutate camera state.
5. Emit `GameEvent` values for observable presentation.
6. Preserve fixed-pool allocation behavior unless capacity is deliberately redesigned.

## Input, commands, and events

Device state is captured once per rendered frame into `PlayerInput`. `PlayerUpdate()` is
an app/controller system: it converts the input frame into simulation intent and invokes
narrow game APIs. Tests can supply the same value without calling raylib input functions.

The command center uses `GameCommandExecute()` for actions such as changing a roster,
respawning/killing/healing actors, spawning or clearing gems, changing class, and
resetting objective/score state. Sliders edit the owned tuning/content authoring values
and rebuild the typed catalog.

Game systems write to `GameEventQueue`. `FxConsumeGameEvents()` converts those events into
presentation-owned particles, text, lights, and shockwaves after simulation. Dropped
events are counted when the fixed queue is full.

Combat does not emit camera shake. The event type remains available only for intentional
non-combat match presentation; tests assert that all five kits, impacts, damage, and
eliminations leave camera shake unchanged.

## Content boundary

`ContentCatalog` owns:

- Five compatibility `WeaponDef` authoring records.
- Five typed `CharacterDefinition` records.
- Eleven active typed `AbilityDefinition` records in a fifteen-slot fixed array: two per
  character plus Tank's optional mobility ability.
- Five `CharacterPresentationDefinition` records with independent home/select yaw,
  scale, and offsets plus camera height and distance.
- Up to eight validated `MapDefinition` records and selected map state.

`WeaponDef` preserves the stable configuration schema. After configuration load or a
live authoring edit, `ContentCatalogRebuildTyped()` produces the runtime definitions used
by game, AI, menus, HUD summaries, and aim previews.

An ability has a behavior tag and a typed projectile, area, or dash payload. Characters
may expose main, super, and optional mobility handles. Projectile definitions can
snapshot a self-heal ratio, while dash payloads carry speed, duration, knockback, and
crate-breaking policy. Generic
periodic `StatusEffect` slots support both ally healing-over-time and enemy
damage-over-time. New abilities should add reusable behavior handlers rather than
character-specific fields to `Brawler`.

Out-of-combat regeneration is a global reusable actor rule rather than an ability or
character field. Successful main/ultimate casts and actual health loss stamp centralized
combat time. A dedicated post-projectile simulation stage evaluates percentage healing,
so damage resolved on the current frame always interrupts recovery before a scheduled
pulse.

Maps have independent terrain, gameplay, visual, and prop layers. The loader validates
the complete catalog before a match can use it. See [MAPS.md](MAPS.md).

## Presentation decomposition

- `assets.c`: shader/model/material/render-target lifetime.
- `generated_assets.c`: procedural textures and the grass cross-quad mesh.
- `environment.c`: map-cell and prop presentation.
- `camera.c`: camera initialization, lead, follow, and permitted shake.
- `effects.c`: game-event consumption and transient visual pools.
- `ability_visuals.c`: active rain/sound fields and all aim previews.
- `menu_scene.c`: hangar, podium, lights, non-rotating preview brawler, and application
  of per-character presentation profiles.
- `render.c`: world-pass orchestration and brawler/projectile/grass drawing.

The renderer reads simulation snapshots. It does not decide hits, healing, line-of-sight
damage, objective results, or other game rules.

## UI ownership

`ui_system.[ch]`, `ui_theme.[ch]`, `ui_icons.[ch]`, and `ui_types.h` define the Helios
Broadcast presentation contract: semantic colors, local fonts and fallback handling,
named text roles, 1280×800 reference layout, focus IDs, input modality, controls, icons,
and reduced-motion timing. Migrated UI draws text only through this layer.

The player-facing shell is split by responsibility:

- `menu.c`: launch deck, roster candidate/commit behavior, Controls and Settings.
- `hud.c`: objective/vitals/ability broadcast, action-retired tutorials, downed state,
  and explicit result Continue.
- `menu_scene.c`: the 3D menu environment and character preview.
- `command_center.c`: explicit developer-tool state with a category rail, scrollable
  body, provenance header, and persistent save/reset footer.

The resizable window has a 960×600 minimum. Layout scales from the reference canvas and
the scene render target is recreated on resize, with direct world rendering as a failure
fallback. UI preference state is profile-scoped; presentation framing is project-scoped
content and participates in transactional validation/promotion.

## Dependency-safe feature placement

Place a change according to the state it owns:

| Change | Owner |
|---|---|
| New numeric designer value | `content` config/schema and tracked project config |
| New map/layout/prop | `data/maps`, parsed by `content` |
| New reusable attack/status rule | `game` |
| Keyboard/mouse/controller mapping | `app` |
| Developer mutation button | `devtools` + `app` command |
| Particle/light/float text response | game event + `presentation/effects` |
| Aim shape or field visualization | `presentation/ability_visuals` |
| Menu/HUD display | `ui` |
| Menu podium/hangar/model framing | `presentation/menu_scene` + content presentation profile |
| UI theme, text, focus, and components | `ui/ui_system` |
| Shader/model/texture lifetime | `presentation/assets` |

Avoid utilities that require every layer. A narrow duplicate helper is preferable to
making game depend on presentation.

## Verification

```bash
make check-architecture
make check-ui
make validate-config
make test
make sanitize
```

The game test executable links core/content/game objects without presentation or UI.
The replay test additionally links the app controller but still verifies that
presentation state remains untouched.

`test_ui` exercises pure layout, target size, focus-neighbor, ID, motion, contrast, and
presentation-profile behavior without opening a window. `check-ui` prevents migrated
player UI from bypassing shared text ownership and verifies the shipped font/license
set. Graphical checks remain documented in [UI_SMOKE_CHECKLIST.md](UI_SMOKE_CHECKLIST.md).

Non-Darwin sanitizer builds use ASan plus UBSan. The current Apple clang/macOS 26 ASan
runtime deadlocks in its own initializer, so Darwin's maintained target runs strict
UBSan instead.

Interactive checks are still required for shader compilation, imported assets, animation
selection, input feel, command-center interaction, and complete screen transitions.

## Deliberate remaining seams

- Character/ability runtime types are clean, but the current five IDs, model IDs, enum
  slots, and compatibility config schema remain compiled.
- Command-center sliders directly edit owned authoring values before rebuilding the typed
  catalog; button-driven gameplay mutations use commands.
- Some renderer/devtool caches are file-static because they have process/UI lifetime.
  They must remain private to their subsystem and must not become simulation state.
