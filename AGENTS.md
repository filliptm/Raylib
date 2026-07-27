# Repository instructions for coding agents

These instructions apply to the entire repository.

## Required reading

Before changing code or documentation:

1. Read `docs/PROJECT_OVERVIEW.md`.
2. Read the local README and directly relevant files for the project being changed.
3. If working on Brawl Arena character assets, also read
   `brawl_arena/docs/CHARACTER_PIPELINE.md`.
4. If working on Brawl Arena gameplay values or persistence, also read
   `brawl_arena/config/README.md` and validate `brawl_arena/config/gameplay.cfg`.
5. If changing Brawl Arena architecture, module ownership, or dependencies, also read
   `brawl_arena/docs/ARCHITECTURE.md`.
6. If changing Brawl Arena maps or runtime content, also read
   `brawl_arena/docs/MAPS.md` and `brawl_arena/docs/CONTENT_AND_TUNING.md`.
7. If changing Brawl Arena source organization, tests, or build workflow, also read
   `brawl_arena/docs/DEVELOPMENT.md`.
8. If working on Hearthstone architecture or editor behavior, read the relevant file in
   `hearthstone/docs/` and verify it against the current implementation.

`docs/PROJECT_OVERVIEW.md` is the maintained repository-level description. The code is
the final authority when any document disagrees with it.

## Documentation is part of the change

Update `docs/PROJECT_OVERVIEW.md` in the same change whenever implementation work affects:

- The set or purpose of projects.
- Directory structure or module ownership.
- Build, run, dependency, or test commands.
- Application startup, screen flow, or frame-update order.
- Controls.
- Gameplay mechanics, modes, default values, capacities, or win conditions.
- AI behavior.
- Data, save, config, or network formats and integration.
- Rendering, shaders, post-processing, or asset ownership.
- Character/model import and animation behavior.
- Test coverage.
- Known limitations, incomplete paths, or previously documented risks.

Also update the affected project-local README or design document when it is the natural
place a developer or player would look. Avoid creating two contradictory sources of truth.

If a code change fixes an issue listed in `docs/PROJECT_OVERVIEW.md`, remove or revise
that issue rather than leaving it as historical commentary. If a new limitation is
intentional, document it explicitly.

## Documentation quality

- Describe implemented behavior, not intended behavior.
- Verify counts, defaults, filenames, controls, and commands from the current tree.
- Distinguish live runtime systems from compiled-but-disconnected scaffolding.
- Distinguish tracked `brawl_arena/config/gameplay.cfg`, compiled recovery values,
  ignored draft/profile state, and the legacy `brawl_arena/tuning.cfg` import source.
- Use relative repository paths in documentation.
- Date `docs/PROJECT_OVERVIEW.md` when performing a full code-verified review.
- Do not use “complete,” “fully integrated,” “exact,” or “cross-platform” unless the
  implementation and verification support the claim.

## Project boundaries

### Raylib examples and launcher

- Treat `raylib-examples/` as vendored/reference material.
- Avoid broad formatting or mechanical rewrites there unless explicitly requested.
- Run the launcher from the repository root because its paths are relative.
- Remember that the launcher compile command is macOS-specific.
- If the examples index changes, verify indexed files against actual `.c` files.

### Squad Runner

- The game intentionally lives in `squad_runner/src/main.c`.
- Do not introduce a framework-sized reorganization for a small change.
- If a feature creates meaningful new ownership boundaries, document and justify any
  modularization.

### Hearthstone

- Do not assume a compiled subsystem is connected to `main.c`.
- Trace whether work belongs to the live hard-coded gameplay path or the newer
  core/data/rules/event path.
- Preserve that distinction in documentation until the paths are actually unified.
- Network work requires deliberate multi-process verification; unit compilation alone is
  insufficient.
- The aggregate `make test` does not execute animation or save-system tests. Run their
  individual targets when relevant.

### Brawl Arena

- Keep source in its owning subsystem: `core`, `content`, `game`, `app`, `presentation`,
  `ui`, or `devtools`. Do not recreate a universal shared-types header.
- Deterministic simulation APIs take `GameContext`, never `App *`. Game/core code must
  not read platform input, call rendering/effect functions, or use raylib randomness.
  Run `make -C brawl_arena check-architecture` after dependency changes.
- `App` owns the game session, player controller, presentation state, flow, tuning,
  content catalog, and configuration provenance. Reset only the state region whose
  lifetime ended.
- Capture device input into `PlayerInput` in the app layer. Communicate simulation
  presentation through `GameEvent`, and route developer gameplay mutations through
  application commands.
- Preserve fixed-pool behavior unless a change explicitly redesigns allocation.
- `brawl_arena/config/gameplay.cfg` is the tracked source of truth for project tuning.
  Change it intentionally or through the command center's explicit PROJECT save actions.
- Use `BRAWL_PROJECT_CONFIG`, `BRAWL_TUNING`, `BRAWL_PROFILE`, and
  `BRAWL_LEGACY_TUNING` with temporary paths for automated/runtime config checks. Do not
  overwrite or delete the user's local tuning/profile files.
- Keep Gem Grab and free-form/practice roster semantics separate. Existing command-center
  bot helpers are not team-aware.
- Maps are versioned packages listed by `brawl_arena/data/maps/manifest.cfg`. Preserve
  separate terrain, gameplay, visual, and prop layers, and validate every listed map.
- Permanent walls and destructible crates are different rules. Do not describe crate
  destruction as permanent-wall destruction.
- Raw Meshy/Tripo GLBs are not runtime-ready. Use the documented conversion pipeline and
  preserve primitive fallback behavior.

## Generated and user-owned files

Build outputs, ignored binaries, logs, tuning files, and imported source archives are not
source. Preserve them unless the task explicitly requires rebuilding or removing a known,
safe target.

In particular:

- Do not commit `brawl_arena/tuning.cfg`, `brawl_arena/tuning.local.cfg`, or
  `brawl_arena/profile.cfg`.
- Do not commit the large `brawl_arena/*.zip` source archives.
- Do not remove user tuning or logs as part of cleanup.
- Do not edit generated binaries directly.
- The root `example_launcher` binary is already tracked despite its ignore rule. Compile
  verification builds to `/tmp/example_launcher_verify` unless intentionally updating it.

## Verification expectations

Run checks proportional to the affected project. The maintained command matrix is in
`docs/PROJECT_OVERVIEW.md`.

At minimum:

### Launcher

```bash
clang -std=c99 -Wall -Wextra example_launcher.c -o /tmp/example_launcher_verify \
  $(pkg-config --cflags --libs raylib) \
  -framework OpenGL -framework Cocoa -framework IOKit
```

### Squad Runner

```bash
make -C squad_runner
clang -std=c99 -Wall -Wextra -fsyntax-only \
  $(pkg-config --cflags raylib) squad_runner/src/main.c
```

### Hearthstone

```bash
make -C hearthstone
make -C hearthstone test
```

Run `make -C hearthstone test_animation` and
`make -C hearthstone test_save_system` when those systems or the full test state matter.

### Brawl Arena

```bash
make -C brawl_arena
make -C brawl_arena check-architecture
make -C brawl_arena validate-config
make -C brawl_arena test
make -C brawl_arena sanitize
```

Compilation does not validate graphical behavior. For changes to input, gameplay,
rendering, screens, shaders, assets, or persistence, perform the relevant interactive
check when the environment permits it and report what was or was not exercised.

## Change discipline

- Preserve unrelated user changes.
- Keep changes scoped to the requested project.
- Do not silently “fix” vendored examples while working on a game.
- Do not hide known gaps behind documentation-only claims.
- Report verification results and any untested graphical paths in the final handoff.
