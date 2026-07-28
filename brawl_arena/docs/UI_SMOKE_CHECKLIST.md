# Helios Broadcast UI smoke checklist

Last code-verified: 2026-07-27

Use this checklist after UI, rendering, input, character, or configuration changes.
Automated checks establish geometry and policy; this pass verifies the parts that need a
real raylib window, GPU, and input devices.

## Before launching

```bash
make
make check-ui
make test
make validate-config
```

For destructive tuning experiments, copy `config/gameplay.cfg` to a temporary location
and use the `BRAWL_PROJECT_CONFIG`, `BRAWL_TUNING`, and `BRAWL_PROFILE` overrides
documented in [CONTENT_AND_TUNING.md](CONTENT_AND_TUNING.md).

## Viewport matrix

Repeat the screen pass at:

- 960×600 minimum window.
- 1280×800 reference composition.
- 1920×1080.
- 2560×1440.

At every size verify:

- No text clips, crosses a protected character area, or leaves its panel.
- Every action remains visible and pointer targets are at least 44 reference pixels.
- Pointer hit testing follows the visual control after resize.
- The world/post-process target fills the viewport without stretching or stale edges.
- Modal dimming, command-center scrolling, footer actions, and result actions remain
  reachable.

## Screen pass

- Launch deck: title, open character stage, active-brawler switcher, mode arrows,
  Practice, Controls, Settings, Deploy, and Quit; combat stats remain exclusive to the
  roster bay.
- Roster bay: all five candidates, candidate preview, explicit Select, cancel without
  committing, and selected state after returning home.
- Controls: correct keyboard/mouse and gamepad bindings; Back restores prior focus.
- Settings: UI scale, reduced motion, high contrast, tutorial hints, glyph mode, and
  tutorial reset all apply immediately and survive restart.
- Match HUD: objective, vitals, ammo, mobility/super state, team shape cues, downed state,
  and action-specific tutorials.
- Result: explicit Continue works; timeout still returns safely.
- Command center: all seven categories, scroll, slider fine adjustment, toggles, gameplay
  commands, provenance, Restore Project, Save All, and Preview / UI framing.

## Input matrix

- Pointer: hover, press, release, wheel, and no click-through into the world.
- Keyboard: arrows/WASD focus movement, Enter/Space activation, Escape hierarchy, and
  Shift fine slider adjustment.
- Gamepad: D-pad/left-stick focus, A activation, B back, bumpers/category movement,
  left-stick move, right-stick aim, triggers/main, right bumper/super, and left
  bumper/mobility.
- Switch between pointer, keyboard, and gamepad; focus visibility and binding glyphs
  should follow the active/forced modality without moving control bounds.

## Character and presentation matrix

Check imported and primitive fallback rendering for every kit on both home and roster
screens:

- Preview never rotates automatically.
- Idle animation remains active unless reduced motion is enabled.
- Feet remain on the podium and the full silhouette remains inside protected space.
- Tank is 205° on home and front-facing/smaller in roster; its shield and feet are fully
  visible in both.
- Preview / UI edits apply live, Reset restores project framing, and Save Kit + Framing
  produces a validated tracked `config/gameplay.cfg` change.

## Accessibility and comfort

- Text remains legible with high-contrast cues enabled and at 1.50× UI scale.
- Team, readiness, warning, and disabled states do not depend on hue alone.
- Reduced motion removes decorative cadence without hiding state changes.
- Tutorials retire only after their corresponding action and stay retired after restart.
- No main attack, super, hit, impact, or elimination shakes any user's camera.

## Rendering and fallback

- Local Barlow/IBM Plex fonts load; temporarily missing fonts fall back without crashing.
- Kenney panels/buttons/bars stay opaque and preserve corners, screws, borders, focus
  rings, and pointer bounds at every viewport.
- Orbital/radar motifs remain restrained behind characters/data and never obscure text
  or silhouettes.
- Helios-9 and Training Court render with and without post effects.
- Resize repeatedly during a match and menu transition; render targets recreate cleanly.
- Temporarily missing station/character assets use coherent procedural fallbacks.
- Temporarily missing individual UI skin textures fall back independently to the
  code-drawn Helios geometry without changing layout or preventing startup.
- Imported animation selection, grass, aim previews, rain, Resonance, and result overlays
  remain aligned with the world.

Record the sizes, input devices, kits, and fallback cases actually exercised when handing
off a change. Do not report this graphical matrix as complete based only on headless
tests.
