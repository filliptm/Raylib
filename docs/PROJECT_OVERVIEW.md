# Project Overview

Last code-verified: 2026-07-27

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
| Brawl Arena | About 10,000 lines across seven source subsystems | Owned `App` state plus deterministic fixed-pool simulation | Cohesive combat vertical slice |

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

The current roster contains five kits:

| Kit | Role | Main attack | Ultimate |
|---|---|---|---|
| Scrapper | damage | short five-pellet spread | nine-pellet crate-breaking Buckshot |
| Longshot | marksman | range-scaled single projectile | piercing Railgun |
| Mortar | artillery | arcing splash shell | three-shell Barrage |
| Tank | tank | short four-pellet burst that self-heals from actual damage, plus Shift Shoulder Jets | damaging crate-breaking Charge |
| Guardian | support | growing rain field that repeatedly damages enemies and heals allies | wide Resonance cone that applies enemy damage-over-time or ally healing-over-time |

Guardian uses `resources/gaia_guardian.glb`. The current tracked Guardian defaults produce
nine 100-point rain pulses over 1.35 seconds and six Resonance ticks over 2.1 seconds.
Those numbers are content values, not hard-coded combat timing.

## Build and verification

From the repository root:

```bash
make -C brawl_arena
make -C brawl_arena run
make -C brawl_arena validate-config
make -C brawl_arena check-architecture
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
- The rule that no attack, impact, damage, or elimination shakes any user's camera.
- Tank self-healing, projectile snapshotting, Shoulder Jets timing/cover behavior, and
  preservation of Charge damage/crate destruction.
- External map catalog loading and runtime construction for Helios-9 and Training Court.
- Deterministic replay from identical input frames, including identical game events.
- Isolation between simulation and presentation state.

`make sanitize` rebuilds those tests with AddressSanitizer and
UndefinedBehaviorSanitizer on non-Darwin systems. The current Apple clang/macOS 26 ASan
runtime deadlocks during its own initialization, so the Darwin target runs strict UBSan.
`make check-architecture` rejects forbidden dependencies in `src/game/` and `src/core/`.
Graphical shaders, input feel, screen transitions, and model animation still require an
interactive run.

## Source organization and dependency direction

```text
brawl_arena/
├── config/                  tracked canonical designer settings
├── data/maps/               versioned map catalog and map packages
├── docs/                    architecture, maps, content, development, character import
├── resources/               runtime GLBs, textures, and environment assets
├── src/
│   ├── core/                limits, shared IDs, deterministic random
│   ├── content/             typed content, config storage, map loading/validation
│   ├── game/                deterministic match simulation and output events
│   ├── app/                 application ownership, input/controller, commands, loop
│   ├── presentation/        assets, camera, effects, ability visuals, world rendering
│   ├── ui/                  player-facing menu and HUD
│   └── devtools/            command center and immediate-mode authoring widgets
├── tests/                   headless behavior and integration checks
└── tools/                   character pipeline and architecture checks
```

The intended dependency direction is implemented, not merely aspirational:

- `core` and `content` define the narrow types consumed by simulation.
- `game` receives a `GameContext` containing only `GameSession`, `Tuning`, and
  `ContentCatalog` pointers. It cannot reach camera, controller, UI, profile, or screen
  state.
- `app` owns the aggregate lifetime and translates device input into `PlayerInput`.
- Simulation emits `GameEvent` records. `presentation/effects.c` consumes them after the
  simulation step.
- UI and developer tools use application commands for gameplay mutations.

`tools/check_architecture.sh` prevents game/core code from importing outer-layer headers,
accepting `App *`, reading keyboard/mouse state, using raylib's nondeterministic random
function, or calling rendering/effect APIs.

## State ownership

`App` owns four distinct runtime regions:

- `GameSession`: arena, brawlers, projectiles, area fields, gems, objective state,
  deterministic random state, simulation clock, statistics, and the event queue.
- `PlayerController`: aim point/distance and main/super charge interaction state.
- `PresentationState`: camera plus fixed pools for particles, float text, dynamic lights,
  and shockwaves.
- `AppFlow`: screen, transition, result-banking, and quit state.

It also owns effective `Tuning`, the `ContentCatalog`, and configuration provenance.
`Assets` has process lifetime in `main.c` and owns shaders, meshes, textures, character
models, animations, station models, and the scene render target.

`ResetMatch()` clears only session, controller, and presentation state. Content,
configuration, profile data, and navigation survive because they have separate owners;
there is no preserve-and-reconstruct whole-application reset.

Fixed capacities currently include eight brawlers, 512 projectiles, 15 typed content
abilities (eleven currently active), 24 active ability
fields, four statuses per brawler, 40 gems, 1,024 game events, 1,024 presentation
particles, 64 float texts, 64 effect lights, and 24 shockwaves. Maps may be up to 64×64,
the catalog may contain eight maps, and a map may contain 64 decorative props.

## Startup and frame flow

Startup:

1. Seed compiled recovery tuning/content.
2. Transactionally load required `config/gameplay.cfg`.
3. Overlay an optional sparse `tuning.local.cfg`, then profile-only `profile.cfg`.
4. Import legacy `tuning.cfg` once when the new local draft does not exist.
5. Load and validate `data/maps/manifest.cfg` and every listed map.
6. Generate procedural fallback assets and load shaders, optional characters, and station
   models.
7. Build the initial match and open the main menu.

During an active match:

1. Clamp real delta time and update screen transitions.
2. Capture one `PlayerInput` frame when interaction is allowed.
3. Apply player commands, AI, brawler state, projectiles, persistent abilities, arena
   damage, and Gem Grab rules using a deterministic `GameContext`.
4. Consume simulation events into presentation pools.
5. Update effects and camera.
6. Autosave the local draft/profile when dirty.
7. Render the world, optional post pass, HUD, command center, and transition fade.

When Gem Grab ends, player/AI/projectile simulation freezes while effects and the camera
finish presenting the result. The result is banked into the profile once, then the shell
returns to the menu after the configured hold or a user skip.

## Controls and modes

- WASD or arrows: camera-relative movement.
- Left Shift: Tank Shoulder Jets along movement input, or current aim while stationary.
- Hold/release left mouse: preview and fire the main attack.
- Tap left mouse or press Space: auto-aim. Guardian first considers a wounded ally in
  range; other cases target the nearest visible enemy.
- Hold/release right mouse: preview and fire a charged ultimate.
- `1`–`5`: change the player's kit.
- Tab: open or close the command center.
- `R`: rebuild the current match without losing tuning.
- Escape: close an overlay, step back a screen, or quit from the main menu.

Play constructs Gem Grab when enabled. Team size is configurable from one to four per
side; slot zero is the human and other player-side slots are allied bots. Teams race to
hold the target gem count for the full countdown. Death drops carried gems.

Practice is a session-only free-form range. It places targets at stepped distances,
opens the command center, and does not rewrite the saved Play mode. Bot behavior can be
Static, Roam, or Fight.

## Content and tuning

`ContentCatalog` owns the mutable authoring records, typed character definitions, typed
ability definitions, and validated map definitions. Each character has a stable ID,
display name, model asset ID, role, health/ammo values, handles for its main and ultimate
abilities, and an optional mobility handle. Ability behavior is a tagged enum with typed
projectile, area, or dash payloads. Projectile content can define self-healing from
actual damage; dash content defines duration, speed, knockback, and crate behavior.

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
`SAVE KIT AS PROJECT DEFAULT` and `SAVE ALL AS PROJECT DEFAULTS` actions validate a full
candidate and atomically rewrite the tracked project file. They create an ordinary Git
working-tree change; they do not commit it. Reset actions restore project values and
rewrite the sparse local draft. Missing or malformed canonical content enables a visible
recovery mode rather than silently treating compiled defaults as project truth.

The parser rejects missing, duplicate, unknown, out-of-range, non-finite, or
behavior-inconsistent values transactionally. Automated probes can isolate all four
paths with `BRAWL_PROJECT_CONFIG`, `BRAWL_TUNING`, `BRAWL_PROFILE`, and
`BRAWL_LEGACY_TUNING`.

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

Damage, healing, ammo, cooldown, optional mobility, ultimate gain, deaths, respawns,
concealment, dash
collision, crate damage, projectiles, rain fields, and sound-wave status application all
belong to `src/game/`. Periodic effects use generic, team-aware `StatusEffect` slots:
the same status heals allies and damages enemies according to the source team.

Tank's main projectiles restore 20% of enemy health actually removed, so overkill,
blocked damage, crates, Charge, and overheal do not create extra sustain. Shoulder Jets
is a non-damaging roughly four-unit boost on a 2.5-second cooldown. It stops on solid
cover; Fight bots use it while closing meaningful gaps or retreating. Charge remains a
separate super that damages, knocks back, and destroys crates.

Simulation never spawns particles or mutates camera state. It emits typed events for
muzzle flashes, impacts, explosions, deaths, crate breaks, float text, lights,
shockwaves, particles, and explicitly requested match presentation. Combat camera shake
is disabled for every attack and impact, including Mortar/thrower attacks; the regression
test iterates all five kits and direct damage/elimination paths.

## Rendering and assets

The presentation layer owns:

- A perspective follow camera with aim lead.
- Imported/fallback brawler drawing and animation selection.
- External station-map rendering aligned with runtime collision.
- Generated fallback floor, wall, crate, bush, metal, cloth, grass, flat, and glow
  textures.
- Instanced wind/brawler-reactive grass.
- Dynamic point-light selection.
- Projectile, solid-effect, ability-field, and aim-preview visuals.
- Optional toon/post-processing controls, including outline, bloom, painterly,
  pixelation, halftone, posterization, grade, vignette, grain, and chromatic fringe.

Procedural surface/grass generation is isolated in `generated_assets.c`; asset lifetime
and shader/model loading remain in `assets.c`. Ability fields and previews are isolated
in `ability_visuals.c`, and camera behavior in `camera.c`.

Sentinel, Ironclad Guardian, and Gaia Guardian share a compatible 24-joint hierarchy, so
clips can be reused only while skeleton names, parent relationships, orientations, and
rest pose remain compatible. raylib does not retarget animations. Raw Meshy/Tripo
exports must pass through `tools/fix_meshy_glb.py`; the detailed constraints and import
workflow are in `brawl_arena/docs/CHARACTER_PIPELINE.md`.

## Known limitations and next seams

- No audio is implemented.
- Character animation changes have no crossfade.
- Longshot and Mortar use primitive fallback characters.
- Character metadata is typed at runtime, but the five stable character IDs/model IDs
  and the legacy authoring schema are still compiled. Adding a sixth slot still requires
  extending those fixed enums/arrays before it can use existing behaviors.
- The command center has application command boundaries, but some controls still edit
  tuning/content values directly before rebuilding the typed catalog.
- Rendering uses a fixed 1280×800 window and animation time comes from rendered frame
  time.
- Shadows are blob decals; there is no shadow map.
- Graphical startup and interaction are manually verified rather than automated.

## Local documentation

- `brawl_arena/README.md`: player-facing behavior and controls.
- `brawl_arena/docs/ARCHITECTURE.md`: ownership and dependency contract.
- `brawl_arena/docs/CONTENT_AND_TUNING.md`: content/catalog/config workflow.
- `brawl_arena/docs/MAPS.md`: map package schema and authoring.
- `brawl_arena/docs/DEVELOPMENT.md`: source layout, adding features, and verification.
- `brawl_arena/docs/CHARACTER_PIPELINE.md`: rigged model/animation conversion.
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
- All five kits, including Guardian rain growth/pulses and Resonance cone HoT/DoT.
- Combat casts, hits, explosions, crate breaks, and eliminations without camera shake.
- Main and super previews.
- Bush concealment.
- Crate destruction versus permanent walls.
- Bot modes.
- Gem pickup/drop/countdown/result.
- Full restart and menu return.
- Command-center input capture and persistence.
- Primitive fallback plus rigged Scrapper, Tank, and Guardian rendering.

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
