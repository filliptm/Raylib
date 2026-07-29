# Brawl Arena architecture

Last code-verified: 2026-07-28

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

`UiSystem` also has process lifetime in `main.c`. It loads local font and curated UI-skin
textures once, builds a reference-canvas layout each frame, captures
pointer/keyboard/gamepad UI navigation, and owns the current/previous focus graph. Each
skin resource has an independent ready flag and a code-drawn fallback. It is not part of
deterministic simulation.

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

`ArenaMoveCircle()` is the shared terrain-movement primitive. It sweeps a circular body
in bounded substeps, resolves wall/crate overlap, and preserves tangential movement while
reporting the outward contact normal. Ordinary movement removes only inward velocity;
dashes and knockback use the same sweep while retaining their separate stop/destruction
rules. Brawlers deliberately have no actor-to-actor collision: every team may overlap
and pass through every other brawler while each body still resolves independently
against terrain.

AI short probes use the same brawler radius rather than point occupancy.
`AINavigationDirection()` follows a direct body-clear sweep when possible and otherwise
builds a small breadth-first route over the live arena grid. Crates participate while
intact and disappear from routing immediately when destroyed. This remains deterministic
and allocation-free through fixed stack arrays bounded by the arena capacities.

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
- Fourteen active typed `AbilityDefinition` records in a fifteen-slot fixed array: two
  per character plus Scrapper, Longshot, Mortar, and Tank secondary abilities.
- One `CharacterShowcaseDefinition` with a shared model transform, camera position,
  target, and vertical FOV for every character and menu screen.
- Up to eight validated `MapDefinition` records and selected map state.

`WeaponDef` preserves the stable configuration schema. After configuration load or a
live authoring edit, `ContentCatalogRebuildTyped()` produces the runtime definitions used
by game, AI, menus, HUD summaries, and aim previews.

An ability has a behavior tag and a typed projectile, area, dash, returning, shield,
grapple, or mine payload. Characters may expose main, super, and optional secondary
handles. Projectile
definitions can snapshot a self-heal ratio, dash payloads carry speed, duration,
knockback, and crate-breaking policy, returning payloads carry outbound/return phase
rules, and shield payloads carry capacity, movement, absorb-healing, recharge, and break
lockout values. Grapple payloads own launch/pull timing and use the ability range;
mine payloads own arm/trigger/knockback values while the ability owns blast
radius/damage. Generic
periodic `StatusEffect` slots support both ally healing-over-time and enemy
damage-over-time. New abilities should add reusable behavior handlers rather than
character-specific fields to `Brawler`.

`BrawlerApplyDamageDetailed()` is the common shield-before-health gateway for hostile
projectiles, fields, dashes, and periodic damage. Its separate `shieldAbsorbed` and
`healthRemoved` results let hit-confirm and crowd-control rules accept shield contact
while health-only mechanics such as Tank Reclaim remain tied to actual health loss.

Out-of-combat regeneration is a global reusable actor rule rather than an ability or
character field. Successful main/ultimate casts and actual health loss stamp centralized
combat time. A dedicated post-projectile simulation stage evaluates percentage healing,
so damage resolved on the current frame always interrupts recovery before a scheduled
pulse.

Maps have independent terrain, gameplay, visual, and prop layers. The loader validates
the complete catalog before a match can use it. See [MAPS.md](MAPS.md).

## Presentation decomposition

- `assets.c`: shader/model/material/render-target lifetime, including project-scaled
  color/sampleable-depth targets, separate source/output post resolutions, mipmapped
  trilinear 8× anisotropic station-atlas sampling, and temporally stable post grain.
- `generated_assets.c`: procedural textures and the grass cross-quad mesh.
- `environment.c`: map-cell and prop presentation. Wall collision keeps its full tile,
  while visible per-cell plinths remain inset to prevent adjacent coplanar geometry.
- `camera.c`: camera initialization, project-tuned distance, lead, follow, and permitted
  shake. Distance scales the fixed-pitch offset and never enters deterministic
  simulation state.
- `effects.c`: game-event consumption and transient visual pools.
- `vfx_catalog.c`: stable, kit-specific effect recipes.
- `vfx.c`: fixed VFX layer pool, animation, pose-socket resolution, sorting, and draw
  state restoration.
- `render_state.h`: batch-safe depth-write transitions shared by transparent,
  additive, billboard, decal, field, and preview passes.
- `ability_visuals.c`: active rain/sound fields, Scrapper Shell, traveling/persistent
  grapple cables, mine trigger/blast telegraphs, and all aim previews. The grapple
  preview resolves its destination through the gameplay-owned body-safe endpoint query.
- `character_animation.c`: pure match clip selection from life, dash/grapple, velocity, and
  facing, plus presentation-only action-state timing/blend envelopes. Concealment reveal
  and attack cooldown timers never double as animation state.
- `menu_scene.c`: hangar, podium, lights, non-rotating preview brawler, and application
  of the shared showcase. Its stage clock is independent of model-preview time, so
  candidate changes do not restart the background. Stage and brawler passes remain
  separate so tintable 2D station motifs can sit behind the model without entering
  world rendering.
- `render.c`: world-pass orchestration and brawler/projectile/grass drawing.

The renderer reads simulation snapshots and presentation events. Ability VFX can name a
source/target brawler and semantic rig socket; the final composed character pose supplies
the mapped positions, with approximate fallbacks for primitives and missing bones. The
renderer does not decide hits, healing, line-of-sight damage, objective results, or
other game rules. Presentation code must not call raw rlgl depth-mask toggles: raylib
batches immediate geometry, so `render_state.h` flushes pending draws on both sides of
every no-depth-write interval.

## UI ownership

`ui_system.[ch]`, `ui_theme.[ch]`, `ui_skin.[ch]`, `ui_icons.[ch]`, and `ui_types.h`
define the Helios Broadcast presentation contract: semantic colors, local fonts,
curated texture lifetime, nine-slice controls, per-resource fallbacks, named text roles,
1280×800 reference layout, focus IDs, input modality, controls, icons, and
reduced-motion timing. Migrated UI draws text and imported UI textures only through this
layer. `Assets` continues to own world models, shaders, textures, and render targets;
UI skin textures never enter deterministic or world-presentation state.

The player-facing shell is split by responsibility:

- `menu.c`: launch deck, roster candidate/commit behavior, Controls and Settings.
- `hud.c`: body-anchored numeric health/shield bars and player ammo, objective/ability
  broadcast, action-retired tutorials, downed state, and explicit result Continue.
- `menu_scene.c`: the 3D menu environment and character preview.
- `command_center.c`: explicit developer-tool state with a category rail, scrollable
  body, provenance header, and persistent save/reset footer.

The resizable window has a 960×600 minimum. Layout scales from the reference canvas.
When post-processing is active, the world renders into a color/sampleable-depth target
at `presentation.render_scale` (1.0×–2.0×, tracked default 1.5×) and downsamples before
native-resolution UI. Window-size and render-scale changes are debounced together, then
recreate the target and refresh separate source/output resolution uniforms. Failed
scaled allocation retries at native resolution before direct world rendering. The
backbuffer requests 4× MSAA for direct, post-disabled, resize, and failure-fallback
frames. UI preference state is profile-scoped; presentation framing and render scale are
project-scoped content and participate in transactional validation/promotion.

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
| Menu podium/hangar/model framing | `presentation/menu_scene` + content showcase |
| UI theme, text, focus, and components | `ui/ui_system` |
| UI texture lifetime, slicing, and decoration | `ui/ui_skin` |
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

`test_ui` exercises pure layout, target size, focus-neighbor, ID, motion, contrast,
nine-slice metadata, and presentation-profile behavior without opening a window.
`check-ui` prevents migrated player UI from bypassing shared text/texture ownership and
verifies shipped font and UI-asset hashes, dimensions, licenses, sources, and archive
policy. Graphical checks remain documented in
[UI_SMOKE_CHECKLIST.md](UI_SMOKE_CHECKLIST.md).

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
