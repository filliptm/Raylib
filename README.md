# Raylib Game Prototypes and Examples

This repository is a raylib workspace containing an examples browser and three original
game prototypes. It is not a single game or a single build target.

The current projects are:

| Project | Purpose | Status |
|---|---|---|
| `raylib-examples/` + `example_launcher.c` | Browse, study, compile, and run the local raylib examples collection | Reference collection and macOS launcher |
| `squad_runner/` | Endless 3D squad autorunner built as one compact C program | Playable prototype |
| `hearthstone/` | Modular 3D card-game experiment with AI, editor, networking, data, and test scaffolding | Broad prototype; several systems remain partially integrated |
| `brawl_arena/` | Top-down 3D arena brawler with Gem Grab, a practice sandbox, live tuning, procedural rendering, and a rigged-character pipeline | Most active and cohesive game |

## Start here

Read [docs/PROJECT_OVERVIEW.md](docs/PROJECT_OVERVIEW.md) for the maintained,
code-verified guide to every project. It covers:

- What each project is intended to demonstrate.
- How its runtime and major systems work.
- Build, run, input, data, asset, and test paths.
- Which features are live and which are scaffolding.
- Known limitations and documentation drift.
- The verification commands appropriate to each project.

Project-specific documentation remains useful for deeper implementation notes:

- [Documentation index](docs/README.md)
- [Launcher guide](docs/LAUNCHER_README.md)
- [brawl_arena/README.md](brawl_arena/README.md)
- [brawl_arena/docs/CHARACTER_PIPELINE.md](brawl_arena/docs/CHARACTER_PIPELINE.md)
- [hearthstone/README.md](hearthstone/README.md)
- [hearthstone/docs/](hearthstone/docs/)

When a project-specific README disagrees with `docs/PROJECT_OVERVIEW.md`, verify the
behavior in code. Some older local documentation describes an earlier stage of its
project.

## Requirements

The original games are written in C99 and use raylib. The current macOS setup expects:

```bash
brew install raylib
pkg-config --modversion raylib
```

The checked environment uses raylib 5.5.0. Linux builds are supported by some project
Makefiles, but the root example launcher constructs a macOS-specific compiler command.

## Common entry points

Run these commands from the repository root:

```bash
# Build the graphical examples launcher.
clang -std=c99 -Wall -Wextra example_launcher.c -o example_launcher \
  $(pkg-config --cflags --libs raylib) \
  -framework OpenGL -framework Cocoa -framework IOKit

# Build or run the original games.
make -C squad_runner
make -C squad_runner run

make -C hearthstone
make -C hearthstone run
make -C hearthstone test

make -C brawl_arena
make -C brawl_arena run
```

Each executable opens a graphical window. `make` only builds; `make run` builds and then
launches.

## Repository conventions

- `build/`, object files, game binaries, logs, and `brawl_arena/tuning.cfg` are generated
  locally and ignored where appropriate. The existing root `example_launcher` binary is
  already tracked despite its ignore rule; use a `/tmp` output for verification unless
  intentionally refreshing that artifact.
- The source character archives under `brawl_arena/*.zip` are local import material. The
  repacked runtime model is `brawl_arena/resources/sentinel.glb`.
- `raylib-examples/` is a vendored/reference collection. Avoid broad rewrites there unless
  the task explicitly concerns the examples.
- Repository-wide instructions for coding agents are in [AGENTS.md](AGENTS.md).

## Current focus

The Git history and code organization indicate that Brawl Arena is the active product
direction. Squad Runner is a small earlier prototype, Hearthstone is a wider experimental
module with unfinished integrations, and the examples collection is primarily a learning
and reference environment.
