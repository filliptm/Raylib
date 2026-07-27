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
| Brawl Arena | About 9,400 lines of C and headers across focused modules | Fixed-pool `World` passed through gameplay systems | Cohesive combat vertical slice |

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
procedural rendering, an imported modular station environment, post-processing, profile
statistics, and optional rigged characters for selected kits.

It is the repository's most recently developed and most cohesive original game.

## Build and run

```bash
make -C brawl_arena
make -C brawl_arena run
make -C brawl_arena validate-config
make -C brawl_arena test
```

The executable is `brawl_arena/build/brawl_arena`.

For a warning-focused source check:

```bash
clang -std=c99 -Wall -Wextra -fsyntax-only \
  $(pkg-config --cflags raylib) brawl_arena/src/*.c
```

The headless tests validate the canonical configuration and its migration/overlay/save
semantics, Guardian rain and Resonance timing, and the rule that combat never drives
camera shake. Graphical input, rendering, and full match flow still require interactive
checks.

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

1. Seed built-in recovery tuning and weapon values.
2. Transactionally validate and load tracked `config/gameplay.cfg` as the canonical
   project configuration.
3. Overlay sparse ignored `tuning.local.cfg`, then profile-only `profile.cfg`.
4. If the new local draft is absent, import an existing legacy `tuning.cfg` into the
   split local/profile files without modifying the legacy file.
5. Generate/load shaders and procedural assets, then load optional character and station
   GLBs.
6. Initialize menu assets.
7. Build an initial match from the effective configuration.
8. Show the main menu.

If the canonical file is missing or invalid, startup retains playable compiled recovery
values and labels the command center `CONFIG RECOVERY MODE`; it does not silently treat
those values as the normal project source.

During a match, each frame:

1. Clamp real frame delta to 0.05 seconds.
2. Update screen transitions.
3. Apply the configured time scale.
4. Update the command center.
5. Handle full-match restart requests.
6. Process player input when the match is active and unlocked.
7. Update AI, brawler movement/state, projectiles, and persistent ability fields.
8. Update destructible arena tiles.
9. Update Gem Grab spawning, pickups, scoring, and result state.
10. Update effects and camera.
11. Bank match statistics once.
12. Autosave changed draft/profile fields without changing project defaults.
13. Render the world, optional post pass, HUD, command center, and screen fade.

When Gem Grab is decided, AI, movement, and projectiles stop. Effects and the camera keep
running while the result is shown, after which the shell returns to the menu.

## World state and capacity

`World` owns the active simulation:

- `Arena`.
- Up to eight `Brawler` values.
- Up to 512 projectiles.
- Up to 24 rain/sound-wave ability fields.
- Up to 1,024 particles.
- Floating text, dynamic effect lights, and shockwaves.
- Up to 40 gems.
- Match, camera, aim-preview, statistics, effective tuning, configuration provenance, and
  screen state.

These are fixed pools. Gameplay avoids per-frame heap allocation and finds inactive slots
when spawning temporary objects.

Most gameplay functions receive `World *`, but the application is not entirely free of
globals:

- `main.c` owns global `World` and `Assets` values.
- `WEAPONS[]` is a mutable global table.
- Menu and command-center widgets keep static UI state.
- Rendering keeps static asset and grass/light buffers.

## Match construction

`ResetMatch()` preserves application-level tuning, configuration provenance, and
screen-transition state, zeroes the match world, reloads the arena, and spawns the
correct roster.

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

The current Helios-9 layout is vertically mirrored and keeps the 33×23 footprint. Short
walls shield the top and bottom docking-bay spawns without forming lane dividers. Broad
movement bands connect both flanks and the centre, while four small L-shaped pylon groups,
hydroponic concealment pockets, and destructible cargo/operations banks provide deliberate
hold and rotation points. The centre-back `P` appears before the two side spawns so player
slot zero remains the human.

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
- Per-kit fractional ammo state; the tracked defaults currently give every kit three.
- Attack cooldown.
- Super charge.
- Alive, death, respawn, reveal, bush, and visibility state.
- Dash state.
- Resonance heal-over-time/damage-over-time mark state.
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
- Quick-tap left mouse: auto-aim with motion leading. Guardian prioritizes a badly hurt
  ally in range, then falls back to the nearest visible enemy.
- Space: quick shot using the same support/enemy target priority.
- Hold/release right mouse: aim and fire a charged super.
- `1`–`5`: swap kit in place.
- Tab: open or close the command center.
- `R`: rebuild the current match while preserving tuning.
- Escape: close overlays or return through the screen hierarchy.

Clicks over the command center are captured so UI interaction does not fire a weapon.

## Ammo, attacks, and supers

Ammo capacity and reload time are per-kit project values. The tracked defaults currently
give every kit three ammo units. Ammo refills continuously at the kit's reload-per-ammo
rate. A main attack requires at least one ammo unit and its cooldown to be ready.

Landing damaging projectiles grants super charge according to `superPerHit` and the global
super gain multiplier. Guardian rain pulses grant charge when they damage an enemy or
restore nonzero health to an ally. Resonance ticks do not recharge Resonance. A takedown
grants an additional charge bonus. Firing a main attack reveals the brawler for the
configured duration.

`config/gameplay.cfg` is the normal starting source for weapon and super values.
`WEAPON_DEFAULTS[]` is a compiled recovery baseline, while `WEAPONS[]` is the mutable
effective table used by simulation. The command center edits the effective table and can
restore or promote one kit or all project-scoped values.

No attack causes camera shake. This includes main/super casts, direct player damage,
projectile explosions, crate destruction, and eliminations for every kit. The remaining
camera-shake calls are match-state presentation cues, not combat feedback.

## Five kits

### Scrapper

- 3,800 default health.
- Five-pellet, 320-damage spread attack.
- Short range and wide spread.
- Super fires nine stronger pellets.
- Super projectiles can destroy crates and continue through the destroyed cover.

Scrapper uses the imported Sentinel model when rigged character models are enabled.

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
- Uses the imported Ironclad Guardian model when rigged character models are enabled.

### Guardian

- 3,400 default health.
- The main attack places a rain field up to 17 units away.
- The field grows to a 3.4-unit radius during its first 44% of life and disappears after
  1.35 seconds.
- It pulses immediately and every 0.15 seconds for nine total pulses. A target that
  remains inside for the entire cast receives 900 total damage as an enemy or up to 900
  total healing as an ally, in 100-point increments.
- Allies at full health remain valid field occupants but gain no health or super charge
  from that pulse. The caster can be healed when standing inside their own rain.
- Quick auto-aim and combat AI prioritize sufficiently injured teammates.
- `RESONANCE` is a 14-unit, 90-degree sound-wave cone, clipped by arena line of sight.
- Every living non-caster in the cone is marked for 2.1 seconds. Allies receive six
  220-point healing ticks (up to 1,320 total); enemies receive six 180-point damage ticks
  (1,080 total). The mark does not revive defeated allies.
- The travelling cone is visible for 0.7 seconds, and marked characters retain a green
  healing or pink damage aura until the timed effect ends.
- Guardian uses the imported Gaia Guardian model when rigged character models are enabled.

## Projectile and ability-field simulation

Projectile slots record owner, team, direction, speed, range, damage, ally healing, radius,
spread-derived trajectory, super behavior, piercing, arcing, and wall-breaking state.

Normal projectile motion is substepped in increments of at most 0.3 world units to reduce
tunneling. Cover collision is evaluated before actor collision. Arcing shells instead
follow a visual arc to a fixed endpoint and resolve splash on landing.

Spread pellets are evenly distributed across the configured angle and receive an
additional random scatter of up to 6% of that angle.

`World.abilityFields` is a separate fixed pool of 24 persistent area records. Rain fields
own their growth, lifetime, pulse timer, team, owner, damage, and healing. Each pulse
checks all living brawlers against the field's current radius. Sound-wave fields own only
the short cast visualization: the cast performs a range, angle, and line-of-sight test
once, then copies its timed Resonance mark onto each target.

The Resonance mark lives on `Brawler`, so its ticks continue after the visible cone is
gone and follow a moving target. A new mark refreshes and replaces the existing one.
Rain fields and marks are cleared when Gem Grab ends so paused gameplay cannot leave
frozen effects on the result screen.

## Aim previews

The renderer converts live weapon data into ground previews:

- Spread weapons: a filled cone with separately raycast ribs.
- Single projectiles: a thick beam clipped to the center-line obstruction.
- Mortars: landing discs and dotted arcs.
- Guardian main attack: green target disc at the clamped rain-field center.
- Guardian Resonance: wide cyan cone with separately raycast ribs.
- Offensive supers: gold treatment.
- Ordinary main attacks: blue treatment.
- Guardian support previews: green treatment.

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
- Guardian bots interrupt combat to place rain on teammates below 85% health. They aim
  Resonance toward a nearby ally below 60% health, and can also cast it offensively when
  an enemy is in cone range.
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
zero, the match records a winner, clears projectiles, ability fields, and Resonance marks,
stops brawler motion, plays result effects, and later returns to the menu. Wins, losses,
and player KOs are banked exactly once into tuning/profile state.

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

The panel is vertically scrollable per tab. Its header reports whether the effective
values are exactly `PROJECT DEFAULTS`, include a numbered local draft, or are running in
configuration recovery mode. The Kit tab can discard/promote the current kit. The World
tab reports status and can discard/promote all project-scoped values. A project save
rewrites the tracked configuration but deliberately does not create a Git commit.

Current command-center issues:

- `SetBotCount()` assumes every brawler after index zero is an enemy. In Gem Grab, indices
  after zero also contain allied bots. Bot-count changes can truncate the team roster.
- “Respawn all bots” respawns all non-player brawlers as enemies, including allies.
- “Restart match” calls `MatchReset()` only. It resets gems and match timers, not arena
  cover, brawler positions/health, or projectiles like the `R` rebuild does.
Treat these as real implementation constraints when tuning or extending Gem Grab.

## Tuning persistence

`config.c` exposes one typed field registry over `Tuning` and all five `WeaponDef`
records. It assigns each stable dotted key a type, scope, range, and optional kit owner.
Main and super behavior use named kinds (`projectile`, `lob`, `rain`, `dash`,
`healing_burst`, and `sound_wave`) rather than persisting implementation booleans.

The layers are:

1. Compiled recovery values from `TuningSetDefaults()` and `WEAPON_DEFAULTS[]`.
2. Required tracked `config/gameplay.cfg`, containing every project-scoped gameplay, AI,
   match, kit, and default presentation value.
3. Sparse ignored `tuning.local.cfg`, containing project fields only while they differ
   from the canonical project plus local-only cheats/debug fields.
4. Ignored `profile.cfg`, containing selected kit and win/loss/KO statistics.

Command-center changes apply immediately. Dirty local/profile state autosaves after 0.6
seconds and flushes on shutdown. Explicit Kit/World save buttons first validate a complete
candidate, then atomically replace `config/gameplay.cfg`; they do not run Git commands.
Reset/discard actions copy from the in-memory canonical snapshot and clean matching draft
keys.

Canonical loading rejects missing, duplicate, unknown, mistyped, non-finite,
out-of-range, and semantically inconsistent values. Optional overlay loading is also
transactional: a rejected draft or profile reports an actionable status without partially
mutating effective state. Writes are deterministic `key value` text through a temporary
file and rename.

When the new local file is absent, a version-1/version-2 `tuning.cfg` is imported once
into the split draft/profile files and preserved. Version-1 Guardian bolt/Sanctuary values
reset to current project semantics because they cannot describe rain and Resonance.

All four config paths can be overridden for isolated tests or tools:

```bash
cd brawl_arena
BRAWL_PROJECT_CONFIG=/tmp/gameplay.cfg \
BRAWL_TUNING=/tmp/tuning.local.cfg \
BRAWL_PROFILE=/tmp/profile.cfg \
BRAWL_LEGACY_TUNING=/tmp/legacy.cfg \
./build/brawl_arena
```

The detailed schema, authoring workflow, and isolation rules are in
`brawl_arena/config/README.md`.

## Rendering pipeline

The world renderer draws:

1. Helios-9 deck, collision-aligned cover, hydroponic beds, and exterior set dressing.
2. Brawlers and character models.
3. Instanced grass.
4. Local-player locator.
5. Gems.
6. Solid effects and debris.
7. Ability fields and timed-mark auras.
8. Aim previews, shockwaves, projectiles, and debug overlays.

HUD and command-center UI are drawn after post-processing so text remains crisp.

### Procedural and static environment assets

`assets.c` generates:

- Floor, wall, crate, bush/grass, metal, cloth, and glow textures.
- Primitive unit meshes.
- Lighting, grass, skinning, and post-processing shaders.
- Render targets and the optional depth texture used for outlines.

The generated floor remains the low-draw-count gameplay grid and the generated wall/crate
meshes remain fallbacks. `environment.c` layers Kenney Space Station Kit models over that
foundation:

- Permanent tiles retain a full collision-aligned structural core. Exposed sides receive
  wall, window, door, banner, or console faces, and imported panels close their tops.
- Destructible tiles use containers in the cargo half and computer/power banks in the
  operations half. Their health tint, hit flash, and disappearance follow the tile state.
- Bush tiles use low station panels as hydroponic beds under the existing grass.
- Spawn pads, reactor floor details, supports, pipes, rails, and exterior work platforms
  are presentation only and do not add simulation collision.
- Shared height constants keep the base deck, imported floor inlays, passive decals, and
  active targeting/effect overlays on distinct planes. This prevents coplanar station
  geometry, shadows, glows, and aim shapes from z-fighting.

The complete runtime-ready GLB set is tracked under
`resources/environment/kenney_space_station/`; `Assets.station` loads the 22 models used
by this arena. The pack's original orange 512×512 atlas and purple variation are shared
by the custom scene draw path. The source ZIP, FBX, OBJ, previews, and sample scene are not
tracked. The bundled `LICENSE.txt` and `SOURCE.md` record the CC0 license and provenance.
An unavailable station model falls back independently to generated geometry so a solid
tile cannot become visually empty.

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

Rigged models are optional per kit. `resources/sentinel.glb` is assigned to Scrapper,
`resources/ironclad_guardian.glb` to Tank, and `resources/gaia_guardian.glb` to Guardian.
If one file fails to load or skin, only the affected kit falls back to its primitive
brawler; startup and the other models continue normally.

The Sentinel runtime GLB is approximately 4.3 MiB and contains:

- One 5,210-vertex mesh, one material, and embedded texture data.
- One 24-joint skin and thirteen named animation clips.

The Ironclad Guardian runtime GLB is approximately 1.6 MiB, with one 4,888-vertex mesh,
one 24-joint skin, and thirteen named clips. `Assets.characters[CLASS_COUNT]` owns the
per-kit runtime instances, while `CHARACTER_MODEL_PATHS` in `assets.c` declares their
class assignments.

The Gaia Guardian runtime GLB is approximately 1.7 MiB, with one 5,070-vertex mesh, one
24-joint skin, and thirteen named clips. It is assigned only to the healer; Ironclad
remains the Tank model.

Directional movement, idle/combat, hit, and death clips are resolved separately for each
model by normalized name matching in `assets.c`. Raylib does not provide animation
crossfading here, so clip changes can visibly restart animation cycles.

`tools/fix_meshy_glb.py` converts raw Meshy exports into a raylib-compatible file by:

- Reconstructing the rest pose from inverse bind matrices.
- Baking problematic uniform scale into vertices.
- Removing non-joint transforms that raylib would compose inconsistently.
- Adding constant filler channels.
- Merging animation clips.
- Optionally downscaling oversized textures with Pillow.

The detailed contract and import procedure are in
`brawl_arena/docs/CHARACTER_PIPELINE.md`.

Sentinel, Ironclad Guardian, and Gaia Guardian share the same 24 joint names and parent
hierarchy, so their clips are reusable in principle. raylib does not perform animation
retargeting: a model with a different hierarchy, rest pose, joint orientation, or naming
needs clips baked for that rig or external retargeting before export. The current GLBs
stay self-contained with their included clips. See
`brawl_arena/docs/CHARACTER_PIPELINE.md` for the compatibility criteria.

The large character source ZIP archives are local import material and ignored. The three
repacked character GLBs and the extracted CC0 station GLBs are tracked runtime assets.

## Known technical risks

- Command-center roster operations are not team-aware.
- Rendering and AI disagree on line-of-sight requirements for proximity bush reveal.
- Movement calls `Lerp` with `moveAccel * dt` without clamping the factor. Low frame rates
  or high time scale can cause velocity overshoot.
- The preview is not exact for random spread and thick beam edges.
- Gems are silently not spawned if the fixed gem pool is exhausted.
- Animation advances from `GetFrameTime()` during rendering rather than the simulation
  delta passed through the update loop.
- No audio.
- No deterministic arena-collision, general projectile, AI, respawn/gem-drop, or
  match-rule tests. Configuration roundtrips, Guardian timing, and combat camera behavior
  are covered.
- Fixed 1280×800 presentation and no resize path.

## Good next engineering steps

1. Make command-center bot operations team-aware and distinguish free-form controls from
   Gem Grab roster controls.
2. Unify visibility behind one line-of-sight-and-concealment predicate.
3. Extend the headless suite with arena collision, general weapon damage, Gem Grab
   countdowns, and respawn/gem-drop cases.
4. Clamp movement interpolation and audit time-scale interactions.
5. Make preview geometry share the exact collision/spread calculations used by firing.
6. Add audio and animation blending only after simulation correctness is protected.
7. Add an interactive smoke-test checklist or harness for screens, input, shaders, and
   imported models.

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
make -C brawl_arena validate-config
make -C brawl_arena test
clang -std=c99 -Wall -Wextra -fsyntax-only \
  $(pkg-config --cflags raylib) brawl_arena/src/*.c
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
