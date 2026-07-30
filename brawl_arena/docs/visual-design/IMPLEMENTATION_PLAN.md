# Arena Ink implementation record

Last code-verified: 2026-07-30

This document records the Arena Ink interface that is connected to the live Brawl Arena
runtime. It complements the browser-ready [visual field guide](index.html) and the
repeatable [UI smoke checklist](../UI_SMOKE_CHECKLIST.md). The code remains authoritative.

## Direction

Arena Ink treats the UI as a playable comic poster:

- vivid blue, red, and yellow fields;
- heavy near-black contours;
- warm paper text and keylines;
- clipped polygon panels with continuous border bands, matched rounded button
  faces/keylines, and offset hard shadows;
- halftone, ink flecks, bursts, and speed lines;
- one loud hero/action moment per screen;
- quieter surfaces wherever values or tools need sustained reading.

The signature composition is the code-drawn Brawl Arena burst wordmark, a centered live
brawler with a two-color sticker edge, and a heavy bottom launch rail. The style is
original to this project and does not require a runtime UI atlas.

## Enhancement pass

The second Arena Ink pass adds fun through identity and response rather than filling
every surface with motion:

- The wordmark, sticker brawler, and bottom rail form one 0.42-second entrance. Shared
  cubic/back easing keeps timing consistent, and reduced motion resolves immediately.
- Scrapper, Longshot, Mortar, Tank, and Guardian use saw, crosshair, blast, shield, and
  growth motifs respectively. Their immutable content styles also carry two poster
  colors and one short impact phrase.
- The former perspective cylinder is replaced by a flat vector podium with an ink
  shadow, color rings, and clipped speed slashes.
- Sticker expansion uses sixteen circular directions plus an intermediate ring and
  softened alpha thresholds. This rounds the paper keyline around shoulders, foliage,
  shields, and feet instead of leaving stepped eight-direction spikes.
- Combat uses short priority-led stamps for KO, ultimate ready, Shell break, downed, and
  Gem Grab team lock. The objective grows briefly only when countdown pressure begins.
- Results use a full split-field poster with identity motif, final score, KOs/downs, and
  Continue, Rematch, and Change Brawler. The configured timeout remains a safe fallback.

## Runtime ownership

`src/content/content_catalog.[ch]`

- Exposes each stable kit's immutable poster colors, mechanic motif, and impact label.
- Keeps screen code from branching on character IDs.

`src/ui/ui_theme.[ch]`

- Owns the semantic Arena Ink palette and contrast helpers.
- Keeps team green/red separate from decorative and action colors.

`src/ui/ui_skin.[ch]`

- Draws reusable comic panels, controls, progress frames, halftone, bursts, speed lines,
  and the full-screen poster backdrop.
- Builds panel borders as closed filled bands and buttons as nested ink, paper, and color
  silhouettes, preventing open joins and color leakage around the keyline.
- Owns no GPU texture resources; `UiSkin.ready` enables the procedural path and the
  existing geometry fallback remains callable.

`src/ui/ui_system.[ch]`

- Owns process-lifetime fonts, theme/skin services, reference-canvas layout, text roles,
  pointer/keyboard/gamepad/touch modality, safe-area-aware layout, focus, control
  interaction, reduced-motion state, shared easing, and drawing helpers.
- Draws the Brawl Arena wordmark, outlined text, panels, buttons, toggles, keycaps,
  progress bars, and focus rings.
- Loads ASCII plus the em dash used by the player-facing copy.

`src/ui/ui_icons.[ch]`

- Draws the code-native action icons, including the Studio target icon.

`src/presentation/menu_scene.[ch]`

- Owns the live 3D character preview and flat vector arena podium.
- Renders the preview into a transparent target before the backbuffer pass.
- Composites it through a circularly sampled shader that adds a black inner contour and
  rounded warm-paper outer keyline. If the target or shader is unavailable, it draws
  the raw preview.
- Keeps the stage clock separate from character preview time, so roster changes do not
  restart the stage.

`src/ui/menu.c`

- Composes the launch deck, roster, Controls, and Settings from shared primitives.
- Orchestrates the wordmark/sticker/rail entrance and draws typed character motifs.
- Keeps candidate navigation separate from profile commit.
- Starts Play and Practice with the command center closed.

`src/ui/hud.c`

- Composes the objective, ability cards, impact stamps, tutorial, downed treatment, and
  three-action result poster.
- Preserves body-anchored numeric health/shield bars and stable team colors.

`src/ui/mobile_controls.[ch]`

- Draws the floating Move and Attack sticks plus Super, Skill, and pause affordances.
- Uses the Arena Ink palette, exterior readiness/cooldown rings, ready-face/halo
  lighting, idle fade, native-resolution placement, safe-area insets, and minimum
  44-point touch targets.
- Keeps Move/Attack at one low-opacity treatment while idle or manipulated. Super/Skill
  artwork renders at 75% of its former size while its existing touch geometry remains
  unchanged.

`src/devtools/command_center.c`, `command_widgets.c`, and `studio.c`

- Use the same ink/paper/blue/yellow pigments and shared clipped surfaces.
- Deliberately reduce ornament so dense authoring controls remain scannable.

## Palette

| Token | Runtime RGB | Use |
|---|---:|---|
| Ink | `7, 16, 25` | contours, shadow mass, dark text |
| Ink soft | `10, 31, 54` | deep backdrop |
| Surface | `14, 48, 88` | quiet panel |
| Raised surface | `18, 76, 141` | secondary control |
| Border | `151, 207, 239` | informational edge |
| Paper | `255, 247, 219` | primary light text/keyline |
| Blue | `7, 108, 213` | navigation/action |
| Yellow | `255, 210, 30` | selection/attention |
| Purple | `129, 64, 240` | super/special |
| Ally | `32, 198, 122` | friendly combat state |
| Enemy/red | `217, 43, 43` | primary action/enemy state |

Paper on blue, red, and purple and ink on yellow meet the checked 4.5:1 contrast floor.
Secondary copy on the base surface meets the approved 3:1 large/supporting-text floor.

## Screen coverage

### Launch deck

- Burst wordmark at upper left without a secondary deployment tag.
- Yellow Studio and Settings, blue Controls, red Quit.
- Centered sticker brawler on a blue arena stage.
- Uninterrupted bottom rail with a blue brawler switcher, centered yellow mode choice,
  blue Practice, and the dominant red Deploy action.
- On iPhone, the launch deck preserves the playable choices but omits desktop-only
  Studio and Quit actions; its safe-width deployment deck places utilities in the
  upper-right and spans the bottom rail across the usable landscape width. App exit
  remains owned by iOS.

### Roster

- Left identity/ability stack, centered sticker preview, right live telemetry.
- Five visible candidates with a yellow selected state.
- Explicit yellow Select and separate Back action preserve candidate/commit behavior.
- On iPhone, those three columns and the five-character rail fill the safe landscape
  width rather than inheriting the centered desktop composition.

### Controls and Settings

- Modal ink panels with halftone accents and outlined yellow headings.
- Existing focus restore, scale, motion, contrast, tutorial, and glyph behavior remains.
- iPhone uses two-column touch instructions and settings groups sized to the safe width.

### Match and results

- Objective and ability surfaces use the comic geometry and stable semantic colors.
- On iPhone, desktop ability/tutorial tiles yield to the two virtual sticks, separate
  Super and Skill controls, and a safe-area pause action. The command center remains a
  desktop authoring surface and is not shown. Move and Attack remain equally translucent
  while idle and in use. Super and Skill artwork is 25% smaller without reducing touch
  targets. Their charge/cooldown grows as an exterior status ring; the muted face and
  completed halo light only when ready.
- Body bars use the shared ink-framed progress primitive.
- The player's health and ammo rails share one height. Typed combat values use
  target-aware ticker lanes: incoming damage and shield loss left, healing right, and
  outgoing damage over its target. Each lane keeps the newest value nearest the actor
  and moves older values upward, capped at three visible entries without aggregation.
- KO/readiness/break/downed/team-lock reactions are brief comic stamps; Gem Grab lock
  also escalates the objective container.
- Tutorials use ink/paper/yellow rails without obscuring play.
- Downed and final-brawl states use concentrated burst/speed-line treatment.
- Continue, Rematch, and Change Brawler are explicit focused actions with timeout
  fallback. Profile result banking remains exactly once per decided match.

### Developer tools

- Command center and VFX Studio share the palette and control geometry.
- Long lists avoid decorative bursts and prioritize labels, values, scroll, and
  provenance.

## Accessibility and interaction

- Layout scales from the 1280×800 reference canvas with a 960×600 minimum.
- Player targets retain a 44-reference-pixel minimum.
- iPhone player-facing menus use a dedicated 500-unit-tall reference frame whose width
  expands across the platform safe area; the launch backdrop remains full-bleed behind
  those insets, and touch hit regions expand to at least 44 points even when a visual is
  smaller.
- Keyboard/gamepad focus uses an ink outer and paper inner ring, independent of hue.
- Pointer hover deepens the button face and lifts it two pixels without adding a
  perimeter shape; press moves it down and deepens it further. Keyboard/gamepad focus
  keeps its independent high-contrast ring, and labels stay centered independently of
  their leading icons.
- Panel border joins remain closed at every chamfer, and button color stays entirely
  inside its rounded paper keyline.
- Reduced motion disables hover/press displacement and decorative timing while keeping
  state transitions visible; menu entrance and stamp pop resolve immediately.
- High-contrast combat cues preserve distinct ally/enemy colors and icons.
- Decorative patterns sit behind protected text and character regions.
- Hover presentation is pointer-only. Touch uses pressed, held, readiness, and release
  feedback without relying on hover or focus rings.
- Attack touch-down is visually quiet until the finger crosses the deliberate-aim
  threshold. A release before that threshold fires auto-aim without showing the world
  preview; a fast drag remains an aimed shot.

## Asset policy

Arena Ink is procedural. `make ui-assets` therefore performs no build and reports the
policy. `make check-ui` verifies:

- player screens use shared text and skin services;
- font lifetime stays inside `ui_system.c`;
- the procedural skin contains no texture load;
- required local fonts and provenance exist;
- downloaded UI archives are absent.

The former `resources/ui/kenney_scifi/` and `resources/ui/scifi_interface/` packs remain
as licensed legacy references. Their runtime PNGs are not loaded by Arena Ink.

## Verification record

On 2026-07-30:

- the optimized desktop build, architecture policy, UI policy, canonical configuration,
  full normal suite, and maintained Darwin UBSan suite passed;
- a full-screen desktop `BRAWL_MOBILE` direct-match capture verified matching
  11-reference-unit player health and ammo rails. A temporary debugger-injected burst
  through the real combat-event API showed incoming damage, healing, and outgoing
  damage in separate outlined lanes, with three same-lane values remaining legible
  instead of drawing over one another;
- an iPhone 17 Pro simulator build launched directly into a match, and its capture
  verified the lower-density Move/Attack artwork plus the 25%-smaller Skill/Super
  artwork with exterior progress rings intact;
- a subsequent iPhone 17 Pro simulator build used the same project-authored 1.5× world
  scale and complete post-processing path as desktop. Its direct-match capture showed
  the tracked saturation/grade and vignette over the world without stretching, stale
  edges, or loss of native-resolution HUD clarity; the runtime log contained no shader,
  framebuffer, or target-allocation errors;
- a cold-launch iPhone menu capture reproduced the flat Scrapper material while the
  launch log confirmed that all embedded character textures had loaded. The shared
  mobile lighting shader still held GLSL's zero `uvScale` because no arena material had
  drawn yet. After character draws began restoring unit UV state through the common
  material-state cache, three settled clean-process Home launches showed the complete
  authored texture and a direct-match capture preserved textured characters and world
  materials;
- source inspection confirmed that Move/Attack opacity no longer branches on active
  touch state and that Skill/Super drawing scales independently from unchanged touch
  geometry;
- the full-render, cold-menu-material-fix signed arm64 bundle built and installed on the
  paired physical iPhone 12 mini. Automated launch was again denied because the phone
  was locked, so the Home texture fix, physical manipulation feel, sustained frame
  pacing, and thermal behavior were not exercised on hardware.

On 2026-07-29:

- the optimized game compiled without warnings;
- architecture, UI policy, character/VFX asset, and canonical-config checks passed;
- the full normal and maintained Darwin UBSan suites passed;
- the UI test covered four desktop viewport calculations, the notched-iPhone safe-width
  home/roster/result compositions, 44-point actions, full-speed stick normalization,
  preview-free tap versus deliberate-drag attack intent, focus, easing/reduced motion,
  all five motifs, contrast, result-action identity, skin lifetime, and the shared
  showcase;
- the gameplay test verified that a touch tap fires auto-aim without entering the
  charging/preview state and that a fast deliberate drag preserves its explicit aim;
- the config test covered profile preferences without changing project provenance;
- a 1280×800 launch-only smoke check on an Apple M5 Max loaded all four imported
  characters, allocated the sticker target, and compiled the updated sticker fragment
  shader without an error.
- a subsequent 1280×800 launch-deck capture verified the caption and side-marker cleanup,
  centered mode placement and button labels, and rounded button keylines without clipping
  or icon overlap.
- a hidden 1280-pixel launch-rail render verified continuous panel corners and matched
  button keyline/fill paths without exposed background wedges.
- an iPhone 17 Pro simulator rendered the safe-width landscape launch deck and a direct
  match with correct notch-safe controls, stable imported CPU-skinned brawlers, closer
  match framing, translucent Move/Attack sticks, exterior Skill/Super progress tracks,
  the completed Skill halo and illuminated face, world/HUD composition, and all five
  mobile controls;
- the updated signed arm64 bundle installed on a paired physical iPhone 12 mini.
  Automated launch of this build was denied because the phone was locked. Physical
  multitouch feel and sustained-play thermal behavior were not exercised during this
  verification.

Earlier first-pass 1280×800 captures covered the launch deck, roster, Controls, match HUD,
and Scrapper sticker. The enhancement pass did not use desktop input automation and did
not visually exercise its rounded contour, entrance, all motifs, result actions, or the
full viewport/input/character/fallback matrix. Those checks remain intentionally tracked
in `docs/UI_SMOKE_CHECKLIST.md`.
