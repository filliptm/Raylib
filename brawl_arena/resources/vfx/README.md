# Ability VFX source assets

This directory contains the curated CC0 source art used by Brawl Arena's
presentation-only ability VFX library.

- `kenney_particle_pack/` contains eight 512px particle shapes selected from
  Kenney's Particle Pack.
- `rpg_vfx_pack/` contains six animated sprite sheets selected from System
  G6/Qoma's RPG VFX Pack.
- `licenses/` preserves the notices shipped with those downloads.

The original ZIP and 7z downloads are not source assets and are deliberately not
tracked. `data/vfx/asset_manifest.json` records the source URLs, license, grid
geometry, and runtime outputs.

Run:

```sh
make vfx-assets
make check-vfx-assets
```

Generated atlases live in `build/assets/vfx/`. Do not edit them directly.
Unlike character textures, VFX frames are sized for their on-screen footprint;
the character pipeline's 1K texture rule does not apply here.
