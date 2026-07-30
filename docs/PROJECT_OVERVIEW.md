# Project Overview

Last code-verified: 2026-07-29

This is the maintained repository-level guide to the projects in this workspace. It is
intended for contributors, coding agents, and anyone deciding where a change belongs.

The code is the final authority. This document records the behavior implemented in the
current tree, including places where older project READMEs describe an earlier design.

## Repository at a glance

The repository combines a learning collection with three independent game prototypes:

```text
Raylib/
├── example_launcher.c          Graphical browser for selected raylib examples
├── raylib-examples/            Vendored examples, resources, screenshots, and Makefiles
├── squad_runner/               One-file endless autorunner
├── hearthstone/                Modular card-game prototype
├── brawl_arena/                Arena-brawler vertical slice and tuning environment
├── docs/
│   ├── README.md               Documentation index
│   ├── PROJECT_OVERVIEW.md     This repository-wide source of truth
│   └── LAUNCHER_README.md      Launcher-specific guide
└── AGENTS.md                   Repository instructions for coding agents
```

There is no root build that produces every project. Each project has its own executable,
state, build path, and design goals.

## Shared technology

- Language: C99 for the applications and games.
- Graphics/input/windowing: raylib.
- Math: raylib vectors plus `raymath.h` where needed.
- Build system: Makefiles for each game; a direct compiler command for the launcher.
- Primary current platform: macOS with Homebrew raylib and Apple OpenGL frameworks.
- Checked raylib version: 5.5.0.

All applications are real-time graphical executables. Build checks can run unattended,
but runtime checks open windows and should be performed deliberately.

## Project comparison

| Project | Size and shape | Main architectural style | Best description |
|---|---|---|---|
| Examples launcher | One 504-line C file plus a vendored examples tree | Global launcher state and on-demand shell compilation | Learning/reference browser |
| Squad Runner | One 1,334-line C file | One global `GameState` with fixed arrays | Focused playable prototype |
| Hearthstone | About 12,400 lines of C and headers across many modules | Central `GameState` plus partially integrated subsystems | Broad experimental platform |
| Brawl Arena | About 11,000 lines across seven source subsystems | Owned `App` state plus deterministic fixed-pool simulation | Cohesive combat vertical slice |

Line counts are descriptive, not contractual. Update them if a major reorganization makes
them materially misleading.

# Raylib examples and graphical launcher

## Purpose

`raylib-examples/` is a local copy of the raylib examples collection. It provides small
programs for studying windowing, input, shapes, textures, text, 3D models, shaders, audio,
and lower-level integrations.

`example_launcher.c` adds a graphical menu for browsing a subset of that collection and
compiling one selected example on demand.

This part of the repository is a reference environment rather than a shared library used
by the three games.

## Collection contents

The current tree contains 188 example C files:

- 45 `core` examples.
- 32 `shapes` examples.
- 26 `textures` examples.
- 15 `text` examples.
- 25 `models` examples.
- 31 `shaders` examples.
- 8 `audio` examples.
- 6 `others` examples.

The first seven groups total the 182 examples advertised by the older root documentation.

`raylib-examples/examples_list.txt` currently contains 187 indexed records: 181 in the
seven primary categories and six in `others`. The file
`shapes/shapes_lines_drawing.c` exists but is not present in that index.

Because the launcher accepts only the seven primary categories and reads from the index,
it currently displays 181 entries rather than all 182 primary example source files. It
also intentionally omits `others`.

## Launcher data flow

The launcher uses fixed global storage:

- `ExampleInfo examples[200]` holds the parsed entries.
- Eight tabs represent All plus the seven accepted categories.
- `selectedExample`, `scrollOffset`, and `activeTab` drive navigation.

At startup, `LoadExamplesList()`:

1. Opens `raylib-examples/examples_list.txt` relative to the repository root.
2. Skips comments and blank lines.
3. Splits each row on semicolons.
4. Keeps category, filename, and difficulty stars.
5. Rejects categories outside the seven main tabs.
6. Builds a relative `.c` filepath and per-tab counts.

The executable therefore must be launched from the repository root unless its path logic
is changed.

## Launcher interaction

- Mouse wheel: move through the list.
- Single click: select an example.
- Double click: compile and run the clicked example.
- Up/Down: move one item.
- Page Up/Page Down: move one visible page.
- Home/End: first or last item.
- Enter: compile and run the selected item.
- `C`: execute `cat` on the selected source file, printing it to the terminal.
- Escape: close the window through raylib's default exit handling.

There is category filtering but no text search. “Source viewer” in older documentation
means terminal output, not an embedded code view.

## Launcher build and execution

From the repository root:

```bash
clang -std=c99 -Wall -Wextra example_launcher.c -o example_launcher \
  $(pkg-config --cflags --libs raylib) \
  -framework OpenGL -framework Cocoa -framework IOKit

./example_launcher
```

When an example is launched, the application constructs a command equivalent to:

```bash
cd raylib-examples/<category>
gcc <example>.c -o /tmp/<example> \
  $(pkg-config --cflags --libs raylib) \
  -framework OpenGL -framework Cocoa -framework IOKit
/tmp/<example>
```

This compile-and-run path is macOS-specific. The vendored examples and their Makefiles
contain broader platform support, but the launcher command itself does not.

## Assets and ownership

Each example owns its source and any required local resources. Many examples assume their
working directory is their category directory, which is why the launcher changes
directories before compiling and running.

Treat `raylib-examples/` as vendored/reference material. Changes to an individual example
should be scoped and should not be mixed into game work unless explicitly needed.

## Known limitations

- One primary example is absent from the index and therefore from the launcher.
- `others` examples are not represented in the UI.
- No text search or in-app source viewer.
- Compiler command is assembled with `system()` and assumes trusted indexed filenames.
- Compile feedback goes to the terminal rather than the launcher window.
- The fixed 200-entry array leaves little headroom if the upstream collection grows.
- The root launcher build currently emits a non-fatal macOS linker alignment warning.
- Not every vendored example is guaranteed to compile against the locally installed
  raylib version; examples may reflect newer upstream development.

## Relevant documentation

- `docs/LAUNCHER_README.md`
- `raylib-examples/README.md`
- Category-local source comments and resource directories.

# Squad Runner

## Purpose

Squad Runner is a compact hybrid 3D/2D endless runner. It tests whether squad growth,
automatic fire, lane steering, simple formation logic, and escalating pressure make a
readable arcade loop.

The entire game is in `squad_runner/src/main.c`. This is a deliberate prototype shape:
there is no engine layer, external data model, or asset pipeline.

## Player experience

The squad automatically travels along positive world Z at a constant forward pace. The
player steers the squad center laterally while individual units arrange themselves around
that center.

The loop is:

1. Begin with five squad units.
2. Steer along the road.
3. Units automatically target and fire at the nearest valid enemy ahead.
4. Enemies return fire.
5. Each enemy projectile that lands removes one squad member.
6. Collect `+N` or `×2` pickups to grow the squad.
7. Continue as enemy health and pressure increase with distance.
8. Game over when the squad reaches zero; restart with `R`.

Score, combo, distance, particles, hit flashes, death motion, floating labels, muzzle
flashes, and camera motion reinforce this loop.

## Input

- `A`/`D` or Left/Right: steer the squad laterally.
- Hold left mouse: map horizontal pointer position onto the road and steer toward it.
- `R`: restart after the game-over delay.

Combat is automatic.

## Runtime architecture

A global `GameState` owns fixed arrays for:

- Squad members.
- Enemies.
- Player and enemy projectiles.
- Collectibles.
- Particles.
- Floating text.
- Camera and scoring state.

`InitGame()` resets state and creates the initial squad. `UpdateGame()` performs spawning,
input, formation, combat, collision, scoring, particles, and game-over logic.
`DrawGame()` renders the road, characters, projectiles, effects, and screen-space HUD.

Squad formation uses concentric elliptical rings around the steered squad center. This
lets a changing unit count remain visually legible without hand-authored layouts.

## Combat and scaling

- The squad fires at enemies ahead within a fixed engagement distance.
- Enemies spawn in groups and shoot toward the squad.
- Enemy health grows with traveled distance.
- Projectiles use simple position updates and distance/collision checks.
- A successful enemy shot removes one unit rather than damaging a shared health pool.
- Collectibles add a fixed number of units or multiply the current count, capped by the
  fixed squad array.

The resulting difficulty curve is driven mainly by distance-scaled health and the squad's
ability to maintain enough simultaneous fire.

## Rendering and assets

The road, units, enemies, pickups, bullets, and effects are built from raylib primitives.
There is no required external art or audio. The `resources/` directory is currently empty.

## Build

```bash
make -C squad_runner
make -C squad_runner run
```

The binary is `squad_runner/squad_runner`.

For a warning-focused source check:

```bash
clang -std=c99 -Wall -Wextra -fsyntax-only \
  $(pkg-config --cflags raylib) squad_runner/src/main.c
```

## Tests and maturity

There is no automated test suite. Verification is compilation plus interactive play.

The game is playable and understandable, but its one-file/global-state design is best
suited to experimentation. Adding multiple modes, persistent content, sophisticated
collision, or a reusable rendering layer would justify splitting it into modules.

## Known limitations

- No dedicated README inside `squad_runner/`.
- No audio, external art pipeline, or saved state.
- No deterministic tests.
- Fixed-capacity arrays silently define content limits.
- High-speed projectile collision is not substepped.
- The mouse-derived squad-velocity calculation assumes a nonzero frame delta.

# Hearthstone-inspired card game

## Purpose and honest status

`hearthstone/` is a broad 3D card-game prototype inspired by Hearthstone. It explores
turn flow, cards, combat, AI, networking, editor tools, data loading, save systems,
logging, events, and modular rendering.

It is not currently a complete integrated game despite that wording in its local README.
Several subsystems are real and tested in isolation but are not connected to the runtime
started by `main.c`.

## Startup modes

Run commands from `hearthstone/`:

```bash
make
make run
```

The executable is `build/hearthstone` and accepts:

```bash
./build/hearthstone                 # medium AI opponent
./build/hearthstone ai 0            # easy AI
./build/hearthstone ai 1            # medium AI
./build/hearthstone ai 2            # hard AI
./build/hearthstone server 7777
./build/hearthstone client 127.0.0.1 7777
```

An unrecognized argument falls back to a local two-player state.

## Main runtime

`main.c` creates a 1400×900 raylib window and initializes one `GameState`.

Each frame:

1. Update the editor and its mode toggles.
2. If the editor is inactive, process game input.
3. Update the game, including win checks, cards, effects, AI, networking, polish systems,
   and the action queue.
4. Draw the base 3D game.
5. If active, draw editor tools and editor UI over the game.

`GameState` owns both players, active turn and phase, selection/targeting state, effects,
queued actions, camera, winner state, and pointers to optional AI, networking, and polish
systems.

## Game initialization and turn flow

Initialization:

- Zeroes the game state.
- Initializes two players and their hard-coded decks.
- Draws three starting cards per player.
- Initializes the polish layer.
- Marks player zero active and starts the first turn.
- Optionally attaches AI or network state.

Starting a turn:

- Draws one card.
- Increases and refreshes mana, capped at ten.
- Refreshes hero power availability.
- Resets minion attack state.
- Emits a turn-start visual effect.

Ending a turn switches the active player and immediately starts the next turn. A player
loses when health or alive state reaches the losing condition. Empty-deck draws apply
increasing fatigue damage.

## Live card and gameplay path

The game played by `main.c` uses:

- `card.c` for hard-coded card definitions and keywords.
- `player.c` for players, decks, hands, boards, mana, and fatigue.
- `gameplay.c` for card/hero-power execution and validation helpers.
- `combat.c` for attacks, damage, healing, and combat keywords.
- `input.c` for drag/drop play and attacks.
- `game_state.c` for turn and top-level lifecycle.

The live decks are generated from hard-coded card IDs, shuffled, and copied into each
player. They do not currently come from `data/cards.json` or `data/decks.json`.

## Player input

Normal game mode uses drag and drop:

- Left press an owned card: select it.
- Hold left mouse: drag a hand or board card.
- Release a hand card over the board: play a valid minion.
- Release a targeted spell or battlecry over a target: attempt the effect.
- Drag a board minion onto a target: attack.
- Space: end the current turn.
- Escape: cancel selection or targeting.
- `R`: after game over, initialize a new local game.

The older local README's right-click instructions do not match the current input code.

## AI

`ai.c` contains a real heuristic opponent. It evaluates:

- Playable cards and mana efficiency.
- Board development and card value.
- Minion trades.
- Face damage and lethal opportunities.
- Hero-power use.
- Whether ending the turn is preferable.

Difficulty changes personality weights, think timing, and mistake chance. The AI module
is connected through `game_ai.c` and is used by the default startup mode.

## Rendering and polish

Rendering is split between the root `render.c` entry point and the `render/` directory:

- Board renderer.
- Card renderer.
- UI renderer.
- Effect renderer.

Cards are interactive 3D objects with hover, selection, dragging, and board/hand layouts.
Effects and polish modules add visual feedback and camera behavior.

Audio initializes a raylib audio device and exposes an API, but sound and music playback
functions are currently TODO stubs.

## Editor

`editor/` implements an in-game board/layout editor. It is initialized for every game and
drawn on top of the normal scene when enabled.

Notable controls include:

- F12: toggle editor mode.
- WASD and Q/E: move the editor camera.
- Right mouse: editor camera look/orbit behavior.
- Left mouse: selection and gizmo dragging.
- F1, F5, Tab, `G`, modifiers, and Escape: editor-specific tools and UI behavior.

Consult `hearthstone/docs/EDITOR_USER_MANUAL.md` and the current key handling in
`editor/editor.c` before modifying editor interactions.

## Data and core subsystem path

`data/` contains:

- `cards.json`: eight sample data-driven cards.
- `decks.json`: three sample deck templates.
- `balance.json`: game limits, mana curve guidance, and AI difficulty values.

`core/data_manager.c` parses and validates this data, but only tests currently initialize
it. `main.c` does not create a `DataManager`, `RulesEngine`, or data-driven deck.

Likewise:

- `core/rules_engine.c` is a separate rule-validation layer not driving the live input
  path.
- `core/event_system.c` is functional and tested but not the central runtime event path.
- `core/save_system.c` can manage save metadata/files, but full JSON state restoration is
  unfinished.
- `utils/logging.c` is a tested utility rather than a pervasive runtime dependency.

This creates two architectural layers: the older playable root modules and a newer
modular core/data layer that has not yet replaced them.

## Networking

The project can initialize server or client sockets and exchange simplified packet types.
The integration is prototype-grade:

- Receive calls are blocking.
- The first connected socket is favored by simplified paths.
- State synchronization copies only part of the game.
- Target and action reconstruction is incomplete.
- There is no robust framing, retry, disconnect recovery, or authoritative simulation.

Do not describe multiplayer as production-ready without substantially changing this
layer and testing two real processes.

## Action queue and targeting limitations

The action queue currently performs End Turn and Concede. Play Card and Attack cases are
comments because those actions happen directly in input/combat code.

`IsValidPlayTarget()` currently accepts any non-null target. Card-specific friendly,
enemy, hero, minion, damaged, and keyword restrictions are therefore incomplete even
though higher-level play code looks modular.

Restarting with `R` after game over calls `InitializeGame()` directly. Because that
function begins with `memset`, it discards pointers to AI/network/polish allocations
without cleaning them up and restarts in local mode. This is a lifecycle bug, not just a
documentation issue.

## Build and tests

```bash
make -C hearthstone
make -C hearthstone test

# These two test executables exist but are not invoked by the aggregate target.
make -C hearthstone test_animation
make -C hearthstone test_save_system
```

The available suites cover:

- Error handling.
- Configuration.
- Animation helpers.
- Data manager.
- Save-system helpers.
- Logging.
- Event system.

All seven executables pass in the checked environment. The default `make test` target
runs five and omits animation and save-system execution even though their objects are
part of the test source list.

`tests/run_tests.c` is not the real aggregate runner: its wrapper functions only print
that tests “would run.” The Makefile directly links and executes the real suite binaries.

## Known limitations and direction

- Runtime cards and JSON data are disconnected.
- Target validation is permissive.
- Network updates can block the frame loop.
- Full save restoration is not implemented.
- Audio playback is not implemented.
- Turn-start and turn-end effect hooks are empty.
- The queued play/attack architecture is not used.
- Game-over restart has allocation and mode-reset problems.
- Many tests validate infrastructure rather than an end-to-end match.
- Project-local architecture documentation contains old “future work” statements for
  systems that now exist, alongside “complete” claims for systems still unfinished.

A productive next step would be to choose one authoritative architecture: either finish
connecting the core/data/event/rules layers or remove the duplicate paths and harden the
smaller live game.

# Brawl Arena

## Purpose and player-facing scope

`brawl_arena/` is a top-down 3D arena-brawler vertical slice. It combines a combat-feel
sandbox with a menu shell, character selection, practice mode, Gem Grab, allied and enemy
AI, concealment, destructible cover, live designer tuning, imported rigged characters,
an external map format, and a stylized rendering pipeline.

The main launch deck is intentionally sparse: it presents the title, open character
stage, active brawler and mode controls, Practice, settings utilities, and Deploy without
duplicating combat telemetry. Brawler Select is the comparison surface, with live
identity, ability, and stat readouts around a centered candidate preview and a fixed
five-character choice row.

The current roster contains five kits:

| Kit | Role | Main attack | Ultimate |
|---|---|---|---|
| Scrapper | damage | returning Ripsaw that can hit once on each leg, plus renewable Magnetic Scrap Shell | returning crate-breaking Wrecking Disc with outbound pull and return knockback |
| Longshot | marksman | tightly paired range-scaled projectiles plus Shift Mag-Line Grapple | piercing Railgun |
| Mortar | artillery | arcing splash shell plus Shift Concussion Mine | three-shell Barrage |
| Tank | tank | short six-pellet burst that self-heals from actual damage, plus Shift Shoulder Jets | damaging crate-breaking Charge |
| Guardian | support | growing rain field that repeatedly damages enemies and heals allies | wide Resonance cone that applies enemy damage-over-time or ally healing-over-time |

Guardian's tracked mesh source is
`resources/characters/models/gaia_guardian.glb`; its generated runtime asset is
`build/assets/characters/gaia_guardian.glb`. The current Guardian defaults produce nine
255-damage/263-healing rain pulses over 1.35 seconds and six Resonance ticks over 2.1
seconds. Those numbers are content values, not hard-coded combat timing.

Scrapper, Longshot, Tank, and Guardian use tracked mesh-only models plus the reusable twelve-clip
`resources/characters/animations/meshy_humanoid_v1.glb` library. Small animation-only
override libraries preserve Scrapper's idle/hit/backpedal and Guardian's distinctive
idle. `make character-assets` retargets motion relative to each model's recorded
animation rest pose and generates self-contained raylib GLBs under
`build/assets/characters/`. All embedded character PNGs are exactly 1024×1024 (1K).
Longshot's unusually dense source mesh is automatically partitioned into four
raylib-safe 16-bit indexed primitives during import; the validator rejects unsafe
32-bit runtime indices instead of allowing raylib to truncate them.
`make check-character-assets` is part of normal builds and tests and rejects incompatible
rigs, missing/full-TRS clips, root drift, external or non-PNG textures, and non-1K assets.

## Build and verification

From the repository root:

```bash
make -C brawl_arena
make -C brawl_arena run
make -C brawl_arena character-assets
make -C brawl_arena vfx-assets
make -C brawl_arena validate-config
make -C brawl_arena check-architecture
make -C brawl_arena check-ui
make -C brawl_arena ui-assets
make -C brawl_arena check-character-assets
make -C brawl_arena check-vfx-assets
make -C brawl_arena test
make -C brawl_arena sanitize
```

The executable is `brawl_arena/build/brawl_arena`. The Makefile discovers C sources
recursively, stores objects under matching `build/obj/<subsystem>/` paths, and generates
header dependency files with `-MMD -MP`.

The headless suite covers:

- Canonical configuration loading, sparse overlays, promotion, reset, invalid-input
  rejection, profile separation, and legacy migration.
- Guardian rain and Resonance pulse behavior.
- Symmetric out-of-combat regeneration delay/cadence, percentage healing, combat
  interruption, caps, and disable state.
- The rule that no attack, impact, damage, or elimination shakes any user's camera.
- Match-camera distance tuning at the original framing and a live alternate distance,
  including fixed pitch and aim-lead separation.
- Tank self-healing, projectile snapshotting, Shoulder Jets timing/cover behavior, and
  preservation of Charge damage/crate destruction.
- Scrapper Ripsaw/Wrecking Disc outbound and return hits, cover policy, ownership,
  Magnetic Scrap Shell absorption/healing/recharge/break/rearm, and Fight-bot
  threat prediction/release.
- Longshot twin-shot projectile count, combined damage and super gain, tight parallel
  spacing, centered trajectory, and both-bolt hit behavior.
- Longshot Mag-Line Grapple launch/pull timing, attack lock, cooldown, cover endpoint,
  and external-displacement cancellation.
- Mortar Concussion Mine arming, ally filtering, line-of-sight, blast
  damage/knockback, replacement, and owner cleanup.
- External map catalog loading and runtime construction for Helios-9 and Training Court.
- Deterministic replay from identical input frames, including identical game events.
- Isolation between simulation and presentation state.
- UI layout/focus at four viewports, minimum targets, contrast, easing/reduced motion,
  distinct character motifs, result actions, procedural-skin lifetime/policy, profile
  preference round trips, and the shared character-showcase contract.
- Character rig mismatch rejection, bind-relative retargeting math, deterministic GLB
  generation, canonical animation coverage, 1K source/generated texture contracts, and
  presentation-only action-overlay timing.
- Deterministic CC0 VFX atlas generation, recipe coverage, flipbook frame selection,
  fixed-pool priority behavior, all-kit ability/action-event mappings, and semantic rig
  socket metadata. Atlas checks also cover transparent cell guards and removal of black
  RGB mattes from zero-alpha sampling borders.

`make sanitize` rebuilds those tests with AddressSanitizer and
UndefinedBehaviorSanitizer on non-Darwin systems. The current Apple clang/macOS 26 ASan
runtime deadlocks during its own initialization, so the Darwin target runs strict UBSan.
`make check-architecture` rejects forbidden dependencies in `src/game/` and `src/core/`;
it also rejects raw presentation depth-mask toggles that could restore depth writes
before raylib flushes transparent geometry. `make check-ui` enforces shared
text/font/texture ownership and validates the curated UI asset hashes, dimensions,
licenses, sources, and archive policy.
Graphical shaders, input feel, screen transitions, and model animation still require an
interactive run.

## Source organization and dependency direction

```text
brawl_arena/
├── config/                  tracked canonical designer settings
├── data/maps/               versioned map catalog and map packages
├── data/characters/         character model/animation build manifest
├── data/vfx/                curated ability-VFX atlas manifest
├── docs/                    architecture, content, development, import, visual system
├── resources/characters/    tracked mesh-only models and reusable animation libraries
├── resources/environment/   runtime environment assets
├── resources/ui/            curated CC0 interface sources, runtime subset, provenance
├── resources/vfx/           curated CC0 VFX sources and license notices
├── src/
│   ├── core/                limits, shared IDs, deterministic random
│   ├── content/             typed content, config storage, map loading/validation
│   ├── game/                deterministic match simulation and output events
│   ├── app/                 application ownership, input/controller, commands, loop
│   ├── presentation/        assets, menu scene, camera, effects, world rendering
│   ├── ui/                  shared UI system, player-facing menu, and HUD
│   └── devtools/            command center and immediate-mode authoring widgets
├── tests/                   headless behavior, pipeline, and integration checks
└── tools/                   character/UI/VFX asset pipelines and architecture checks
```

The intended dependency direction is implemented, not merely aspirational:

- `core` and `content` define the narrow types consumed by simulation.
- `game` receives a `GameContext` containing only `GameSession`, `Tuning`, and
  `ContentCatalog` pointers. It cannot reach camera, controller, UI, profile, or screen
  state.
- `app` owns the aggregate lifetime and translates device input into `PlayerInput`.
- Simulation emits `GameEvent` records, including stable presentation-only
  `VfxEffectId` values. `presentation/effects.c` consumes them after the simulation
  step.
- UI and developer tools use application commands for gameplay mutations.

`tools/check_architecture.sh` prevents game/core code from importing outer-layer headers,
accepting `App *`, reading keyboard/mouse state, using raylib's nondeterministic random
function, or calling rendering/effect APIs.

## State ownership

`App` owns four distinct runtime regions:

- `GameSession`: arena, brawlers, projectiles, area fields, gems, objective state,
  deterministic random state, simulation clock, statistics, and the event queue.
- `PlayerController`: aim point/distance and main/super charge interaction state.
- `PresentationState`: camera plus fixed pools for particles, animated VFX layers,
  float text, dynamic lights, and shockwaves.
- `AppFlow`: screen, transition, result-banking, and quit state.

It also owns effective `Tuning`, the `ContentCatalog`, and configuration provenance.
`UiPreferences` is an additional application-shell region for profile-only scale,
reduced-motion, contrast, tutorial, and glyph choices.
`Assets` has process lifetime in `main.c` and owns shaders, meshes, textures, character
models, animations, station models, and the scene render target.
The process-lifetime `UiSystem` owns local font handles, the procedural Arena Ink skin,
theme/text/easing services, reference-canvas layout, reduced-motion state, input
modality, and focus graphs.

`ResetMatch()` clears only session, controller, and presentation state. Content,
configuration, profile data, and navigation survive because they have separate owners;
there is no preserve-and-reconstruct whole-application reset.

Fixed capacities currently include eight brawlers, 512 projectiles, 15 typed content
abilities (fourteen currently active), 24 active ability
fields, four statuses per brawler, 40 gems, 1,024 game events, 1,024 presentation
particles, 192 priority-managed VFX layers, 64 float texts, 64 effect lights, and 24
shockwaves. Maps may be up to 64×64, the catalog may contain eight maps, and a map may
contain 64 decorative props.

## Startup and frame flow

Startup:

1. Seed compiled recovery tuning/content.
2. Transactionally load required `config/gameplay.cfg`.
3. Overlay an optional sparse `tuning.local.cfg`, then profile-only `profile.cfg`.
4. Import legacy `tuning.cfg` once when the new local draft does not exist.
5. Load and validate `data/maps/manifest.cfg` and every listed map.
6. Generate procedural fallback assets, then load shaders, optional characters, station
   models, build-generated ability-VFX atlases, local UI fonts, and the procedural Arena
   Ink skin.
7. Build the initial match with the command center closed, then open the main menu.

During an active match:

1. Clamp real delta time and update screen transitions.
2. Capture one `PlayerInput` frame when interaction is allowed.
3. Apply player commands, AI, brawler movement, statuses, cooldowns, projectiles,
   persistent abilities, and arena damage using a deterministic `GameContext`.
4. Resolve out-of-combat health regeneration after projectile/ability damage, then
   update Gem Grab rules.
5. Consume simulation events into presentation pools.
6. Update effects, camera, and presentation-owned HUD feedback transitions.
7. Autosave the local draft/profile when dirty.
8. Render the world, optional post pass, HUD, command center, and transition fade.

During a menu frame, `MenuUpdate()` advances the shared showcase, then
`MenuPrepareDraw()` renders only the current brawler into a window-sized transparent
target before the backbuffer pass. `MenuDraw()` paints the procedural comic backdrop,
typed mechanic motif, and flat vector podium; composites the rounded outlined sticker
through its shader; then draws screen controls, overlays, and the transition fade at
native UI resolution. The wordmark, sticker, and launch rail share one short entrance,
with an immediate reduced-motion resolution.

When Gem Grab ends, player/AI/projectile simulation freezes while effects and the camera
finish presenting the result. The result is banked into the profile once. Continue
returns to the menu, Rematch resets the same selected kit/mode, Change Brawler opens the
roster, and the configured hold still provides an automatic menu fallback.

## Controls and modes

- WASD/arrows or left stick: camera-relative movement.
- Left Shift or left bumper: use the current kit's secondary. Scrapper holds its
  360-degree Magnetic Scrap Shell until released or broken; Longshot holds to preview
  its cover-aware grapple path and releases to launch; Mortar places a mine at its
  feet; Tank fires Shoulder Jets along movement input, or current aim while stationary.
- Hold/release left mouse or right trigger: preview and fire the main attack.
- Tap left mouse or press Space: auto-aim. Guardian first considers a wounded ally in
  range; gamepad A is the same quick action.
- Hold/release right mouse or right bumper: preview and fire a charged ultimate.
- `1`–`5`: change the player's kit.
- Tab: open or close the command center.
- `R`: rebuild the current match without losing tuning.
- Escape: close an overlay, step back a screen, or quit from the main menu.

Play constructs Gem Grab when enabled. Team size is configurable from one to four per
side; slot zero is the human and other player-side slots are allied bots. Teams race to
hold the target gem count for the full countdown. Death drops carried gems.

Practice is a session-only free-form range. On a fresh application launch, whichever
mode is entered first starts with the command center closed. Practice does not rewrite
the saved Play mode; bot behavior can be Static, Roam, or Fight.

## Content and tuning

`ContentCatalog` owns the mutable authoring records, typed character definitions, typed
ability definitions, and validated map definitions. Each character has a stable ID,
display name, model asset ID, role, health/ammo values, handles for its main and ultimate
abilities, and an optional secondary handle. It also owns one project-authorable
showcase transform/camera shared by every character and both menu screens. Ability
behavior is a tagged enum with typed projectile, area, dash, returning-disc, shield,
grapple, or mine payloads. Projectile content can define self-healing from actual
damage; dash content defines duration, speed, knockback, and crate behavior; grapples
define launch/pull timing and range; mines define arm/trigger/blast/damage/knockback.

The current config schema retains `WeaponDef` as a compatibility/authoring record.
`ContentCatalogRebuildTyped()` converts it into the runtime character/ability catalog
after load or live edits. Game, AI, menus, HUD summaries, and aim previews consume the
typed catalog rather than branching over a universal bag of weapon fields.

`config/gameplay.cfg` is the tracked project truth. Runtime layering is:

```text
compiled recovery baseline
        ↓
config/gameplay.cfg       tracked and required
        ↓
tuning.local.cfg          ignored sparse authoring draft
        ↓
profile.cfg               ignored profile-only state
```

Command-center changes apply immediately and autosave after 0.6 seconds. Explicit
`SAVE KIT + SHOWCASE AS PROJECT DEFAULT` and `SAVE ALL AS PROJECT DEFAULTS` actions
validate a full candidate and atomically rewrite the tracked project file. They create
an ordinary Git working-tree change; they do not commit it. Reset actions restore
project values and rewrite the sparse local draft. Missing or malformed canonical
content enables a visible recovery mode rather than silently treating compiled defaults
as project truth. Personal UI preferences and tutorial completion stay in ignored
`profile.cfg` and never enter project defaults.

Global recovery values—delay, pulse interval, and maximum-health ratio—are project
tuning rather than kit content. Their tracked defaults are 3.0 seconds, 1.0 second, and
0.13.

The parser rejects missing, duplicate, unknown, out-of-range, non-finite, or
behavior-inconsistent values transactionally. Automated probes can isolate all four
paths with `BRAWL_PROJECT_CONFIG`, `BRAWL_TUNING`, `BRAWL_PROFILE`, and
`BRAWL_LEGACY_TUNING`.

The tracked project format is version 3. It stores returning-attack values, typed
`secondary.*` values, project-scoped `presentation.match_camera_distance` and
`presentation.render_scale`, and one `preview.showcase.*` camera/transform. Version-1 files are
migrated in memory: Tank mobility becomes a dash secondary, legacy Scrapper weapon
numbers are discarded in favor of Ripsaw/Shell, and the shared showcase is derived from
Scrapper's old home profile. Version-2 typed files migrate `guard` to `shield`, retain
capacity and movement, discard the obsolete arc/hold/counterblast fields, and seed
healing/recharge/break values from current recovery defaults. The next save emits
version 3.

## Maps

`data/maps/manifest.cfg` declares a version, default map ID, and ordered map list. Each
map directory contains:

- `map.cfg`: stable ID/name, dimensions, tile size, and layer filenames.
- `terrain.layer`: floor (`.`), permanent wall (`#`), crate (`c`), or bush (`b`).
- `gameplay.layer`: player spawn (`P`), enemy spawn (`E`), objective vent (`V`), or
  empty (`.`).
- `visual.layer`: palette and decorative cell hints independent of collision.
- `props.cfg`: explicit station-model placement, rotation, scale, palette, and emissive
  strength.

Loading validates versions, dimensions, exact row widths, legal symbols, a sealed wall
border, walkable gameplay markers, at least one spawn per side, exactly one vent,
spawn-to-vent reachability, prop syntax/ranges, duplicate IDs, and the default ID.
Invalid catalogs stop startup rather than creating a partially valid arena.

Helios-9 is the primary full map. Training Court is a second small fixture that proves
the runtime is not hard-coded to Helios-9's 33×23 dimensions or prop layout. The command
center can select the next catalog map and request a match rebuild.

## Combat, events, and camera policy

The simulation uses fixed pools and deterministic xorshift random state. Player input is
captured once per frame, so tests can replay the same input sequence without linking
keyboard or mouse reads into simulation.

Damage, healing, ammo, cooldown, optional secondary abilities, ultimate gain, deaths,
respawns, concealment, dash collision, shield interception, crate damage, returning
projectiles, rain fields, sound-wave status application, and out-of-combat regeneration
all belong to `src/game/`. Brawlers use substepped circle sweeps against permanent walls
and intact crates. Contact preserves tangential movement, removes inward velocity, and
keeps each brawler solid against terrain. Brawlers intentionally do not collide with one
another, so allies and opponents can overlap and pass through freely. The acceleration
response is clamped so slow frames cannot extrapolate beyond authored speed. Bot probes
use the same body radius, with a fixed-capacity breadth-first route over live wall/crate
tiles when the direct sweep is blocked; breaking a crate changes routing on the next
update. Every living
brawler restores 13% maximum health at the three-second quiet mark and once per second
afterward. Successful main/ultimate casts and actual health loss reset combat time;
failed attacks, movement, aiming, Shoulder Jets, and received healing do not.
Regeneration uses the normal capped healing path and never revives. Periodic effects use
generic, team-aware `StatusEffect` slots: the same status heals allies and damages
enemies according to the source team.

Floating damage, healing, regeneration, and shield values enter the event queue only
when the local player index is their source or target. The player therefore sees damage
dealt and received, healing given and received, self-healing, and shield absorption
involving their attacks or body. Bot-only combat values are suppressed before they can
consume float-text presentation slots. Generic non-combat labels, including class-change
and gem feedback, use a separate unfiltered event path.

Tank's tracked main projectiles restore about 49.9% of enemy health actually removed, so
overkill, blocked damage, crates, Charge, and overheal do not create extra sustain.
Shoulder Jets is a non-damaging roughly four-unit boost on a 2.5-second cooldown. It
stops on solid cover; Fight bots use it while closing meaningful gaps or retreating.
Charge remains a separate super that damages, knocks back, and destroys crates.

Longshot's tracked main attack spends one ammo cell to launch two zero-spread bolts on
parallel lanes 0.22 world units apart. Each carries 625 base damage and 0.15 super
charge, preserving the prior 1,250 combined damage and 0.30 combined charge when both
connect; the existing 50%-to-100% travel-distance scaling applies per bolt. Its compiled
recovery baseline follows the same split at 800 per bolt and 1,600 combined. The pair
retains the single-beam aim preview rather than presenting itself as a spread weapon.

Longshot's Mag-Line Grapple is a hold-and-release skill shot. Holding Shift/left bumper
shows the exact cover-aware path and endpoint, a 10-world-unit maximum-range ring, and
amber or red feedback for shortened or invalid paths; releasing a valid aim starts the
7.5-second cooldown. The hook tip travels visibly for 0.25 seconds before Longshot is
pulled to that body-safe endpoint over 0.45 seconds. Walls and crates shorten the
endpoint without taking damage, actors do not block travel, and competing actions are
locked while aiming and during traversal. External displacement cancels the grapple
without refunding cooldown. Fight bots retain direct retreat activation.

Mortar's Concussion Mine is a persistent one-per-owner field placed at Mortar's feet on
an 8-second cooldown. It arms after 0.55 seconds, ignores allies, and requires line of
sight to trigger at 2.4 units. Its 3.2-unit blast deals 400 damage and 4.5 units of
knockback, also respects cover, and grants no super charge. A new placement replaces
the old mine; owner death, class change, and session reset remove it. Fight bots deploy
it against enemies pressing inside the trigger area.

Scrapper's 700-damage Ripsaw travels outward for 13 units, turns at range or solid
cover, and returns to the owner's current position. It may damage each target once per
leg, does not damage crates, and disappears when caught, when its return strikes solid
cover, or when its owner dies or changes class. Wrecking Disc uses the same two-leg
rule at 1,100 damage per leg and 18-unit range; it breaks and passes through crates,
pulls targets toward its line outbound, and knocks them along its travel direction on
return.

Holding Scrapper's Magnetic Scrap Shell raises a 1,200-point, 360-degree bubble at 65%
movement speed and disables main/ultimate attacks. The central damage gateway spends
charge before health for hostile projectiles, area fields, dashes, and periodic damage,
healing Scrapper for 30% of the amount absorbed. Only overflow feeds Tank lifesteal,
while shield contact still preserves existing super-gain, pull, and knockback rules.
Shield hits also interrupt ordinary out-of-combat regeneration.

Releasing preserves charge. After three seconds without shield or health damage it
recharges at 300 points per second. Breaking it forces a five-second lockout, restores
full charge, and requires control release before reactivation. Fight bots raise it for
projectiles predicted within 0.48 seconds and lower it when the threat passes so it can
recharge.

Simulation never spawns particles or mutates camera state. It emits typed events for
muzzle flashes, impacts, explosions, deaths, crate breaks, float text, lights,
shockwaves, particles, stable ability-VFX recipes, and explicitly requested match
presentation. Combat camera shake is disabled for every attack and impact, including
Mortar/thrower attacks; the regression test iterates all five kits and direct
damage/elimination paths.

## Rendering and assets

The presentation layer owns:

- A perspective follow camera with aim lead and a project-scoped 20–60-unit distance.
  The tracked 38.013156-unit default reproduces the original `{0, 31, -22}` offset;
  live distance edits scale that vector without changing pitch.
- Imported/fallback brawler drawing and movement-owned animation selection. Stationary
  casts retain idle as their base and cannot be changed by the independent bush-reveal
  timer. Successful actions emit an explicit presentation-only one-shot that blends an
  optional semantic clip—or a restrained upper-body procedural fallback—over
  locomotion, while facing, muzzle light, projectiles, and ability VFX carry the shot.
  The current twelve-clip libraries have no authored action clips, so current characters
  use the procedural fallback, including Scrapper's braced Shell pose, Longshot's
  reach/brace/tuck Grapple, and Mortar's fallback crouched Mine Deploy. The internal
  optional clip names are `guard`, `grapple`, and `mine_deploy`.
- A flat vector arena podium and non-rotating character previews, retaining idle animation
  without automatic yaw rotation. The live 3D preview renders through a transparent
  target and a shader adds a black contour plus rounded paper sticker keyline using
  sixteen circular directions, an intermediate ring, and softened alpha thresholds.
  Every character and both menu screens use
  the same exact showcase: 180° yaw, 0.90 scale, zero offset, camera
  `(0, 2.7, -7.6)`, target `(0, 1.4, 0)`, and 40° vertical FOV. Swapping candidates
  replaces only the model; the stage clock and background continue uninterrupted.
- The procedural Arena Ink interface: opaque chamfered shapes, thick ink contours,
  paper keylines, offset shadows, bold blue/red/yellow fields, halftone, bursts, speed
  lines, a code-drawn Brawl Arena wordmark, and mechanic-derived saw/crosshair/blast/
  shield/growth character motifs. Short HUD stamps cover KO, ultimate-ready, Shell
  break, downed, and Gem Grab team-lock transitions; results use a full three-action
  poster. The former Kenney/OpenGameArt UI packs remain licensed reference material but
  are not loaded at runtime.
- Body-anchored health bars with their point values centered inside: the player team
  stays green and opponents stay red at every health level, reinforced by distinct
  ally/enemy icons. Scrapper's shield points or broken countdown occupy its separate
  bar, and the redundant bottom-left vitals panel is not drawn.
- External station-map rendering aligned with runtime collision. Wall collision keeps
  its full-tile footprint. Imported straight-wall variants discard their unused
  longitudinal end-cap triangles after loading, so perpendicular textured faces meet
  without competing for the same corner pixels; visible per-cell plinths remain inset
  so adjoining wall tiles also avoid coplanar top surfaces. Station atlases use
  mipmapped trilinear sampling with 8× anisotropy to stabilize oblique wall faces.
- Generated fallback floor, wall, crate, bush, metal, cloth, grass, flat, and glow
  textures.
- Instanced wind/brawler-reactive grass.
- Dynamic point-light selection.
- Projectile, solid-effect, ability-field, and aim-preview visuals, including a
  persistent Grapple cable and solid Mine with authoritative trigger/blast rings.
- A presentation-only 41-recipe ability library built from seven generated CC0
  flipbook/shape atlases, existing particles, lights, and shockwaves. Casts, beams,
  returning saw transitions/catches, Shell start/hit/collapse/restore, Grapple
  fire/hook/pull/land, Mine place/arm/detonate, shoulder jets, healing returns, and
  received effects can follow semantic hand,
  shoulder, chest, foot, or center sockets from the final animated pose; primitive and
  incomplete rigs retain approximate socket positions. Imported recipe layers use a
  shared `4.0×` presentation scale for match-camera readability without changing
  gameplay dimensions or authoritative telegraphs.
- Optional toon/post-processing controls, including outline, bloom, painterly,
  pixelation, halftone, posterization, grade, vignette, temporally stable screen-space
  grain, and chromatic fringe. The project-scoped `presentation.render_scale` ranges
  from 1.0× to 2.0× and defaults to 1.5×, scaling from drawable framebuffer pixels
  rather than logical UI coordinates before the native-resolution HUD. This preserves
  the intended world sampling ratio when those dimensions differ on a HiDPI platform.

Procedural surface/grass generation is isolated in `generated_assets.c`; asset lifetime
and shader/model loading remain in `assets.c`. Ability fields and previews are isolated
in `ability_visuals.c`, camera behavior in `camera.c`, and menu presentation in
`menu_scene.c`. Match clip selection is isolated in `character_animation.c` and reads
physical movement rather than attack/concealment timers. Imported ability recipes live
in `vfx_catalog.c`; `vfx.c` owns their
fixed pool, animation, alpha sorting, ground/beam/billboard drawing, and render-state
restoration. Shared no-depth-write transitions in `render_state.h` flush raylib's
immediate batch before changing and restoring the depth mask, preventing transparent
billboard rectangles from entering the depth-based ink outline. The World command-center
tab reports atlas load state, active/pool/dropped counts, event/layer totals, and the
last recipe, and exposes direct recipe and character action previews for graphical
checks. The window is resizable down to 960×600; the scene/depth target is
recreated after framebuffer-size or render-scale changes settle. The post shader tracks
separate scene-source and drawable-output resolutions so resize, HiDPI scaling, and
supersampling do not distort screen-space effects. A failed scaled allocation retries
at native resolution before the direct-render fallback, and the window requests 4×
MSAA for post-disabled, resize, and failure-fallback frames.

Sentinel, Longshot, Ironclad Guardian, and Gaia Guardian share a compatible 24-joint
hierarchy, so
clips can be reused only while skeleton names, parent relationships, orientations, and
rest pose remain compatible. raylib does not retarget animations. Raw Meshy/Tripo
exports must pass through `tools/fix_meshy_glb.py`; the detailed constraints and import
workflow are in `brawl_arena/docs/CHARACTER_PIPELINE.md`.

## Known limitations and next seams

- Runtime sound effects, ambience, voice, and music are not implemented.
- Locomotion clip changes have no crossfade; explicit action overlays blend in and out
  over the selected locomotion pose.
- Mortar uses a primitive fallback character.
- Character metadata is typed at runtime, but the five stable character IDs/model IDs
  and the legacy authoring schema are still compiled. Adding a sixth slot still requires
  extending those fixed enums/arrays before it can use existing behaviors.
- The command center has application command boundaries, but some controls still edit
  tuning/content values directly before rebuilding the typed catalog.
- Shadows are blob decals; there is no shadow map.
- Full cross-GPU, four-viewport, gamepad, post-effect-extreme, and forced-asset-fallback
  validation remains a manual release matrix.

## Local documentation

- `brawl_arena/README.md`: player-facing behavior and controls.
- `brawl_arena/docs/ARCHITECTURE.md`: ownership and dependency contract.
- `brawl_arena/docs/CONTENT_AND_TUNING.md`: content/catalog/config workflow.
- `brawl_arena/docs/MAPS.md`: map package schema and authoring.
- `brawl_arena/docs/DEVELOPMENT.md`: source layout, adding features, and verification.
- `brawl_arena/docs/CHARACTER_PIPELINE.md`: rigged model/animation conversion.
- `brawl_arena/docs/VFX_PIPELINE.md`: curated effect sources, atlas build, recipes,
  rendering rules, and verification.
- `brawl_arena/docs/visual-design/index.html`: browser-ready Arena Ink runtime
  reference, linked menu/HUD compositions, and preserved pre-implementation audit.
- `brawl_arena/docs/visual-design/IMPLEMENTATION_PLAN.md`: implementation record with
  ownership, file structure, persistence, screen migrations, gates, and delivered scope.
- `brawl_arena/docs/UI_SMOKE_CHECKLIST.md`: graphical viewport, input, character,
  accessibility, and rendering release matrix.
- `brawl_arena/config/README.md`: canonical configuration file format and promotion.

# Reference and generated artifacts

## Root reference image

`is-there-any-game-like-this-really-i-want-to-play-this-v0-w2t8qyln08lc1.webp` is a visual
reference artifact. It is not loaded by the launcher or any of the three games.

## Generated and ignored paths

The root `.gitignore` identifies local outputs:

- `*.o`
- `build/`
- `example_launcher`
- `squad_runner/squad_runner`
- `hearthstone/build/`
- `brawl_arena/tuning.cfg`
- `brawl_arena/tuning.local.cfg`
- `brawl_arena/profile.cfg`
- `logs/`
- `brawl_arena/*.zip`

`brawl_arena/config/gameplay.cfg` is intentionally tracked project source, not generated
per-machine state.

The existing root `example_launcher` binary is already tracked even though its name is in
`.gitignore`. Rebuilding it in place can therefore create a tracked binary diff; use the
`/tmp/example_launcher_verify` command below for ordinary verification.

Do not treat the mere presence of generated files as source changes. Do not delete a
user's tuning, logs, imported archives, or build artifacts unless the task requires it
and the scope is clear.

# Verification matrix

Use the narrowest checks that cover a change.

## Root launcher

```bash
clang -std=c99 -Wall -Wextra example_launcher.c -o /tmp/example_launcher_verify \
  $(pkg-config --cflags --libs raylib) \
  -framework OpenGL -framework Cocoa -framework IOKit
```

For index changes, recalculate:

- Total `.c` files per category.
- Indexed records per category.
- Files present but missing from the index.
- Entries indexed without a corresponding source file.

## Squad Runner

```bash
make -C squad_runner
clang -std=c99 -Wall -Wextra -fsyntax-only \
  $(pkg-config --cflags raylib) squad_runner/src/main.c
```

Interactive checks should cover steering, automatic fire, pickups, squad loss, game over,
and restart.

## Hearthstone

```bash
make -C hearthstone
make -C hearthstone test
make -C hearthstone test_animation
make -C hearthstone test_save_system
```

Interactive checks should match the touched path: default AI, drag/drop play, attacks,
turn changes, editor toggling, or a two-process server/client test.

## Brawl Arena

```bash
make -C brawl_arena
make -C brawl_arena check-architecture
make -C brawl_arena character-assets
make -C brawl_arena check-character-assets
make -C brawl_arena validate-config
make -C brawl_arena test
make -C brawl_arena sanitize
```

For manual runtime/config probes, isolate every layer rather than touching the user's
normal files or tracked project defaults:

```bash
cd brawl_arena
BRAWL_PROJECT_CONFIG=/tmp/brawl-gameplay.cfg \
BRAWL_TUNING=/tmp/brawl-tuning.local.cfg \
BRAWL_PROFILE=/tmp/brawl-profile.cfg \
BRAWL_LEGACY_TUNING=/tmp/brawl-legacy.cfg \
./build/brawl_arena
```

Interactive checks should cover the changed system. For broad gameplay changes, check:

- Menu and screen transitions.
- Practice and Play.
- All five kits, including Scrapper's two-leg saws and held Shell lifecycle, plus
  Guardian rain growth/pulses and Resonance cone HoT/DoT.
- Combat casts, hits, explosions, crate breaks, and eliminations without camera shake.
- Main, super, and secondary previews and state.
- Bush concealment.
- Crate destruction versus permanent walls.
- Bot modes.
- Gem pickup/drop/countdown stamp and objective pulse, plus Continue/Rematch/Change
  Brawler result actions.
- Full restart and menu return.
- Command-center input capture and persistence.
- Closed-by-default command-center entry on a fresh launch plus live match-camera
  distance at both slider endpoints.
- Primitive fallback plus rigged Scrapper, Longshot, Tank, and Guardian rendering.
- Identical home/roster showcase framing, typed character motifs, and rounded sticker
  contour while rapidly changing candidates, without resetting the comic stage.
- Menu entrance/reduced-motion behavior and UI-cue volume/mute plus silent-device
  fallback.

# Documentation maintenance

Update this file in the same change whenever code alters:

- Project inventory or directory ownership.
- Build/run/test commands or dependencies.
- Runtime initialization or frame order.
- Controls or screen flow.
- Gameplay rules, values, capacities, or modes.
- AI behavior.
- Save/config/data formats.
- Asset or character import pipelines.
- Connected versus scaffolded subsystems.
- Known limitations that are fixed or newly introduced.

Prefer precise, testable statements. Do not call a prototype “complete,” a button “live,”
or a subsystem “integrated” unless its current runtime path demonstrates that claim.

Repository-wide agent requirements are defined in `AGENTS.md`.
