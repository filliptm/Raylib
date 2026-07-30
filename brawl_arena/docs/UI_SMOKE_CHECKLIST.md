# Arena Ink UI smoke checklist

Last code-verified: 2026-07-29

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
- At the default 1.5× world render scale, a 1280×800 drawable creates a 1920×1200 scene
  target and a 960×600 drawable creates a 1440×900 target while the HUD remains
  native-resolution. On HiDPI, verify these calculations use drawable pixels rather
  than logical UI coordinates.
- Modal dimming, command-center scrolling, footer actions, and result actions remain
  reachable.

## Screen pass

- Launch deck: Brawl Arena burst wordmark, open character stage, active-brawler switcher,
  centered mode selector and arrows, Practice, Controls, Settings, Deploy, and Quit;
  redundant deployment/brawler/mode captions and the bottom rail's side marker remain
  absent, while combat stats remain exclusive to the roster bay.
- Roster bay: all five candidates, candidate preview, explicit Select, cancel without
  committing, and selected state after returning home.
- Controls: correct keyboard/mouse and gamepad bindings; Back restores prior focus.
- Settings: UI scale, reduced motion, high contrast, tutorial hints, glyph mode, and
  tutorial reset all apply immediately and survive restart.
- Match HUD: objective, secondary/super state, downed state, and action-specific
  tutorials; no duplicate bottom-left vitals panel.
- World bars: health values are centered inside every health bar, Scrapper charge or
  broken lockout is centered inside its shield bar, and the player's ammo remains below
  the body-anchored health bar.
- Team readability: the player and allied bots remain green at every health level,
  enemies remain red at every health level, and ally/enemy shape cues remain visible.
- Combat-text relevance: damage dealt to or received by the player, healing given or
  received by the player, self-healing, and player-involved shield absorption remain
  visible. Damage, healing, regeneration, and shield exchanges involving bots alone do
  not produce numbers; gem and class-change labels remain available.
- Result: Continue returns home, Rematch rebuilds the same selection/mode, Change Brawler
  opens the roster, banking occurs once, and timeout still returns safely.
- Command center: all seven categories, scroll, slider fine adjustment, toggles, gameplay
  commands, provenance, Restore Project, Save All, and Preview / UI framing.
- On separate fresh launches, Play and Practice each begin with the command center
  closed; `TAB` opens and closes it.
- VISUAL World render scale applies live at 1.0×, 1.5×, and 2.0× without stretching,
  stale borders, or changing HUD scale.
- WORLD match-camera distance applies live at 20 and 60 units without changing pitch,
  breaking aim lead, or clipping the playable arena.

## Input matrix

- Pointer: hover, press, release, wheel, and no click-through into the world.
- Button hover uses only a slightly deeper face plus lift—no extra hexagonal perimeter;
  press moves down, and reduced motion retains the face-tone change without displacement.
- Keyboard: arrows/WASD focus movement, Enter/Space activation, Escape hierarchy, and
  Shift fine slider adjustment.
- Gamepad: D-pad/left-stick focus, A activation, B back, bumpers/category movement,
  left-stick move, right-stick aim, triggers/main, right bumper/super, and left
  bumper/secondary.
- Switch between pointer, keyboard, and gamepad; focus visibility and binding glyphs
  should follow the active/forced modality without moving control bounds.

## Character and presentation matrix

Check imported and primitive fallback rendering for every kit on both home and roster
screens:

- Preview never rotates automatically.
- Idle animation remains active unless reduced motion is enabled.
- Feet remain on the stage and the full silhouette plus paper keyline stays inside
  protected space.
- The saw, crosshair, blast, shield, and growth motifs match their kits and remain behind
  the protected silhouette/text regions.
- The logo, character, and launch rail settle as one short entrance. Reduced motion
  resolves the composition immediately without hiding state.
- Every model has identical yaw, scale, offset, camera, and target framing on home and
  roster; rapidly changing candidates does not move/restart the comic stage.
- Tank's shield and every character's feet remain fully visible under that one framing.
- Preview / UI edits apply live, Reset restores the project showcase, and Save Kit +
  Showcase produces a validated tracked `config/gameplay.cfg` change.

## Accessibility and comfort

- Text remains legible with high-contrast cues enabled and at 1.50× UI scale.
- Team, readiness, warning, and disabled states do not depend on hue alone.
- Reduced motion removes decorative cadence without hiding state changes.
- Tutorials retire only after their corresponding action and stay retired after restart.
- No main attack, super, hit, impact, or elimination shakes any user's camera.

## Rendering and fallback

- Local Barlow/IBM Plex fonts load; temporarily missing fonts fall back without crashing.
- Procedural panels/buttons/bars stay opaque and preserve clipped panel corners, rounded
  button paper keylines, ink contours, hard shadows, focus rings, centered labels, and
  pointer bounds at every viewport. Panel borders form uninterrupted bands through every
  chamfer; button color stays inside the matching rounded keyline without corner wedges.
- Halftone, bursts, and speed lines remain restrained behind characters/data and never
  obscure text or silhouettes.
- The sticker shader produces a continuous black inner contour and rounded paper outer
  keyline without stair steps or pointed diagonal corners for every imported model and
  the primitive Mortar fallback; shader failure still draws the unobscured raw preview.
- Navigation, activation, brawler selection, deploy, ultimate-ready, victory, and defeat
  cues play at the configured level; mute and missing-device startup remain silent.
- Helios-9 and Training Court render with and without post effects.
- Move parallel and diagonally past adjoining Helios-9 wall blocks; their base seams and
  shallow-angle side textures remain stable without flashing, crawling, or coplanar
  flicker at the default 1.5× render scale. Inspect the complete floor-to-cap vertical
  edge where perpendicular textured wall faces meet; no dark/light atlas strip may
  alternate there. Leave grain enabled and confirm flat wall colors do not pulse while
  standing still.
- Resize repeatedly during a match and menu transition; scaled color/depth targets
  recreate cleanly and post effects do not retain the previous source or output
  resolution.
- Disable post effects and confirm direct world rendering remains antialiased through
  the 4× MSAA backbuffer path.
- Temporarily missing station/character assets use coherent procedural fallbacks.
- Arena Ink starts without loading a UI texture atlas; retained legacy UI source packs
  are not runtime dependencies.
- Imported animation selection, grass, aim previews, Scrapper saw/Shell visuals,
  Longshot Grapple cable/pose phases, Mortar Mine body/rings/deploy pose, rain,
  Resonance, and result overlays remain aligned with the world.
- Scrapper's procedural saw is readable on both legs; the full spherical Shell cage,
  low-charge instability, start/hit/collapse/restore recipes, numeric world charge bar,
  and braced pose are visible without rectangular outline artifacts.
- Longshot's cable terminates at cover, stays attached to the animated right hand, and
  its fire/hook/pull/land phases do not show rectangular atlas edges. Holding
  Shift/left bumper shows a stable maximum-range ring and exact endpoint while moving
  and aiming; cover-limited paths turn amber, invalid short paths turn red, release
  launches toward that same endpoint, and the hook tip visibly travels before pulling.
- Mortar's Mine rises/arms clearly, shows distinct trigger and blast rings without
  floor fighting, remains hidden behind solid cover where appropriate, and detonates
  with readable knockback.

Record the sizes, input devices, kits, and fallback cases actually exercised when handing
off a change. Do not report this graphical matrix as complete based only on headless
tests.
