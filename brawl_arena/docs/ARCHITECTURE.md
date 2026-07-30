# Brawl Arena architecture

Last code-verified: 2026-07-29

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
  gameplay commands, platform paths/safe areas, and touch-to-player-input mapping.
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
├── PresentationState   camera, transient visual pools, and resettable HUD feedback
├── AppFlow             screens, fades, result banking, quit
├── MobileControlsState stable touch IDs, virtual-stick anchors, and held actions
├── Tuning              effective project/local/profile settings
├── ContentCatalog      authoring records + typed content + maps
├── ConfigState         canonical project snapshot and load status
└── UiPreferences       profile-scoped scale/motion/contrast/hints/glyphs
```

`Assets` has process lifetime and is owned next to `App` in `main.c`. It contains GPU and
imported resource handles rather than simulation data.

`UiSystem` also has process lifetime in `main.c`. It loads local fonts and the
procedural UI skin once; builds a reference-canvas layout each frame; captures
pointer/keyboard/gamepad/touch UI navigation; applies platform safe-area insets; and owns
the current/previous focus graph. It is not part of deterministic simulation.

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
Touch ownership resets when a session or application-active lifetime ends, preventing a
contact that began before a transition or background event from leaking into gameplay.

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

On iPhone, `player_touch.c` assigns raylib touch IDs to one floating movement stick, one
floating aim/fire stick, **SUPER**, **SKILL**, and pause. It converts stick displacement
through the current camera basis, then writes the same move, aim, preview, release,
secondary, and pause fields used by the desktop controller. Movement normalizes to a
unit vector after the dead zone, so touch and physical-gamepad sticks select direction
but never a slower walking speed; aim magnitude remains analog. Attack touch-down alone
does not enter charging. Crossing the drag threshold emits the deliberate press edge,
while release without crossing emits the direct auto-aim edge, so tap-to-shoot cannot
render a preview. An explicit aimed marker prevents a fast drag/release from being
misclassified by the desktop time-based tap rule. The mapping, gesture classification,
full-speed normalization, and layout are pure functions covered without a window;
game/core code never sees touch IDs, UIKit, or virtual-control rectangles.

The command center uses `GameCommandExecute()` for actions such as changing a roster,
respawning/killing/healing actors, spawning or clearing gems, changing class, and
resetting objective/score state. Sliders edit the owned tuning/content authoring values
and rebuild the typed catalog.

Game systems write to `GameEventQueue`. `FxConsumeGameEvents()` converts those events into
presentation-owned particles, text, lights, and shockwaves after simulation. Dropped
events are counted when the fixed queue is full.

Generic floating labels use `GameEmitFloatText()`. Damage, healing, and shield values use
`GameEmitCombatText()`, which queues text only when `GameSession.playerIdx` is the source
or target. This keeps bot-only exchanges out of both the event queue and presentation
pool while retaining local outgoing, incoming, self-heal, and shield feedback.

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
  color/sampleable-depth targets sized from drawable framebuffer pixels, separate
  source/output post resolutions, mipmapped trilinear 8× anisotropic station-atlas
  sampling, removal of unused tiled-wall longitudinal caps, and temporally stable post
  grain.
- `generated_assets.c`: procedural textures and the grass cross-quad mesh.
- `environment.c`: map-cell and prop presentation. Wall collision keeps its full tile;
  open-ended imported wall skins meet at single-owner edges, while visible per-cell
  plinths remain inset to prevent adjacent coplanar geometry.
- `camera.c`: camera initialization, project-tuned distance, lead, follow, and permitted
  shake. Distance scales the fixed-pitch offset and never enters deterministic
  simulation state. iPhone applies a presentation-only 0.80 effective-distance
  multiplier with a 20-unit floor.
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
- `menu_scene.c`: flat vector arena podium, non-rotating preview brawler, transparent
  sticker render target/shader, and application of the shared showcase. Its circular
  multi-direction dilation and softened alpha thresholds produce a rounded ink/paper
  contour. The stage clock is independent of model-preview time, so candidate changes
  do not restart the stage. Stage and brawler passes remain separate so the UI layer
  can sit behind and around the composited silhouette without entering world rendering.
- `render.c`: world-pass orchestration and brawler/projectile/grass drawing.

Imported runtime character GLBs are used when available on both desktop and iPhone.
Desktop uploads composed bone matrices to the skinned shader. The pinned iPhone ANGLE
path instead applies the same animation poses to the model's position and normal buffers
with raylib CPU skinning, then draws them through the OpenGL ES 3 lighting shader. The
procedural characters remain asset-failure fallbacks on both platforms.

The renderer reads simulation snapshots and presentation events. Ability VFX can name a
source/target brawler and semantic rig socket; the final composed character pose supplies
the mapped positions, with approximate fallbacks for primitives and missing bones. The
renderer does not decide hits, healing, line-of-sight damage, objective results, or
other game rules. Presentation code must not call raw rlgl depth-mask toggles: raylib
batches immediate geometry, so `render_state.h` flushes pending draws on both sides of
every no-depth-write interval.

## UI ownership

`ui_system.[ch]`, `ui_theme.[ch]`, `ui_skin.[ch]`, `ui_icons.[ch]`, and `ui_types.h`
define the Arena Ink presentation contract: semantic colors, local fonts, procedural
comic geometry, named text roles, 1280×800
reference layout, focus IDs, input modality, controls, icons, easing, and reduced-motion
timing. Migrated UI draws text and UI primitives only through this layer. `Assets`
continues to own world models, shaders, textures, and scene render targets; the
menu-specific transparent sticker target belongs to `MenuScene`, and no UI state enters
deterministic simulation.

`content_catalog.c` exposes one immutable `CharacterUiStyle` per stable kit—two colors,
a mechanic-derived motif, and a short impact label. UI and presentation consume that
typed identity without branching on character names.

The player-facing shell is split by responsibility:

- `menu.c`: launch deck, mechanic-derived character motifs, orchestrated entrance,
  roster candidate/commit behavior, Controls, and Settings.
- `phone_layout.c`: pure safe-width iPhone frames and home/roster/result geometry.
- `hud.c`: body-anchored numeric health/shield bars and player ammo, objective/ability
  broadcast, resettable impact-stamp detection, action-retired tutorials, downed state,
  and the Continue/Rematch/Change Brawler result poster.
- `mobile_controls.c`: native-resolution, safe-area-aware translucent movement/attack
  sticks with opacity independent of touch state, 75%-scale Super/Skill artwork over
  unchanged touch regions, exterior charge/cooldown rings, ready-face/halo feedback,
  and ability/pause idle fade.
- `menu_scene.c`: the vector arena podium, character preview, and rounded sticker
  compositing pass.
- `command_center.c`: explicit developer-tool state with a category rail, scrollable
  body, provenance header, and persistent save/reset footer.

The resizable window has a 960×600 minimum. Layout scales from the reference canvas.
On iPhone, the backdrop extends across the full viewport beneath the cutout and
home-indicator regions. Home, roster, Controls, Settings, downed, and result UI
temporarily use a 500-unit-tall phone frame whose reference width expands to fill the
inset landscape safe area; the shared process-lifetime layout is restored afterward.
Touch targets expand to at least 44 points, and touch-specific binding labels are used
when the current modality is touch.
When post-processing is active, the world renders into a color/sampleable-depth target
at `presentation.render_scale` (1.0×–2.0×, tracked default 1.5×) and downsamples before
native-resolution UI. Scene-target sizing and post output resolution use drawable
framebuffer pixels; UI layout and input retain logical screen coordinates. Window-size
and render-scale changes are debounced together, then recreate the target and refresh
separate source/output resolution uniforms. Failed scaled allocation retries at native
resolution before direct world rendering. The backbuffer requests 4× MSAA for direct,
post-disabled, resize, and failure-fallback frames. UI preference state is
profile-scoped; presentation framing and render scale are project-scoped content and
participate in transactional validation/promotion.
The iPhone runtime keeps authored project values intact, caps the effective render scale
at 1.0×, disables the post pass, and applies the camera distance policy described above.
World shaders compile as OpenGL ES 3 variants, while the safe-area HUD remains
native-resolution.

`platform.c` is the only app-owned platform policy module. The iPhone bridge changes the
working directory to the bundled `BrawlAssets` resource root before content load,
reports UIKit safe-area insets, and redirects ignored draft/profile/legacy paths to
Application Support. The packaged canonical project config remains read-only.

## Dependency-safe feature placement

Place a change according to the state it owns:

| Change | Owner |
|---|---|
| New numeric designer value | `content` config/schema and tracked project config |
| New map/layout/prop | `data/maps`, parsed by `content` |
| New reusable attack/status rule | `game` |
| Keyboard/mouse/controller mapping | `app` |
| Touch ownership, virtual-stick mapping, or platform paths | `app` |
| Developer mutation button | `devtools` + `app` command |
| Particle/light/float text response | game event + `presentation/effects` |
| Aim shape or field visualization | `presentation/ability_visuals` |
| Menu/HUD display | `ui` |
| Menu stage/model/sticker framing | `presentation/menu_scene` + content showcase |
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

`test_ui` exercises pure layout, target size, focus-neighbor, ID, easing/reduced motion,
distinct character motifs, result actions, contrast, procedural-skin lifetime, and
presentation-profile behavior without opening a window. It also covers notched safe
areas, dedicated phone compositions, 44-point touch expansion, non-overlapping mobile
controls, camera-relative stick mapping, binary full-speed movement, tap/drag attack
classification, and touch binding language.
`check-ui` prevents migrated player UI from bypassing shared text/skin ownership and
verifies shipped fonts, procedural ownership, retained reference provenance, and archive
policy. Graphical checks remain documented in
[UI_SMOKE_CHECKLIST.md](UI_SMOKE_CHECKLIST.md).

Non-Darwin sanitizer builds use ASan plus UBSan. The current Apple clang/macOS 26 ASan
runtime deadlocks in its own initializer, so Darwin's maintained target runs strict
UBSan instead.

Interactive checks are still required for shader compilation, imported assets, animation
selection, input feel, command-center interaction, and complete screen transitions.
The iPhone build/install workflow and its separate simulator/device checks are documented
in [`../apple/README.md`](../apple/README.md).

## Deliberate remaining seams

- Character/ability runtime types are clean, but the current five IDs, model IDs, enum
  slots, and compatibility config schema remain compiled.
- Command-center sliders directly edit owned authoring values before rebuilding the typed
  catalog; button-driven gameplay mutations use commands.
- Some renderer/devtool caches are file-static because they have process/UI lifetime.
  They must remain private to their subsystem and must not become simulation state.
