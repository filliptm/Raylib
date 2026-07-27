# Project Overview

Last code-verified: 2026-07-26

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
| Brawl Arena | About 7,350 lines of C and headers across focused modules | Fixed-pool `World` passed through gameplay systems | Cohesive combat vertical slice |

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

## Purpose and current role

`brawl_arena/` is a top-down 3D arena brawler in the Brawl Stars mold. It began as a
combat-feel sandbox and now includes a menu shell, character selection, practice mode,
Gem Grab, allied and enemy AI, concealment, destructible cover, a live command center,
procedural rendering, post-processing, profile statistics, and one rigged character.

It is the repository's most recently developed and most cohesive original game.

## Build and run

```bash
make -C brawl_arena
make -C brawl_arena run
```

The executable is `brawl_arena/build/brawl_arena`.

For a warning-focused source check:

```bash
clang -std=c99 -Wall -Wextra -fsyntax-only \
  $(pkg-config --cflags raylib) brawl_arena/src/*.c
```

There is currently no automated gameplay test suite.

## Application shell

The game opens a 1280×800 MSAA window and disables raylib's default Escape-to-close
behavior so Escape can mean “back.”

The shell has:

- Main menu.
- Brawler selection.
- Match screen.
- Fade-out/swap/fade-in transitions.

The menu displays profile statistics, the selected kit, kit-derived statistics, controls,
mode selection, Practice, Play, Brawlers, and Quit. Menu actions mutate real state rather
than being decorative placeholders.

## Startup and frame flow

Startup:

1. Reset the mutable weapon table to defaults.
2. Populate built-in tuning defaults.
3. Load `tuning.cfg` if present.
4. Generate/load assets and shaders.
5. Initialize menu assets.
6. Build an initial match.
7. Show the main menu.

During a match, each frame:

1. Clamp real frame delta to 0.05 seconds.
2. Update screen transitions.
3. Apply the configured time scale.
4. Update the command center.
5. Handle full-match restart requests.
6. Process player input when the match is active and unlocked.
7. Update AI, brawler movement/state, and projectiles.
8. Update destructible arena tiles.
9. Update Gem Grab spawning, pickups, scoring, and result state.
10. Update effects and camera.
11. Bank match statistics once.
12. Autosave changed tuning.
13. Render the world, optional post pass, HUD, command center, and screen fade.

When Gem Grab is decided, AI, movement, and projectiles stop. Effects and the camera keep
running while the result is shown, after which the shell returns to the menu.

## World state and capacity

`World` owns the active simulation:

- `Arena`.
- Up to eight `Brawler` values.
- Up to 512 projectiles.
- Up to 1,024 particles.
- Floating text, dynamic effect lights, and shockwaves.
- Up to 40 gems.
- Match, camera, aim-preview, statistics, tuning, and screen state.

These are fixed pools. Gameplay avoids per-frame heap allocation and finds inactive slots
when spawning temporary objects.

Most gameplay functions receive `World *`, but the application is not entirely free of
globals:

- `main.c` owns global `World` and `Assets` values.
- `WEAPONS[]` is a mutable global table.
- Menu and command-center widgets keep static UI state.
- Rendering keeps static asset and grass/light buffers.

## Match construction

`ResetMatch()` preserves application-level tuning and screen-transition state, zeroes the
match world, reloads the arena, and spawns the correct roster.

Gem Grab:

- Team size is configurable from one to four per side.
- Slot zero on the player team is the human.
- Remaining player slots are allied bots.
- The enemy team uses bot-controlled kits.
- Each brawler receives a stable spawn slot.

Free-form/practice:

- The human occupies index zero.
- A configurable number of enemy bots fills later indices.
- Practice targets are fanned out at stepped ranges in front of the player.
- Practice spawn locations are nudged away from walls and bushes.

After spawning, the match state, grass instances, and camera are reset.

## Arena

The arena is a 33×23 character grid in `src/arena.c`. Each tile is two world units.

Map symbols:

- `#`: permanent metal wall.
- `c`: destructible crate.
- `b`: bush/grass concealment tile.
- `P`: player-team spawn.
- `E`: enemy-team spawn.

The parser pads short rows and forces a solid border. It records three spawn locations per
side and places the Gem Grab vent at the center.

Arena responsibilities:

- Circle-versus-grid movement resolution.
- Solid and bush queries.
- Sampled line of sight.
- Direct and radius damage to crates.
- Destruction animation/state.

Permanent walls do not take damage. “Breaks walls” in some UI/README text generally means
that a super destroys crates; it does not remove the permanent metal wall tiles.

## Brawlers and movement

Each brawler stores:

- Team, class, position, velocity, and facing.
- Health and maximum health.
- Three-unit fractional ammo state.
- Attack cooldown.
- Super charge.
- Alive, death, respawn, reveal, bush, and visibility state.
- Dash state.
- Gem count.
- AI target, steering, retreat, strafe, and wander state.

Movement intent is converted to a desired velocity and interpolated using the live movement
acceleration tuning. Arena collision and brawler separation are applied afterward.

There is no passive health regeneration. Death drops all carried gems, plays effects,
increments relevant statistics, and starts a configurable respawn timer.

## Player input

- WASD or arrows: camera-relative movement.
- Hold left mouse: aim the main attack and show its preview.
- Release left mouse: fire.
- Quick-tap left mouse: auto-aim at the nearest visible enemy with motion leading.
- Space: quick shot at the nearest visible enemy without the same lead calculation.
- Hold/release right mouse: aim and fire a charged super.
- `1`–`4`: swap kit in place.
- Tab: open or close the command center.
- `R`: rebuild the current match while preserving tuning.
- Escape: close overlays or return through the screen hierarchy.

Clicks over the command center are captured so UI interaction does not fire a weapon.

## Ammo, attacks, and supers

Every kit has three ammo units. Ammo refills continuously at the kit's reload-per-ammo
rate. A main attack requires at least one ammo unit and its cooldown to be ready.

Landing projectiles grants super charge according to `superPerHit` and the global super
gain multiplier. A takedown grants an additional charge bonus. Firing a main attack
reveals the brawler for the configured duration.

Weapon and super values live in:

- Immutable baseline: `WEAPON_DEFAULTS[]`.
- Mutable live values: `WEAPONS[]`.

The command center edits the live table and can restore one kit or all kits.

## Four kits

### Scrapper

- 3,800 default health.
- Five-pellet, 320-damage spread attack.
- Short range and wide spread.
- Super fires nine stronger pellets.
- Super projectiles can destroy crates and continue through the destroyed cover.

Scrapper is the only kit that can currently use the imported rigged character model.

### Longshot

- 2,800 default health.
- One fast, long-range projectile.
- Listed damage is 1,600 at maximum travel.
- Damage scales from 50% at point-blank to 100% at maximum range.
- Super is a piercing rail shot that can hit multiple enemies before permanent cover.

Older documentation says damage reaches 2×. The implemented formula is 0.5×–1.0× of the
configured damage.

### Mortar

- 3,200 default health.
- Arcing shell with splash damage.
- The projectile ignores mid-flight cover and detonates at its landing point.
- Splash falls toward 55% damage at the edge.
- Super launches a three-shell barrage.

Arcing supers damage crates in their impact radius.

### Tank

- 5,600 default health.
- Four-pellet, very-short-range attack.
- Super is a directional charge.
- The charge damages and knocks back each enemy once, smashes crates, and stops at
  permanent walls.

## Projectile simulation

Projectile slots record owner, team, direction, speed, range, damage, radius, spread-derived
trajectory, super behavior, piercing, arcing, and wall-breaking state.

Normal projectile motion is substepped in increments of at most 0.3 world units to reduce
tunneling. Cover collision is evaluated before actor collision. Arcing shells instead
follow a visual arc to a fixed endpoint and resolve splash on landing.

Spread pellets are evenly distributed across the configured angle and receive an
additional random scatter of up to 6% of that angle.

## Aim previews

The renderer converts live weapon data into ground previews:

- Spread weapons: a filled cone with separately raycast ribs.
- Single projectiles: a thick beam clipped to the center-line obstruction.
- Mortars: landing discs and dotted arcs.
- Supers: gold treatment.
- Main attacks: blue treatment.

The previews are highly informative but not mathematically exact in every case:

- Random firing scatter can land outside the deterministic fan ribs.
- The thick single-shot beam does not independently raycast both visual edges.
- Permanent and destructible cover use the current arena ray tests.

## Concealment

A brawler in a bush is hidden from opponents unless:

- It has an active reveal timer after firing.
- An opponent moves within the configured reveal distance.

`BrawlerCanSee()` also requires arena line of sight. AI targeting uses this function.

The later player-facing `visible` calculation uses proximity for bush reveal without
rechecking line of sight. Consequently, an enemy in a bush can become rendered at close
range even if a permanent wall lies between it and the player. This is a known consistency
issue between AI sight and rendering sight.

Concealed characters use screen-door dithering with a green cast rather than alpha
blending. The local player also receives a locator ring drawn after grass so the player
does not lose track of their own position.

## AI

Bot modes:

- `STATIC`: no movement or fire.
- `ROAM`: wander without combat.
- `FIGHT`: target, steer, strafe, fire, retreat, use supers, and collect gems.

Gem Grab forces combat behavior regardless of the free-form bot-mode setting.

Combat AI:

- Selects the nearest visible opponent.
- Predicts projectile travel and leads the target.
- Adds randomized aim error.
- Moves toward a fraction of weapon range.
- Strafes while fighting.
- Retreats toward a nearby bush below 30% health.
- Uses a super when the target is plausibly in range.
- Diverts toward loose gems when appropriate.

There is no A* or navigation mesh. Steering samples nearby alternatives and relies on
collision resolution. Bots do not explicitly coordinate around carriers, defend a
countdown leader, escort allies, or choose team-level strategies.

## Gem Grab

The central vent produces its first gem after two seconds and then uses the configured
interval.

Loose gems:

- Arc out of the vent or a defeated carrier.
- Settle into a hover.
- Have a short pickup grace period.
- Are collected within a fixed radius.

Team scores count gems carried by living brawlers. A defeated carrier's count is set to
zero before its gems become loose, so dropped gems do not count for either team.

A countdown begins when one team:

- Has at least the configured target number of gems.
- Has a strict lead over the other team.

Losing that lead resets the countdown rather than pausing it. When the countdown reaches
zero, the match records a winner, clears projectiles, stops brawler motion, plays result
effects, and later returns to the menu. Wins, losses, and player KOs are banked exactly
once into tuning/profile state.

## Command center

Tab opens an immediate-mode tuning panel with tabs for:

- Match rules.
- Bots.
- Player movement and cheats.
- Current kit and super.
- Rendering style.
- World, grass, debug, and reset tools.

Most sliders write directly into `World.tune` or `WEAPONS[]` and take effect immediately.
Rule changes that require a rebuilt roster set `matchRestartPending`.

Current command-center issues:

- `SetBotCount()` assumes every brawler after index zero is an enemy. In Gem Grab, indices
  after zero also contain allied bots. Bot-count changes can truncate the team roster.
- “Respawn all bots” respawns all non-player brawlers as enemies, including allies.
- “Reset ALL tuning” calls the same free-form bot-count helper and can corrupt a live
  Gem Grab roster.
- “Restart match” calls `MatchReset()` only. It resets gems and match timers, not arena
  cover, brawler positions/health, or projectiles like the `R` rebuild does.
- Changing Active Kit through the panel respawns the player and preserves super charge,
  but not carried gems or spawn-slot state.
- The `superPerHit` value is 0–1 but is displayed with `%.0f%%`, producing a misleading
  `0%` or `1%` rather than a scaled percentage.

Treat these as real implementation constraints when tuning or extending Gem Grab.

## Tuning persistence

`Tuning` includes movement, concealment, respawn, time scale, cheats, debug rendering,
post-processing, profile statistics, Gem Grab rules, grass, and bot settings.

`config.c` builds one table of pointers to:

- All tuning fields.
- Fourteen mutable weapon fields for each of the four kits.

It saves that table as plain `key value` text. Changes are autosaved after 0.6 seconds and
flushed during relevant screen transitions. Unknown and missing keys are ignored.

`BRAWL_TUNING` can override the file path for isolated tests or tools:

```bash
cd brawl_arena
BRAWL_TUNING=/tmp/brawl-test-tuning.cfg ./build/brawl_arena
```

The normal ignored path is `brawl_arena/tuning.cfg`. The current local file has Gem Grab
disabled and two bots, so the checkout does not presently launch Play with the built-in
3v3 Gem Grab defaults.

Load validation clamps many important values but not every hand-editable weapon or visual
field. A malformed file cannot break the principal counts and minimum timings, but manual
negative damage/radius/spread-style values are not comprehensively sanitized.

## Rendering pipeline

The world renderer draws:

1. Arena floor and cover.
2. Brawlers and character models.
3. Instanced grass.
4. Local-player locator.
5. Gems.
6. Solid effects and debris.
7. Aim previews, shockwaves, projectiles, and debug overlays.

HUD and command-center UI are drawn after post-processing so text remains crisp.

### Procedural assets

`assets.c` generates:

- Floor, wall, crate, bush/grass, metal, cloth, and glow textures.
- Primitive unit meshes.
- Lighting, grass, skinning, and post-processing shaders.
- Render targets and the optional depth texture used for outlines.

Bush concealment is visually represented by a dark ground tile and instanced grass. A
generated `texBush` texture exists but is not currently used by rendering.

### Lighting

The main lighting shader uses half-Lambert diffuse, optional Blinn-style highlights, rim
light, and fog. Toon mode quantizes illumination into bands. Each frame, the renderer
selects a limited number of the strongest dynamic effect lights.

### Grass

Grass uses one instanced crossed-quad mesh. Static transforms are built per bush tile.
The vertex shader provides:

- Position-phased wind.
- Height-weighted bending.
- Displacement away from visible brawlers.
- Travel-direction bias.

Hidden enemies are left out of the grass actor list so grass movement does not reveal
their exact location.

### Post-processing

The optional full-screen post shader combines:

- Bright-neighborhood bloom.
- Painterly quadrant filtering.
- Pixelation.
- Chromatic aberration.
- Halftone.
- Posterization.
- Saturation and brightness.
- Grain.
- Vignette.
- Depth-assisted outlines when the custom depth target is available.

The render target is fixed to the 1280×800 window. The window is not resizable.

## Rigged-character asset

`resources/sentinel.glb` is an optional Scrapper model. If loading or skinning fails, the
game falls back to the primitive brawler without preventing startup.

The current runtime GLB is approximately 4.53 MB and contains:

- One mesh and one material.
- One 24-joint skin.
- Thirteen named animation clips.
- Embedded PNG texture data.

Directional movement, idle/combat, hit, and death clips are resolved by normalized name
matching in `assets.c`. Raylib does not provide animation crossfading here, so clip changes
can visibly restart animation cycles.

`tools/fix_meshy_glb.py` converts raw Meshy exports into a raylib-compatible file by:

- Reconstructing the rest pose from inverse bind matrices.
- Baking problematic uniform scale into vertices.
- Removing non-joint transforms that raylib would compose inconsistently.
- Adding constant filler channels.
- Merging animation clips.
- Optionally downscaling oversized textures with Pillow.

The detailed contract and import procedure are in
`brawl_arena/docs/CHARACTER_PIPELINE.md`.

The large source ZIP archives are local import material and ignored. The repacked GLB is
the tracked runtime asset.

## Documentation drift in the local Brawl README

The README contains valuable design reasoning but combines multiple development eras.
Known mismatches include:

- It calls the project a no-objective combat slice before later documenting Gem Grab.
- The final Known Gaps section still says there is no win condition.
- It says characters are primitives after documenting the rigged Sentinel.
- It says the GLB has 12 clips and is 1.7 MB; it has 13 clips and is about 4.53 MB.
- It says Longshot reaches 2× damage; implemented scaling reaches the configured 1× value.
- It says there is no hidden global state; several application/UI/render globals exist.
- It says every tuning control applies next frame; roster and rule changes can require a
  rebuild, and some current helpers are unsafe for Gem Grab.
- Escape appears twice in the controls table with conflicting descriptions.

Keep the README's design explanations where they remain useful, but use this overview and
the code for current-state decisions.

## Known technical risks

- Command-center roster operations are not team-aware.
- Rendering and AI disagree on line-of-sight requirements for proximity bush reveal.
- Movement calls `Lerp` with `moveAccel * dt` without clamping the factor. Low frame rates
  or high time scale can cause velocity overshoot.
- The preview is not exact for random spread and thick beam edges.
- Gems are silently not spawned if the fixed gem pool is exhausted.
- Some configuration fields are only partially clamped on manual load.
- Animation advances from `GetFrameTime()` during rendering rather than the simulation
  delta passed through the update loop.
- No audio.
- No deterministic gameplay, config-roundtrip, arena, projectile, AI, or match-rule tests.
- Fixed 1280×800 presentation and no resize path.

## Good next engineering steps

1. Make command-center bot operations team-aware and distinguish free-form controls from
   Gem Grab roster controls.
2. Unify visibility behind one line-of-sight-and-concealment predicate.
3. Add headless deterministic tests for arena collision, weapon damage, Gem Grab
   countdowns, respawn/gem drops, and config roundtrips.
4. Clamp movement interpolation and audit time-scale interactions.
5. Make preview geometry share the exact collision/spread calculations used by firing.
6. Add audio and animation blending only after simulation correctness is protected.
7. Reconcile or replace the project-local README so it describes one current product.

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
- `logs/`
- `brawl_arena/*.zip`

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
clang -std=c99 -Wall -Wextra -fsyntax-only \
  $(pkg-config --cflags raylib) brawl_arena/src/*.c
```

Use `BRAWL_TUNING` for isolated runtime/config tests rather than overwriting the user's
normal tuning:

```bash
cd brawl_arena
BRAWL_TUNING=/tmp/brawl-verification.cfg ./build/brawl_arena
```

Interactive checks should cover the changed system. For broad gameplay changes, check:

- Menu and screen transitions.
- Practice and Play.
- All four kits.
- Main and super previews.
- Bush concealment.
- Crate destruction versus permanent walls.
- Bot modes.
- Gem pickup/drop/countdown/result.
- Full restart and menu return.
- Command-center input capture and persistence.
- Primitive fallback and rigged Scrapper rendering.

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
