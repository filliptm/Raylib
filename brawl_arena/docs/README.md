# Brawl Arena documentation

All implementation documentation for Brawl Arena lives in this root-level `docs/`
directory:

- [Architecture](ARCHITECTURE.md): layers, ownership, dependency rules, contexts,
  commands, events, and feature placement.
- [Content and tuning](CONTENT_AND_TUNING.md): canonical configuration, local drafts,
  project save actions, runtime catalog, and Guardian behavior.
- [Map packages](MAPS.md): catalog, layers, validation, props, and map authoring.
- [Development guide](DEVELOPMENT.md): build targets, source placement, feature
  workflows, tests, and interactive verification.
- [Character pipeline](CHARACTER_PIPELINE.md): Meshy/Tripo conversion, raylib skeleton
  constraints, animation reuse, and model import.

The player-facing overview remains at `../README.md`, and the detailed canonical config
format remains at `../config/README.md`. Repository-wide project context lives in
`../../docs/PROJECT_OVERVIEW.md`.

Update these documents with the code whenever structure, behavior, content formats,
controls, build commands, or verification changes. The repository `AGENTS.md` requires
future agents to read the relevant files and maintain them.
