# Helios Broadcast implementation plan

Status: runtime integration implemented; graphical release matrix remains repeatable QA  
Last code-verified: 2026-07-27

This plan turns the [Helios Broadcast visual system](index.html), the
[screen concepts](menus.html), and the [visual audit](audit.html) into an
implementation record for the live raylib game.

The architecture, state, screen, persistence, and verification decisions in this
document now describe the runtime. Milestone wording is retained as the implementation
history. The complete graphical matrix remains an operator-run release check because it
requires multiple real viewport, GPU, fallback-asset, and input-device combinations; use
[UI_SMOKE_CHECKLIST.md](../UI_SMOKE_CHECKLIST.md).

## Delivered integration

- Shared Helios theme, locally shipped fonts, semantic text roles, code-drawn icons,
  reference-canvas layout, controls, modality, and focus graph.
- Curated CC0 Kenney control hardware plus two OpenGameArt-derived orbital/radar motifs,
  owned by `UiSystem`, rendered with nine-slice scaling, and protected by per-resource
  code-drawn fallbacks and static asset validation.
- Opaque physical control surfaces; alpha is reserved for shadows, modal dimming, fades,
  and emissive effects rather than panel/card backgrounds.
- Resizable scene rendering with a 960×600 minimum and resize-safe render-target
  recreation.
- Quiet launch deck, detailed candidate/commit roster, Controls, Settings, combat HUD,
  action-retired tutorials, downed state, explicit results, and the reorganized command
  center.
- A Helios-9 menu scene with non-rotating previews and project-authorable per-character
  home/select presentation profiles.
- Profile-only UI scale, reduced motion, high contrast, tutorial, and glyph preferences.
- Pointer, keyboard, and player-facing gamepad paths; keyboard fine adjustment for
  command-center sliders.
- Headless layout/focus/theme/profile tests, configuration round trips, and static UI
  ownership/font/asset policy checks.

The illustrative module tree below was consolidated where ownership stayed cohesive:
font resources, text, layout, focus, and common controls live together in
`ui_system.[ch]`; tokens and icons remain separate. This keeps the same dependency
contract without creating one-file wrappers.

## Outcome

The finished game should have:

- A single, named visual system for colors, typography, spacing, panel construction,
  interaction states, and motion.
- Local, licensed font assets loaded once and used through one text API.
- A scalable desktop layout that retains 1280×800 as its reference composition.
- Mouse, keyboard, and controller navigation for every player-facing menu.
- Per-character home and selection presentation profiles.
- The approved home, roster, controls, settings, HUD, result, and command-center
  compositions.
- A station/hangar presentation scene behind the menus using existing Helios-9 assets.
- Player preferences for UI scale, reduced motion, high-contrast cues, tutorial hints,
  and input glyph choice.
- Automated tests for pure layout, focus, configuration, and profile validation, plus a
  repeatable graphical smoke checklist.
- No gameplay-rule change and no path from UI code into deterministic simulation except
  through the existing application commands and screen requests.

## Scope decisions

| Decision | Choice |
|---|---|
| Primary platform | Desktop raylib, keyboard/mouse and common gamepads |
| Reference composition | 1280×800 |
| Supported validation sizes | 960×600, 1280×800, 1920×1080, 2560×1440 |
| Theme | One authored dark Helios-9 theme; no generic light mode |
| Display type | Barlow Condensed Bold |
| Body type | Barlow Regular/Semibold |
| Data type | IBM Plex Mono Medium |
| Signature | Signal rail, bolted hull controls, orbital/radar linework, and safety marks |
| Player UI navigation | Mouse, keyboard, and controller |
| Developer tool navigation | Mouse and keyboard required; controller optional |
| Runtime allocation | No per-frame heap allocation in UI |
| Gameplay impact | None |
| Existing tuning truth | `config/gameplay.cfg` remains tracked project truth |
| User UI preferences | Persist with profile-scoped state, never project defaults |
| Visual source documents | The linked HTML concepts remain the visual reference |

The generic Orbitron/monospaced cyberpunk treatment is deliberately out of scope. It
would make Brawl Arena resemble a broad category of sci-fi interfaces rather than the
industrial Helios-9 world. Monospaced type is reserved for numbers, timers, bindings, and
configuration provenance.

## Non-goals

- Rewriting game simulation, AI, weapons, maps, or character behavior.
- Adding audio.
- Adding a general-purpose GUI framework or ECS.
- Full operating-system screen-reader integration. raylib has no native semantic
  accessibility tree; that would require a separate platform bridge.
- Making every theme color designer-tunable. Brand tokens remain authored source so the
  interface cannot drift into an incoherent palette.
- Supporting arbitrary window sizes below 960×600.
- Localizing the game in this pass. Text APIs must wrap and fit safely so localization
  can be added later.
- Maintaining the old and new UI indefinitely. Screens migrate one at a time, then the
  superseded path is removed.

## Architectural constraints

The existing dependency direction remains:

```text
core/content/game
        ↓
 app composition
   /     |      \
presentation   ui   devtools
```

Rules for this work:

1. `src/game` and `src/core` do not import UI, fonts, focus, window, or device state.
2. UI reads application/content snapshots and requests actions through narrow app APIs.
3. Gameplay mutations initiated by developer tools continue through `GameCommand`.
4. UI never calls gameplay damage, spawn, objective, or weapon functions directly.
5. The 3D menu scene belongs to presentation, not to reusable UI components.
6. Components do not depend on `App`; screen controllers provide their labels, values,
   rectangles, and callbacks/actions.
7. Player-facing components and developer widgets share services, not layouts.
8. All long-lived UI and developer state has an explicit owner instead of growing the
   current collection of file-static globals.

## Target ownership

`main.c` remains the composition root and owns:

```text
App
├── GameSession
├── PlayerController
├── PresentationState
├── AppFlow
├── Tuning / ContentCatalog / ConfigState
└── UiPreferences

Assets
├── world shaders/models/textures
└── post-process render target

UiSystem
├── UiTheme
├── UiResources (local font handles)
├── UiSkin (curated textures, slicing metadata, and per-resource fallback state)
├── UiInputState
├── UiFrameLayout
├── MenuState
├── HudState
└── transient UI feedback

MenuScene
├── presentation camera and lights
├── preview brawler and animation state
└── station/hangar scene state

CommandCenterState
├── open tab and per-tab scroll
├── active editor interaction
└── pending app/config actions
```

### Lifetime rules

| State | Lifetime | Reset |
|---|---|---|
| `UiTheme` and fonts | Process | On shutdown only |
| `UiSkin` textures | Process | On shutdown only; missing files fall back independently |
| `UiInputState` | Process with per-frame snapshot | Begin each frame |
| `MenuState` | Process; screen-specific portions reset on entry | Screen transition |
| `HudState` | Match presentation | `ResetMatch()` or match entry |
| `MenuScene` | Process; preview rebuilt when candidate changes | Character/screen change |
| `CommandCenterState` | Process | Explicit reset or shutdown |
| `UiPreferences` | Profile | Profile load/change |
| Tutorial completion | Profile | Explicit “Reset hints” action |

Do not place `UiSystem` in `GameSession`; match reset must not unload fonts, forget
menu focus, or destroy player preferences.

## Proposed source layout

```text
src/
├── app/
│   ├── app_types.h                 add UiPreferences, not component types
│   └── main.c                      composition and frame orchestration
├── content/
│   ├── config.[ch]                 profile preferences + project preview values
│   ├── content_types.h             CharacterPresentationDefinition
│   └── content_catalog.[ch]        validated presentation lookup
├── presentation/
│   ├── assets.[ch]                 world assets and resize-safe post target
│   ├── menu_scene.[ch]             podium, hangar, model preview, lighting
│   └── render.[ch]                 match world only
├── ui/
│   ├── ui_types.h                  IDs, roles, responses, preferences-facing enums
│   ├── ui_theme.[ch]               immutable semantic tokens
│   ├── ui_resources.[ch]           font and generated UI texture lifetime
│   ├── ui_text.[ch]                measurement, fitting, wrapping, styled drawing
│   ├── ui_layout.[ch]              scale, safe frame, anchors, layout helpers
│   ├── ui_input.[ch]               modality, focus, keyboard/controller navigation
│   ├── ui_icons.[ch]               consistent code-drawn station glyphs
│   ├── ui_skin.[ch]                curated texture lifetime, slicing, and motifs
│   ├── ui_components.[ch]          buttons, panels, rows, bars, keycaps, modals
│   ├── ui_system.[ch]              process lifecycle and begin/end frame
│   ├── menu.[ch]                   menu flow and shared orchestration
│   ├── menu_home.c                 launch deck
│   ├── menu_roster.c               brawler selection
│   ├── menu_overlays.c             controls and settings
│   ├── hud.[ch]                    match HUD orchestration
│   ├── hud_world.c                 overhead bars and world-adjacent labels
│   ├── hud_match.c                 objective, vitals, abilities, result
│   └── hud_tutorial.c              action-based tutorial state
└── devtools/
    ├── command_center.[ch]          state, rail, scroll, sticky header/footer
    ├── command_widgets.[ch]         dense widgets using shared UI services
    ├── command_tabs_gameplay.c      Match, Bots, Player
    ├── command_tabs_content.c       Kit and map/content controls
    └── command_tabs_presentation.c  World, visual, and preview controls
```

This split is based on ownership and cohesive screen regions. It should not be expanded
into one file per button or one type per visual token.

## Core UI types

The following structures are the intended shape, not copy-ready declarations:

```c
typedef unsigned int UiId;

typedef enum UiInputModality {
    UI_INPUT_POINTER = 0,
    UI_INPUT_KEYBOARD,
    UI_INPUT_GAMEPAD
} UiInputModality;

typedef enum UiTextRole {
    UI_TEXT_DISPLAY = 0,
    UI_TEXT_TITLE,
    UI_TEXT_HEADING,
    UI_TEXT_BODY,
    UI_TEXT_LABEL,
    UI_TEXT_CAPTION,
    UI_TEXT_DATA,
    UI_TEXT_ROLE_COUNT
} UiTextRole;

typedef struct UiResponse {
    bool hovered;
    bool focused;
    bool held;
    bool activated;
} UiResponse;

typedef struct UiFrameLayout {
    int viewportWidth;
    int viewportHeight;
    float scale;
    Rectangle safe;
    Rectangle content;
} UiFrameLayout;

typedef struct UiPreferences {
    float scale;
    bool reducedMotion;
    bool highContrast;
    bool showTutorialHints;
    int inputGlyphMode;       // auto, keyboard/mouse, gamepad
    unsigned int tutorialFlags;
} UiPreferences;
```

`UiId` values must be stable across frames. Screen controllers may use explicit enum
values or a deterministic string hash. Transient pointers and list indices that change
when scrolling are not stable IDs.

## UI system and frame API

Proposed lifecycle:

```c
bool UiSystemLoad(UiSystem *ui, const UiPreferences *preferences);
void UiSystemUnload(UiSystem *ui);
void UiSystemBeginFrame(UiSystem *ui, int width, int height, float dt);
void UiSystemEndFrame(UiSystem *ui);

void MenuEnter(UiSystem *ui, const App *app, AppScreen screen);
UiActions MenuUpdate(UiSystem *ui, App *app, float dt);
void MenuDraw(UiSystem *ui, const App *app);

void HudReset(UiSystem *ui);
void HudObservePlayerInput(UiSystem *ui, const PlayerInput *input, const App *app);
void HudDraw(UiSystem *ui, const App *app);
```

`UiActions` is a small value returned to the app, for example:

```c
typedef struct UiActions {
    bool requestQuit;
    bool requestPractice;
    bool requestPlay;
    bool requestMatchRestart;
    bool selectedKitChanged;
    AppScreen requestedScreen;
} UiActions;
```

The app applies actions after UI input handling. Component drawing must not directly
reset matches or quit the process.

## Theme contract

`UiTheme` contains semantic roles rather than screen-specific colors:

```text
Background:  Void #050B14 / Deep #07111F
Surfaces:    Deck #0E1B2E / #12233A
Structure:   Hull #1A3049 / #29435E / Line #3B5874
Text:        Paper #F3F7FB / Mist #A8B7C8 / Muted #8295AA
Interaction: Ion #64B9FF
Attention:   Safety #FF9D42
Ready:       Gold #F6CF65
Ability:     Reactor #B88CFF
Ally:        Green #55D59A
Enemy:       Coral #FF6577
```

Every functional color also has a non-color cue:

- Ally: left/diamond/plus cues.
- Enemy: right/chevron/minus cues.
- Ready: gold border plus `READY` label and filled state.
- Warning: safety rail plus warning label.
- Selected: signal rail plus inset fill and selection label.
- Focus: bright outer border independent of hover.

Raw theme colors should not be redeclared in `menu_*.c`, `hud_*.c`, or command tabs.
Character accents remain content data because they identify a kit, not a generic state.

### Shape and elevation

- Panels use a shared chamfered polygon with small structural cuts.
- Player UI uses two elevation levels: docked/inset and modal/foreground.
- Developer controls use flatter, denser surfaces.
- Rounded pills are limited to progress tracks, compact state chips, and slider tracks.
- Buttons keep fixed bounds in every state; press feedback is an internal two-pixel
  surface compression, never a hit-box move.
- Glow is reserved for focus, ready, and active energy states.

## Typography and font assets

Add locally licensed files:

```text
resources/fonts/
├── BarlowCondensed-Bold.ttf
├── Barlow-Regular.ttf
├── Barlow-SemiBold.ttf
├── IBMPlexMono-Medium.ttf
├── LICENSE-Barlow.txt
├── LICENSE-IBM-Plex.txt
└── SOURCE.md
```

Implementation rules:

1. Load fonts once after `InitWindow()`.
2. Use `LoadFontEx`; request the actual glyph range required by shipped copy.
3. Load display atlases at approximately 96 px and body/data atlases at 64 px so scaled
   desktop rendering remains crisp.
4. Apply bilinear filtering to each font texture.
5. Fall back to `GetFontDefault()` if a file is missing; log one actionable warning and
   keep the game playable.
6. Draw and measure through `UiText*` only.
7. Use `DrawTextEx` and `MeasureTextEx`.
8. Use local `snprintf` buffers for composed strings. Do not retain or combine multiple
   `TextFormat()` return pointers.
9. Support left, center, and right alignment; single-line fit; word wrapping; ellipsis
   as an explicit last resort; shadow and restrained outline styles.
10. Keep body text in sentence case. Uppercase is reserved for display, compact labels,
    keycaps, and station telemetry.

Reference sizes at 1280×800:

| Role | Size | Face |
|---|---:|---|
| Display | 64–78 | Barlow Condensed Bold |
| Screen title | 40–48 | Barlow Condensed Bold |
| Heading | 26–32 | Barlow Condensed Bold |
| Body | 17–18 | Barlow Regular |
| Emphasis | 17–18 | Barlow Semibold |
| Label | 13–14 | Barlow Semibold |
| Caption | 11–12 | IBM Plex Mono Medium |
| Data | 13–20 | IBM Plex Mono Medium |
| Result banner | 72–84 | Barlow Condensed Bold |

Text smaller than the caption role is not permitted in player-facing UI.

## Layout and resizing

### Stage 1: scalable layout at the current fixed window

All new screen layouts first use `UiFrameLayout` while the window remains 1280×800.
This isolates coordinate changes from render-target resizing.

### Stage 2: supported resizable window

After every player screen uses the layout service:

1. Add `FLAG_WINDOW_RESIZABLE`.
2. Set a 960×600 minimum.
3. Compute scale from the reference canvas while anchoring edge regions to the actual
   viewport.
4. Preserve a 24-reference-pixel safe frame at minimum scale.
5. Keep center character and objective regions independent from left/right docks.
6. Recreate the scene/depth target through a single `AssetsResizeViewport()` path when
   framebuffer dimensions change.
7. Update post-process resolution and depth bindings after recreation.
8. If target recreation fails, disable the post path for that frame and continue with a
   direct world draw.

The target is responsive desktop composition, not uniform stretching. Wide layouts gain
breathing room around the model/arena; narrow layouts compress secondary panels before
shrinking primary text.

## Input and focus

### Player-facing navigation map

| Intent | Keyboard | Gamepad |
|---|---|---|
| Navigate | Arrows or WASD | D-pad or left stick |
| Activate | Enter or Space | South/A button |
| Back/close | Escape | East/B button |
| Previous/next category | Q/E or Page Up/Down where shown | Shoulder buttons |
| Scroll roster/details | Wheel, drag, arrows | Stick/D-pad |

Rules:

- Pointer movement switches modality to pointer and suppresses the focus ring until a
  keyboard/gamepad navigation action occurs.
- Keyboard/gamepad navigation restores a visible focus ring.
- Every focusable target has at least a 44×44 reference-pixel hit area.
- Focus order follows visual order.
- Closing a modal restores focus to the control that opened it.
- Changing screens assigns a deliberate initial focus.
- List focus stays on the selected character after scrolling or resize.
- Directional navigation uses precomputed screen layout rectangles, not guessed
  hard-coded pixel jumps.
- Input during shell fades is ignored consistently.
- UI capture prevents the same pointer/key activation from firing a gameplay action.

### Command center

The command center must support:

- Pointer drag for sliders.
- Left/right arrow adjustment while focused.
- Shift-modified fine adjustment.
- Enter/Space toggles and buttons.
- Visible focus.
- Escape to cancel text editing or close the panel.
- Tab remains the panel open/close key and is not repurposed as internal focus traversal.

Common gamepad navigation is a player-facing requirement; it is optional for the
developer command center.

## Icons

System icons should be code-drawn from lines, triangles, circles, and filled polygons so
they remain crisp, tintable, and dependency-free in raylib. Define one consistent stroke
and size system:

```text
12: caption glyph
16: compact control
20: normal control
28: ability/system tile
40: large mode/role emblem
```

Initial named glyphs:

- Back, close, settings, controls, practice, quit.
- Previous, next, scroll, confirm.
- Health, damage, range, reload, mobility.
- Main attack, super, cooldown, ready.
- Ally, enemy, gem, timer, victory, defeat.
- Mouse buttons, keyboard, and generic gamepad face/shoulder controls.

Do not use emoji as structural icons. Ability-specific illustrated art can later replace
the procedural behavior glyphs without changing component APIs.

## Shared component set

Player components:

- `UiPanel`
- `UiButton`
- `UiIconButton`
- `UiPrimaryButton`
- `UiSegmentedControl`
- `UiKeycap`
- `UiStatusChip`
- `UiProgressBar`
- `UiAmmoPips`
- `UiStatRow`
- `UiAbilityTile`
- `UiRosterRow`
- `UiModal`
- `UiToast`
- `UiScrollRegion`

Developer components:

- Dense section heading.
- Dense button/toggle/cycler.
- Slider with focus, step keys, typed numeric value, units, and project-difference mark.
- Sticky provenance status.
- Dirty-count badge.
- Save/reset action footer.

Developer widgets consume `UiTheme`, `UiResources`, `UiInputState`, and layout scale, but
do not reuse the large player button/card layout.

## Motion

Named durations:

| Token | Duration | Use |
|---|---:|---|
| Press | 90 ms | Internal surface compression |
| Hover/focus | 160 ms | Border/fill transition |
| Panel | 220 ms | Modal and dock appearance |
| Screen | 280 ms | Menu-to-roster spatial transition |
| Result | 320 ms | One result reveal sequence |

Rules:

- UI motion uses opacity and transform equivalents; component bounds stay stable.
- One selection transition moves identity/model/roster as a coordinated unit.
- Ready and urgent states share one calm cadence.
- Exit is shorter than entry.
- Animations are interruptible.
- Reduced motion sets navigation/modal duration to effectively zero and disables
  decorative orbit, pulse, sweep, and podium bob.
- Gameplay-critical rain, wave, projectile, and hit-area visuals remain visible under
  reduced motion.

## Persistence and source of truth

### Project-scoped values

The following are eligible for `config/gameplay.cfg` because they are authored
presentation defaults:

- Character preview home/select yaw.
- Character preview home/select scale.
- Character preview home/select model offset.
- Character preview home/select camera target height and distance.
- Menu-scene key/rim intensity if the implementation needs live authoring.

They participate in the existing local-draft and explicit project-promotion workflow.
They are validated transactionally and appear in the command center's presentation
controls.

### Profile-scoped values

Store with ignored profile state:

```text
profile.ui_scale
profile.reduced_motion
profile.high_contrast
profile.show_tutorial_hints
profile.input_glyph_mode
profile.tutorial_flags
```

These values:

- Never appear in project override counts.
- Never become tracked project defaults.
- Autosave through the existing profile path.
- Have safe recovery defaults.

`UiPreferences` should be a separate member of `App`, even if the existing typed config
registry writes the same `profile.cfg` file. Do not add more UI fields to `Tuning`.

### Source-authored values

These remain code/assets:

- Theme palette and semantic mappings.
- Font choices.
- Spacing, shape, and motion tokens.
- Icon geometry.
- Component state recipes.

## Character presentation profiles

Add:

```c
typedef struct CharacterPreviewProfile {
    Vector3 modelOffset;
    float yawDegrees;
    float scale;
    float cameraTargetY;
    float cameraDistance;
} CharacterPreviewProfile;

typedef struct CharacterPresentationDefinition {
    Color accent;
    CharacterPreviewProfile home;
    CharacterPreviewProfile select;
} CharacterPresentationDefinition;
```

`ContentCatalog` owns one validated definition per character and exposes:

```c
const CharacterPresentationDefinition *
ContentCharacterPresentation(const ContentCatalog *catalog, BrawlerClass cls);
```

Validation rejects:

- Non-finite values.
- Scale outside a conservative visible range.
- Extreme camera distance or target height.
- Unknown or duplicate character IDs in external data, if the profile becomes external.

Required initial behavior:

- Tank home yaw: +25° relative to the camera-facing base.
- Tank select yaw: 0° relative to the camera-facing base.
- Every other character starts at 0° unless visual QA approves a deliberate pose.
- Primitive fallbacks use the same profile.

The menu scene uses the profile selected for the current screen during both update and
draw. It never relies on yaw or offset left by a previous screen.

## Menu presentation scene

Create `presentation/menu_scene.[ch]` and remove asset/model drawing from reusable menu
components.

Scene contents:

- Existing podium geometry and character idle animation.
- A shallow hangar/deployment-bay background assembled from existing station panels,
  rails, display walls, pipes, and floor details.
- One warm industrial key, one cool rim/fill, and a restrained podium/team glow.
- A contact shadow and protected silhouette zone.
- Low-contrast depth layers that never compete with menu text.
- A debug overlay for model bounds, protected zones, camera target, and current profile
  values.

The scene must:

- Draw before player UI.
- Share existing optional/fallback asset behavior.
- Continue when an imported model is missing.
- Avoid post effects that blur UI; UI always draws after any scene post pass.
- Use reduced-motion preferences for decorative orbit/bob only.

## Screen migrations

### Home: launch deck

Target hierarchy:

1. Game title and subordinate Controls, Settings, and Quit utilities.
2. Center protected character stage.
3. Bottom active-brawler control.
4. Bottom active-mode control.
5. Practice and the primary Deploy action.

Behavior:

- `Deploy` is the only primary action.
- Mode selection uses explicit previous/next or segmented controls, not hidden
  “click to change” behavior.
- The active-brawler control leads to roster selection.
- Combat stats, ability descriptions, and comparative telemetry do not appear here;
  those belong to Brawler Select.
- Practice remains session-only and does not alter the Play mode.
- Quit is separated and styled as destructive utility.
- The five imported/fallback silhouettes must fit the protected stage at every supported
  viewport.

### Controls overlay

- Group rows into Movement, Combat, Brawler ability, and System.
- Use keycap/glyph components.
- Show bindings for the active input glyph mode.
- Close button, Escape/B, and click outside dismiss consistently.
- Restore focus to the invoking control.
- Keep all groups visible at 960×600 through scrolling if required.

### Settings overlay

Add a player-facing settings overlay containing:

- UI scale.
- Reduced motion.
- High-contrast gameplay cues.
- Tutorial hints on/off.
- Input glyph selection: Auto, Keyboard/Mouse, Gamepad.
- Reset tutorial hints.

Changes preview immediately and persist as profile state. No setting mutates gameplay
tuning or tracked project config.

### Brawler select: roster bay

State split:

- `candidateKit`: currently previewed row.
- `selectedKit`: committed profile choice.

Behavior:

- Entering the screen sets candidate to the committed selection.
- Pointer hover may preview after a short stable dwell; pointer click or navigation
  chooses the candidate.
- `Select <name>` commits, marks profile dirty, and returns home.
- Back returns without changing the committed selection.
- A fixed ordered five-choice row keeps the complete roster visible at the bottom.
- Pointer, keyboard, and gamepad navigation are supported.
- The left detail pane owns identity and ability descriptions, the centered stage owns
  the candidate silhouette, and the right telemetry pane owns live combat stats.
- Roster choices remain concise and distinguish candidate preview from committed
  selection.
- Presentation profiles independently frame home and select poses.

### Match HUD

Layer order:

1. World-adjacent health/team/gem indicators.
2. Top-center objective broadcast.
3. Bottom-left player vitals and ammo.
4. Bottom-right mobility/super ability dock.
5. Contextual tutorial chip.
6. Downed/result/modal overlays.
7. Command center and transition fade.

Requirements:

- Objective state remains stable in one top-center container.
- Team totals use position, icon, label, and color.
- Personal vitals do not duplicate every world-bar detail.
- Ability tiles expose unavailable, cooldown, ready, aiming, and active states.
- Super readiness uses gold plus text/border, not pulse alone.
- HUD does not shake, scale, or translate in response to attacks.
- Debug/command-center surfaces remain visually distinct from the player HUD.

### Action-based tutorial

Replace the 22-second fixed block with individual tutorial goals:

- Move.
- Aim main.
- Fire main.
- Quick/auto aim.
- Use mobility when the chosen kit has it.
- Aim/use super when charged.
- Open command center in practice.

The app reports input actions to `HudObservePlayerInput()`. A prompt retires after the
action is demonstrated, not after a timer. Completion persists through
`profile.tutorial_flags` and can be reset in Settings.

Only one prompt appears at once. It cannot cover the objective broadcast or ability dock.

### Downed and result states

Downed:

- Keep arena visibility.
- Show clear respawn state and timer.
- Avoid large repeated pulsing.

Result:

- One orchestrated Victory/Defeat reveal.
- Final score.
- Player KOs/deaths and objective contribution already available from live state.
- Explicit `Continue` action.
- Existing configured hold duration remains a safe automatic fallback.
- Input cannot fall through and fire or activate the command center.

## Command-center migration

The command center remains a developer inspector rather than a player menu.

Target structure:

- Wider adaptive panel: approximately 480–520 reference pixels at 1280×800, clamped to
  a sensible portion of smaller viewports.
- Vertical category rail instead of six compressed top tabs.
- Sticky title/provenance header.
- Scrollable control region.
- Sticky save/reset footer.
- Monospaced aligned values and units.
- Clear project, local draft, recovery, and dirty state.

Proposed grouping:

```text
GAMEPLAY
  Match
  Bots
  Player

CONTENT
  Kit
  Map

PRESENTATION
  World
  Visual
  Preview/UI
```

Widget changes:

- Every widget returns a typed change result.
- Rebuild typed content only when a content field changed.
- Return pending commands/restart requests to the app after interaction.
- Sliders support pointer, arrow steps, fine adjustment, and numeric editing.
- Project-different fields receive a visible marker.
- Save labels name their destination.
- `SAVE KIT + FRAMING AS PROJECT DEFAULT` remains character-scoped.
- `SAVE ALL AS PROJECT DEFAULTS` stays explicit and visually separated from discard.
- Recovery-mode messaging remains prominent.

## Frame orchestration

Target menu frame:

```text
ShellUpdate
UiSystemBeginFrame
MenuUpdate → UiActions
apply screen/profile actions
MenuSceneUpdate
BeginDrawing
  MenuSceneDraw
  MenuDraw
  MenuOverlayDraw
  UiFeedbackDraw
  ShellDrawFade
EndDrawing
UiSystemEndFrame
```

Target match frame:

```text
ShellUpdate
UiSystemBeginFrame
CommandCenterUpdate
capture PlayerInput with UI capture state
simulate match
consume events / update presentation
HudObservePlayerInput
render world through direct or post path
  HudDraw
  CommandCenterDraw
  UiFeedbackDraw
  ShellDrawFade
UiSystemEndFrame
```

The same physical press must not both activate UI and perform gameplay input.

## Implementation milestones

The estimates below are relative planning ranges for one developer familiar with this
codebase. They are not calendar commitments.

### Milestone 0 — baselines and safety harness

Relative size: Small  
Estimate: 1–2 focused days

Work:

- Capture current home, roster, controls, HUD, downed, result, and all command tabs.
- Record current click areas, screen transitions, and config behavior.
- Add a UI smoke checklist keyed to supported viewports and input modes.
- Add a temporary layout/focus debug overlay switch.
- Confirm all current project checks pass before structural changes.

Exit:

- Baseline evidence exists.
- No behavior change.
- Build, architecture, config, test, and sanitizer targets pass.

### Milestone 1 — theme, fonts, text, and layout

Relative size: Large  
Estimate: 3–5 focused days

Work:

- Add licensed fonts and source notices.
- Implement `ui_theme`, `ui_resources`, `ui_text`, and `ui_layout`.
- Add fallback font behavior.
- Add semantic text roles and all alignment/wrapping helpers.
- Route one non-critical screen region through the new services as a vertical slice.
- Add pure layout tests at four supported sizes.

Exit:

- Fonts load/unload once without leaks.
- Text roles match the approved spec.
- No new raw theme colors in migrated code.
- Layout helpers pass geometry tests.

### Milestone 2 — components, input, focus, and preferences

Relative size: Large  
Estimate: 3–5 focused days

Work:

- Implement component state recipes.
- Implement pointer/keyboard/gamepad modality and stable focus IDs.
- Add `UiPreferences` and profile persistence.
- Add Settings overlay.
- Add focus/layout debug visualization.
- Add headless focus and profile/config tests.

Exit:

- A component gallery or debug screen demonstrates every state.
- All player controls meet minimum target size.
- Focus survives resize, modal open/close, and screen return.
- Reduced motion and UI scale apply immediately.

### Milestone 3 — presentation profiles and menu scene

Relative size: Medium  
Estimate: 2–4 focused days

Work:

- Add character presentation definitions and validation.
- Add project-config fields and authoring controls for preview fit.
- Extract podium/model drawing from `menu.c` into `menu_scene`.
- Build the station/hangar background from existing assets.
- Add model/protected-zone debug overlay.

Exit:

- All five models and fallbacks fit home/select stages.
- Tank is +25° only on home and front-facing in select.
- Switching screens never carries presentation state.
- Missing assets still produce a coherent fallback.

### Milestone 4 — player menu migration

Relative size: Extra large  
Estimate: 4–6 focused days

Work:

- Implement launch deck.
- Implement explicit mode selection and Deploy hierarchy.
- Implement grouped Controls overlay.
- Implement settings access.
- Implement roster bay and candidate/commit behavior.
- Remove the old `Card` helper and obsolete menu globals after migration.

Exit:

- Home, controls, settings, and roster match the approved information hierarchy.
- Every action works with pointer, keyboard, and controller.
- Back/cancel behavior is predictable and restores focus.
- Copy and models fit all supported viewports.

### Milestone 5 — HUD, tutorial, downed, and results

Relative size: Large  
Estimate: 3–5 focused days

Work:

- Migrate overhead bars and team cues.
- Implement objective broadcast, vitals, ammo, and ability dock.
- Implement action-based tutorial persistence.
- Implement downed and result compositions.
- Consolidate motion cadence and reduced-motion variants.
- Remove superseded HUD drawing helpers.

Exit:

- Combat remains readable under every supported post-effect extreme.
- Tutorial state retires per action and can be reset.
- Results have explicit Continue plus safe timeout.
- No attack or UI state shakes the camera.

### Milestone 6 — command center

Relative size: Large  
Estimate: 3–5 focused days

Work:

- Add explicit `CommandCenterState`.
- Adopt shared font/theme/input services.
- Build rail/header/content/footer layout.
- Split coherent tab groups into separate source files.
- Add focused sliders, fine steps, numeric edit, and dirty markers.
- Rebuild typed content only on actual edits.

Exit:

- Every current control and action remains available.
- Local/project/recovery provenance is always visible.
- Save/reset actions retain their current scope and validation.
- Mouse input never leaks into gameplay.

### Milestone 7 — resizing and final world integration

Relative size: Large  
Estimate: 3–5 focused days

Work:

- Enable resizable window and minimum size.
- Implement resize-safe scene/depth target recreation.
- Validate menu scene, match world, post effects, UI, and pointer mapping.
- Tune menu lighting, contrast, and decorative motion.
- Add high-contrast cue variant.

Exit:

- Four supported viewport sizes pass the full screen checklist.
- No clipped copy, inaccessible target, stretched panel, or stale post target.
- Direct-render fallback works if target recreation fails.

### Milestone 8 — hardening and documentation

Relative size: Medium  
Estimate: 2–4 focused days

Work:

- Run full graphical matrix for five kits and fallbacks.
- Check contrast pairs and non-color state cues.
- Exercise all navigation with keyboard and gamepad.
- Profile frame cost and remove per-frame allocations/redundant rebuilds.
- Remove transitional code and dead globals.
- Update architecture, development, README, config, and root overview documents.
- Change the HTML proposal status only where the runtime now matches.

Exit:

- Definition of done below is satisfied.
- All automated and interactive gates are reported.

### Overall size

Expected implementation scope: roughly 24–41 focused engineering days for one developer,
depending mainly on controller focus behavior, resize/post-target edge cases, and the
amount of final visual tuning needed for all character silhouettes.

This is intentionally a sequence of stable checkpoints. It can be implemented without
pull requests, but each milestone should leave a buildable, documented state.

## Verification plan

### Automated gates after every milestone

```bash
make -C brawl_arena
make -C brawl_arena check-architecture
make -C brawl_arena validate-config
make -C brawl_arena test
make -C brawl_arena sanitize
```

Character assets remain covered by the normal build/test path.

### New headless coverage

Add a narrowly linked `test_ui` executable covering:

- Safe/content rectangles at each supported viewport.
- Minimum player interaction target size.
- Focus graph movement and wrap/clamp rules.
- Modal focus restoration.
- Roster candidate versus committed selection.
- Character preview validation and Tank home/select defaults.
- UI preference parsing, rejection, profile save, and round trip.
- Reduced-motion duration mapping.
- Theme contrast for approved text/surface pairs.

Keep GPU/font rasterization out of the headless assertions.

### Static policy checks

Extend the architecture check or add a small `check-ui` script to reject:

- Direct `DrawText`/`MeasureText` calls in migrated screen files.
- Font loading outside `ui_resources.c`.
- UI or presentation includes from `src/game`/`src/core`.
- Player-facing input targets below the declared minimum in layout fixtures.
- Reintroduction of attack-driven camera shake.

### Interactive matrix

Screens:

- Home.
- Controls.
- Settings.
- Roster.
- Practice HUD.
- Gem Grab HUD.
- Downed.
- Victory.
- Defeat.
- Every command-center destination.

Content:

- All five brawlers.
- Rigged Scrapper, Longshot, Tank, Guardian.
- Primitive Mortar.
- Forced primitive fallback for each imported slot.
- Both maps.

Input:

- Mouse-only.
- Keyboard-only.
- Gamepad-only for player-facing UI.
- Mid-screen modality switching.

Viewports:

- 960×600.
- 1280×800.
- 1920×1080.
- 2560×1440.

Preferences:

- UI scale minimum/default/maximum.
- Reduced motion on/off.
- High contrast on/off.
- Tutorial hints fresh/completed/disabled.
- Keyboard and gamepad glyph modes.

Rendering:

- Post effects off.
- Default toon/post values.
- Maximum supported bloom/outline.
- Pixelate/painterly/halftone/posterize extremes.
- Imported asset failure fallback.

## Performance budgets

- No per-frame heap allocation in UI, HUD, or command-center drawing.
- No font/model/texture load after initialization except explicit viewport target resize.
- Fixed focus-node capacity with an overflow diagnostic.
- UI update/draw should remain comfortably below 1 ms at 1280×800 on the current
  development machine, excluding 3D menu scene rendering.
- Typed content rebuild occurs only after an authoring value changes.
- Text formatting uses bounded stack buffers.
- Resize target recreation occurs only when framebuffer dimensions actually change.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| New font metrics cause clipping | Migrate by named text roles; validate copy at max UI scale before polishing |
| Controller focus becomes brittle in dynamic lists | Stable IDs, explicit layout rectangles, pure navigation tests |
| Menu UI steals gameplay input | Begin UI frame before player capture; one capture contract; regression smoke test |
| Resizing breaks custom depth target | Isolate recreation API; direct-render fallback; test repeated resize/fullscreen cycles |
| One profile cannot frame every idle pose | Per-character, per-screen profile plus authoring debug overlay |
| Command-center refactor changes save semantics | Preserve existing Config APIs; add provenance/config regression tests before layout migration |
| Too many modules create ceremony | Split only along ownership shown in the target tree; avoid one-widget files |
| Theme drifts during screen work | Semantic tokens and a static check; no screen-local palettes |
| Reduced motion hides useful combat information | Disable decorative UI motion only; retain gameplay telegraphs |
| Visual regressions are hard to automate across GPUs | Headless geometry/state tests plus reviewed reference screenshots and interactive matrix |
| Existing unrelated work overlaps touched files | Re-check the worktree before every milestone and preserve concurrent character-pipeline changes |

## Definition of done

The Helios Broadcast integration is done when all of the following are true:

### Visual system

- Every player-facing screen uses the shared theme, text, layout, component, and icon
  services.
- Barlow/IBM Plex fonts are shipped locally with license/source records.
- Curated UI art is shipped with license/source records, reproducible motif generation,
  hash/dimension checks, and no downloaded archives.
- No default raylib font is used during normal startup.
- No superseded menu/HUD palette or card helper remains.
- The station signature is visible without decorative clutter.

### Menus

- Home, controls, settings, and roster match the approved hierarchy.
- Deploy is the only primary home action.
- Home contains no combat telemetry; Brawler Select owns character detail.
- Every current action remains reachable.
- Candidate selection does not overwrite the committed character until confirmed.
- Every character and fallback fits at every supported viewport and UI scale.
- Tank retains its approved home pose and is front-facing in select.

### HUD

- Objective, survival, ability, tutorial, downed, and result states have distinct stable
  regions.
- Team and readiness information does not rely on color alone.
- Tutorials retire by demonstrated action.
- Results expose Continue and retain a safe timeout.
- No attack shakes any user's camera.

### Input and accessibility

- Mouse, keyboard, and gamepad operate every player-facing screen.
- Focus is visible and restored correctly.
- UI scale, reduced motion, high contrast, tutorial hints, and glyph choice persist.
- Player targets meet minimum sizes.
- Supported text does not clip at maximum UI scale.
- The documentation does not claim native screen-reader support.

### Developer tools and tuning

- Every existing command-center control still works.
- Project versus local/profile state remains explicit.
- Project promotion remains validated and atomic.
- Preview fit values can be authored through the established project-draft workflow.
- UI preferences never become project defaults.

### Engineering

- Ownership follows the target architecture.
- There is no UI dependency in deterministic simulation.
- Normal build, architecture, config, test, and sanitizer gates pass.
- New UI headless tests pass.
- The full interactive matrix is completed and reported.
- README, architecture, development, config, visual design, and repository overview
  documents match implemented behavior.

## Recommended first implementation checkpoint

Implement Milestones 0 and 1 together:

1. Capture baselines.
2. Add licensed font assets.
3. Add `ui_theme`, `ui_resources`, `ui_text`, and `ui_layout`.
4. Migrate the controls modal as the first vertical slice.
5. Add pure layout tests.

The controls modal is the safest proving ground: it exercises titles, body text,
keycaps, panels, buttons, wrapping, focus-ready geometry, modal dimming, and small-screen
scrolling without touching match rules, model framing, or the primary home flow. Once it
is correct, the same services can support the larger home and roster migrations.
