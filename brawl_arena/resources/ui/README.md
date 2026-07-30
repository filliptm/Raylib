# UI resources

The live **Arena Ink** interface is generated procedurally by
`src/ui/ui_skin.c` and `src/ui/ui_system.c`. It does not load a UI texture
atlas. Comic panels, thick keylines, hard shadows, bursts, halftone fields,
speed lines, the Brawl Arena wordmark, progress bars, and focus treatment all
scale from the shared reference canvas at runtime.

The `kenney_scifi/` and `scifi_interface/` directories are retained as licensed
legacy source material from the former interface. Their PNGs and SVGs are not
loaded by Arena Ink. Keep each pack's `LICENSE.txt` and `SOURCE.md` with the
files if the reference material is moved or removed.

`make check-ui` enforces the procedural ownership boundary and rejects
downloaded archives. `make ui-assets` documents that no runtime UI atlas build
is required.
