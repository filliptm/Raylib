# Documentation

This directory contains the repository-level documentation for the Raylib workspace.

## Main guides

- [Project overview](PROJECT_OVERVIEW.md): the maintained, code-verified description of
  every project, including architecture, runtime behavior, build and test paths, assets,
  maturity, and known limitations.
- [Examples launcher guide](LAUNCHER_README.md): detailed usage and build notes for the
  graphical raylib examples launcher.

## Project-local documentation

Implementation-specific documentation remains beside the project it describes:

- [Brawl Arena README](../brawl_arena/README.md)
- [Brawl Arena project configuration](../brawl_arena/config/README.md)
- [Brawl Arena character pipeline](../brawl_arena/docs/CHARACTER_PIPELINE.md)
- [Hearthstone README](../hearthstone/README.md)
- [Hearthstone design](../hearthstone/docs/HEARTHSTONE_DESIGN.md)
- [Hearthstone modular architecture](../hearthstone/docs/MODULAR_ARCHITECTURE.md)
- [Hearthstone editor manual](../hearthstone/docs/EDITOR_USER_MANUAL.md)
- [Hearthstone editor roadmap](../hearthstone/docs/EDITOR_ROADMAP.md)
- [Hearthstone editor troubleshooting](../hearthstone/docs/EDITOR_TROUBLESHOOTING.md)

The root `README.md` remains the repository landing page. The root `AGENTS.md` remains at
the root so coding agents discover its instructions automatically. Both point here for
the maintained documentation.

When a local document conflicts with `PROJECT_OVERVIEW.md`, verify the behavior in code
and update the applicable documentation as part of the same change.
