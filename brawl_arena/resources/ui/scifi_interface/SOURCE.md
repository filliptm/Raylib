# Sci-Fi Interface Textures

- Author: rubberduck
- Source: https://opengameart.org/content/sci-fi-interface-textures
- Published: 2018-11-23
- License: Creative Commons Zero (CC0 1.0)
- Download archive SHA-256:
  `dae38a634a2199e8eaef51f7fff6b62b1a20f480121ce570b45578bcb6c1b9f3`

The upstream pack contains four 2048×2048 dashboard compositions, each in
with-effects and no-effects variants. Brawl Arena keeps only the two
no-effects sheets needed to reproduce its selected motifs:

| Source file | SHA-256 |
| --- | --- |
| `source/interface_2_no_effects.png` | `fa4afb792c89f561167c754e4f08641f38c4dd7edf88764919a45ab2cc560c39` |
| `source/interface_3_no_effects.png` | `f132d068a54115e05b91ec3bd58c44497cdbede20ba1d2b0799b18ad744573b7` |

`tools/build_ui_assets.py` creates the runtime derivatives:

| Runtime file | Source | Crop `(left, top, right, bottom)` | Size |
| --- | --- | --- | --- |
| `radar_disc.png` | interface 2 | `(276, 80, 800, 604)` | 512×512 |
| `orbital_ring.png` | interface 3 | `(96, 780, 1160, 1844)` | 768×768 |

The build converts the selected circular geometry to transparent white
linework. Runtime tinting then makes it part of the Helios palette. Complete
dashboard plates, charts, decorative glyph strings, glow variants, source
archives, and the downloaded ZIP are intentionally excluded.
