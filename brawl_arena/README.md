# Brawl Arena

A top-down 3D arena brawler in the Brawl Stars mould, built with raylib in C.

This build is a playable **Gem Grab combat slice**: two external maps, five kits, 3v3 team play,
a no-pressure practice range, and a live tuning panel. It keeps movement, aiming, and
weapon feel easy to iterate while providing a complete match loop and win condition.

Bots stand still and do not shoot back by default, so you can dial weapons in without
being interrupted. Switch them to `ROAM` or `FIGHT` in the command center when you want
a real opponent.

## Quick Start

```bash
cd brawl_arena
make run
```

Requires raylib 5.5+ (`brew install raylib`).

## Screens

The **launch deck** is a deliberately quiet deployment screen: the game title and
utilities sit above an open character stage, while the bottom rail contains the active
brawler switcher, active mode, Practice, and the single primary **DEPLOY** action. Combat
stats and ability copy stay out of this screen so choosing a session remains the focus.

**BRAWLERS** opens the roster bay. Navigation changes a candidate preview; **SELECT**
commits that candidate to the profile, while Back leaves the previous selection intact.
This is the detailed character view: identity and abilities sit beside the centered
model, field telemetry sits opposite, and all five concise character choices remain
visible along the bottom. The readouts derive health, attack, range, cooldown, ammo, and
ability descriptions from the live content catalog, so tuned values remain accurate.

Controls and Settings are modal overlays. Settings persist UI scale, reduced motion,
high-contrast combat cues, tutorial visibility, and keyboard/gamepad glyph preference to
the ignored player profile. Menu, roster, overlays, result screen, and tuning controls
support pointer and keyboard focus; player-facing screens also support gamepad focus.

The shell and HUD use the **Helios Broadcast** skin. Neutral CC0 hardware from Kenney's
UI Pack - Sci-Fi supplies scalable bolted panels, buttons, keycaps, and progress frames;
two cropped CC0 OpenGameArt motifs supply the orbital/radar stage linework. Both are
tinted through the existing Helios palette, while Barlow/IBM Plex text and code-drawn
icons remain authoritative. Resources load once through `UiSystem`, and every textured
primitive retains the previous geometry fallback if a file is unavailable. Source,
license, curated-runtime, and derivative records live together under `resources/ui/`.

During a match, health and Scrapper shield points are centered inside the bars above
each brawler. The player and allied bots always use green health fills; opponents always
use red, even at low health, with ally/enemy shape icons retained as a second cue. The
player's ammo pips remain beneath the body-anchored bar. There is no duplicate
bottom-left health/ammo panel.

`ESC` closes the nearest overlay or command center first, then steps back a screen:
roster to menu, match to menu, and menu to quit. raylib's default escape-to-close behavior
is disabled so this navigation remains explicit.

## Controls

| Input | Action |
|-------|--------|
| `WASD` / arrows or left stick | Move |
| `Left Shift` | Use the active secondary: Scrapper holds its Shell; Longshot holds to preview and releases to grapple; Mortar plants a mine; Tank fires Shoulder Jets |
| Mouse or right stick | Aim |
| Hold/release `LMB` or right trigger | Preview/fire the main attack |
| Tap `LMB`, `SPACE`, or gamepad A | Quick shot; Guardian prioritizes a badly hurt ally, otherwise targets the nearest visible enemy |
| Hold/release `RMB` or right bumper | Preview/fire the super |
| Left bumper | Use the active secondary |
| `1` – `5` | Swap kit on the spot |
| `TAB` | Open / close the command center |
| `ESC` | Back a screen — select to menu, match to menu, menu quits |
| `R` | Reset the match (keeps your tuning) |

## What's implemented

**Aim-and-release shooting.** Holding the fire button draws the actual shot on the ground
as a solid shape, matched to how the weapon behaves:

- **A filled cone** for spread weapons, opening to the real spread angle.
- **A thick beam** for single-line weapons, Longshot's tight parallel pair, returning
  saws, and the dash charge.
- **A filled disc** for lobbed shots, one per shell, sitting where the splash will land —
  plus a dotted arc showing the flight path over any wall in between.
- **A green target disc** for Guardian's growing rain field.
- **A wide cyan cone** for Guardian's Resonance sound wave.

Every rib of the cone and the center of the beam are raycast, so the shape is
clipped flat against whatever wall it runs into rather than passing through it: you can
read exactly where a shot dies before committing. Supers preview in gold, ordinary shots
in blue, and Guardian support previews in green. A quick tap instead fires an auto-aimed
shot with target leading, standing in for mobile's tap-to-shoot.

**Turn-and-fire.** An auto-aimed shot snaps the brawler around to face its target,
holds that facing for a beat, then eases back to whichever way it is running. The shot
itself still leaves instantly along the true aim line, so the turn is presentation only
and never costs you accuracy.

**Ammo economy.** Capacity is configured per kit (all tracked defaults currently use
three pips), with each pip reloading continuously and independently. You cannot hold the
trigger; you manage a small budget of shots and reposition while it refills.

**Wall sliding and body-aware routing.** Brawlers move as swept circles in bounded
substeps, so frame spikes and tuned movement speeds cannot skip through a wall or crate.
Contact removes only velocity aimed into cover, preserving responsive movement along
wall faces and around corners. Brawlers do not collide with one another, so players,
allies, and enemies can overlap and cross freely without changing terrain collision.
Bots use the same body clearance for short probes and a live tile route when a direct
path is blocked; destroyed crates open that route on the next update.

**Out-of-combat regeneration.** Every living brawler follows the same recovery rule.
After three gameplay seconds without successfully firing a main attack or ultimate and
without losing health, the brawler immediately restores 13% of maximum health. Further
pulses arrive once per gameplay second until full. Real damage—including each
damage-over-time pulse—restarts the delay; failed attacks, movement, aiming, Shoulder
Jets, and receiving healing do not. Recovery caps at maximum health and never revives.

**Scrapper return-and-shell loop.** Ripsaw deals 700 damage outbound and can damage each
target once again while returning to Scrapper's current position. Its 13-unit outbound
path turns at range, walls, or crates; the ordinary saw never damages cover. Catching
the saw is visual only—ammo begins reloading from the cast as usual.

Wrecking Disc extends that pattern to 18 units and 1,100 damage per leg. It breaks and
passes through crates, pulls enemies toward the outbound line, and knocks them along the
return path. Permanent walls turn its outbound leg or destroy its returning leg.

Hold Left Shift/left bumper for Magnetic Scrap Shell, a 1,200-point bubble that catches
hostile combat damage from every direction. Scrapper moves at 65% speed and cannot fire
another ability while the shell is raised. Every absorbed point spends one point of
charge and restores health equal to 30% of the absorbed damage; overflow alone reaches
health, so Tank lifesteal still keys off real health removed. Releasing preserves the
remaining charge. After three seconds without shield or health damage it refills at 300
points per second. A full break forces the shell down for five seconds, restores it to
full, and requires release before it can be raised again. Pulls and knockback still
apply on shield contact, and shield hits interrupt ordinary out-of-combat regeneration.

**Tank sustain and mobility.** Tank's tracked Reclamation Rounds restore about 49.9% of
the enemy health actually removed by each main-attack pellet. Overkill cannot create
extra healing, healing stops at maximum health, and hits against crates, Charge damage,
and shots fired by other kits do not feed it.

Shoulder Jets is a separate Left Shift mobility ability on a 2.5-second cooldown. It
travels for 0.18 seconds at 22 world units per second (about four world units), using
movement input first and current aim when stationary. It deals no damage, grants no
invulnerability, stops on walls or crates, and cannot destroy cover. The charged Charge
super remains the longer damaging dash that knocks enemies back and smashes crates.

**Longshot twin shot.** One ammo cast launches two zero-spread bolts on parallel lanes
only 0.22 world units apart, so the attack keeps the silhouette and aiming language of
one precision round. In the tracked project tuning each bolt carries 625 base damage
and 0.15 super charge, preserving the previous 1,250 combined damage and 0.30 combined
charge when both connect; range scaling still applies independently to each bolt. The
compiled recovery baseline uses the same split with 800 per bolt and 1,600 combined.

**Longshot grapple.** Hold Shift/left bumper to aim Mag-Line Grapple, then release to
launch it along the current aim direction. The ground preview draws the exact body-safe
path and endpoint plus a 10-world-unit maximum-range ring; cover-shortened endpoints
turn amber and unusably short paths turn red. A valid release starts the 7.5-second
cooldown and visibly sends the hook tip down the cable over 0.25 seconds before pulling
Longshot to the previewed point over 0.45 seconds. Permanent walls and crates shorten
the endpoint without taking damage; actors do not block the cable. Longshot can move
while aiming but cannot fire a competing ability, cannot fire during launch or pull,
and an external pull/knockback cancels traversal without refunding the cooldown. Fight
bots reserve the direct activation path for retreating.

**Mortar mine control.** Concussion Mine places one persistent charge at Mortar's feet
on an 8-second cooldown. It arms after 0.55 seconds, ignores its owner and allies, then
triggers when a visible enemy enters 2.4 units. The 3.2-unit blast deals 400 damage and
4.5 units of knockback but grants no super charge. Walls and crates block detection and
blast damage. Planting again replaces Mortar's old mine; death, class change, or match
reset removes it. Fight bots plant it when an enemy presses into the trigger zone.

**Supers charged by useful hits.** Every damaging pellet that lands fills the meter, as
does each Guardian rain pulse that damages an enemy or actually restores an ally; a
takedown tops it up. No timers — participating in the fight is what earns your super.

**Grass you can hide in.** Bush tiles grow a field of instanced blades standing about as
tall as a brawler. Standing in one hides you from the bots entirely, unless you fire
(which reveals you for a second) or someone walks within ~3 units.

The grass is a single instanced cross-quad mesh with all of its motion in the vertex
shader:

- **Wind** displaces X and Z weighted by height up the blade, so roots stay planted and
  only the tips travel. Two desynchronised sine waves, phase-offset by world position,
  keep the field from swaying like a metronome.
- **Brawlers push blades aside.** Every brawler's position and travel direction goes to
  the shader, and blades within the bend radius lean away with a squared falloff, biased
  along the direction of travel. Walking through leaves a parting behind you; standing
  still clears a pocket around you. Passing actor positions as uniforms only scales to a
  handful of characters, which is exactly the case here (max 8) - a bigger cast would
  need the displacement baked into a render target instead.
- **Only brawlers you can legitimately see bend the grass.** A concealed enemy is left
  out of the actor list entirely, because rustling blades would otherwise track them
  through cover and undo the hiding place. The moment something reveals them - firing,
  or you closing to within the reveal range - they start disturbing the field again.
- Blade footprints are **kept inside their own tile**: the jitter budget is whatever is
  left after subtracting the quad's half-width from the half-tile, so the field lines up
  with the floor grid rather than bleeding over onto bare ground. Density comes from the
  count per tile instead of from oversized quads.
- Blades are **alpha cutout, not alpha blended**, so the depth buffer resolves draw order
  and no per-frame sorting is needed.

**Concealment reads as a ghost.** A brawler standing in grass dissolves on a 4x4 Bayer
pattern with a green cast over them. Screen-door transparency beats real alpha here: it
needs no blending, so it can never sort wrongly against the grass drawn over it. A soft
pulsing ring on the ground, drawn after the grass with depth testing off, means you can
always find yourself however deep in the field you are.

**Cover reads at a glance.** The two kinds of hard cover are deliberately opposite so you
never have to think about which is which mid-fight:

- **Crates** are warm planked wood with metal banding, sitting flush on the floor. They
  block movement and shots, but supers blow them apart and the dash smashes straight
  through, so the map opens up as a fight goes on.
- **Walls** are cold bolted steel plating - recessed panel seams, a stud in each plate,
  and a wider skirt where they meet the ground. They never break. The plating is on the
  top face as well as the sides, because from this camera angle the top is most of what
  you actually see.

**Bots** with three modes, set from the command center:

- `STATIC` (default) — inert targets. They hold position and never fire.
- `ROAM` — they wander the arena but never open fire, for testing tracking and aim.
- `FIGHT` — the full state machine: patrol when blind, chase to weapon range, then hold
  that range while strafing and firing with lead prediction. They retreat toward the
  nearest bush below 30% health, use dash secondaries to close meaningful gaps or
  retreat, raise Scrapper's Shell for projectiles predicted to arrive within roughly
  0.5 seconds, lower it after the threat passes so it can recharge, and fire supers only
  when the shot will connect.

## The command center

The panel starts closed so a match opens on unobstructed gameplay. Press `TAB` to open
or close it.

Effective tuning lives in `App.tune` and its owned `ContentCatalog`, sourced from tracked
`config/gameplay.cfg`. Sliders apply immediately, rebuild typed runtime content, and
autosave an ignored local draft; roster and rule changes that alter match construction
request a rebuild. Clicks over the panel never reach the game, and each tab scrolls
independently when its controls are taller than the window.

| Tab | What's in it |
|-----|--------------|
| **MATCH** | Gem Grab toggle, team size, target count, countdown/vent timing, match state, and objective actions |
| **BOTS** | Behaviour mode, bot count (0–7), mixed or fixed kits, respawn delay, AI health thresholds/probe distance, and respawn / kill / heal buttons |
| **PLAYER** | Active kit, god mode, infinite ammo, move speed, acceleration, dash speed, respawn delay, plus charge-super and heal buttons |
| **KIT** | Live edit of the active kit, including ammo capacity and behavior-specific main/super/secondary values. Scrapper exposes Shell lifecycle; Longshot exposes Grapple range/launch/pull timing; Mortar exposes Mine arm/trigger/blast/damage/knockback; Tank exposes Shoulder Jets and self-healing; Guardian exposes rain and Resonance timing, plus reset/save-as-project actions |
| **VISUAL** | Post-processing master switch, toon controls, painterly/pixel/print effects, color grade, bloom, vignette, grain, and chromatic fringe |
| **WORLD** | Time scale, super gain, out-of-combat regeneration delay/interval/ratio, crate/result timing, stealth, grass, 20–60-unit match-camera distance, rendering, debug, live VFX diagnostics, direct effect/action previews, draft discard, and `SAVE ALL AS PROJECT DEFAULTS` |
| **PREVIEW / UI** | One shared home/roster showcase transform and camera plus personal UI scale, motion, contrast, tutorial, and glyph preferences |

Editing a kit's max health updates living brawlers of that class immediately, keeping
their health ratio, so you can feel a change without respawning.

The debug overlay draws each brawler's weapon range and body radius, plus a sight line
to the player — green when that bot can actually see you, red when a wall or bush is
blocking it. It's the fastest way to check the stealth rules are doing what you expect.

### The sandbox

**PRACTICE** from the menu drops you into a firing range: three static targets fanned out
ahead of you at stepped distances, no objective, and no return fire. On a fresh
application launch, the command center starts closed just as it does when Play is the
first selected mode; `TAB` opens the tuning panel. Reading weapon range and damage
falloff off the screen is the whole point, so the targets spawn in front of you rather
than at the far end of the map, and the spawner nudges them clear of walls and bushes so
nothing hides.

The sandbox is a **session**, not a saved setting. Entering it never rewrites which mode
`PLAY` gives you, and the MATCH tab says so rather than showing a Gem Grab toggle that
looks live but is not. Leaving to the menu ends the session.

### Drafts, project defaults, and profile state

`config/gameplay.cfg` is tracked and complete: it is the canonical source for every
designer-facing gameplay, kit, AI, match, and default presentation value. Startup
validates it transactionally before constructing a match. Missing, duplicate, unknown,
out-of-range, or internally inconsistent canonical values put the game into a clearly
labelled recovery mode instead of silently creating a hybrid configuration.

Slider edits autosave after 0.6 seconds to ignored `tuning.local.cfg`. It is sparse:
project keys appear only while they differ from `gameplay.cfg`; cheats and debug state
are always local. `profile.cfg` separately owns the selected kit, win/loss/KO record,
personal UI preferences, and completed tutorial actions.

The command center shows `PROJECT DEFAULTS` or `PROJECT + LOCAL DRAFT (N)`:

- `SAVE KIT + SHOWCASE AS PROJECT DEFAULT` promotes the active kit and shared showcase.
- `SAVE ALL AS PROJECT DEFAULTS` promotes all live project-scoped values.
- Reset/discard actions restore the tracked project values and clear matching draft keys.

Promotion validates and atomically writes the tracked file. It creates an ordinary Git
working-tree modification but does not run `git commit`.

If the new local file is absent, an existing `tuning.cfg` is imported once into the
split draft/profile files without modifying the original. Version-1 Tank mobility is
migrated to a dash secondary, its old Scrapper weapon fields are discarded in favor of
the current Ripsaw/Shell kit, and its shared showcase is derived from Scrapper's former
home framing. Version-1 Guardian bolt/Sanctuary numbers are also discarded because they
do not describe the current rain/Resonance behavior. The next save writes the complete
version-3 schema. Version-2 typed files migrate the old Guard kind/capacity/movement
values to Shell while using the new healing and recharge defaults. See
[`config/README.md`](config/README.md) for the schema and authoring
workflow.

## Look

The station deck keeps a generated material underlay and the shaders are embedded as
strings. Static environment pieces and rigged characters load from `resources/`, with
procedural primitives retained as independent fallbacks.

**Lit geometry.** The arena and brawlers are drawn as real meshes through a custom GLSL
330 shader: a half-Lambert key light so shadowed sides stay readable, a Blinn specular
highlight, a rim term that lifts silhouettes off the background, and distance fog for
depth. Immediate-mode `DrawSphere`/`DrawCapsule` calls were dropped because they do not
emit reliable normals — meshes from `GenMesh*` do.

**Dynamic lights.** Muzzle flashes, explosions, crate breaks, in-flight projectiles and
charged supers all emit real point lights that pool on the floor. Every candidate is
scored each frame and the strongest eight go to the GPU.

**Procedural foundations.** Floor tiles with grout lines still produce the readable
gameplay grid without hundreds of floor meshes. Generated metal, wall, crate, grass, and
glow textures support effects and fallback geometry. Imported station panels, walls, and
cover are layered over that foundation.

**Projectiles** are a bright core plus additive glow billboards with a fading trail.
Lobbed shells are different: they are solid geometry, a dark casing with an emissive
team-coloured band, tumbling on three axes as they fly, so a thrown object reads as an
object rather than as a light.

**Explosions** are built in layers so a blast has a shape rather than being a puff:

- A hard white core for a couple of frames, giving the blast an instant.
- Two expanding ground rings - one the full blast colour at the real radius, one tighter
  and hotter - which is what actually communicates how far the damage reached.
- Sparks thrown along the ground rather than up, drawn stretched along their travel so
  they read as streaks instead of dots.
- Solid tumbling debris chunks, so there is something with edges in the middle of it.
- Smoke that lingers after the light has gone.

Shadows are soft radial ground decals rather than hard cylinders.

**Helios control hardware.** Player and developer UI surfaces are opaque physical
controls rather than glass. Nine-slice Kenney textures scale without stretching their
corners or screws, progress fills preserve their framed edges, and sparse
OpenGameArt-derived rings sit behind menu characters and real result/objective data.
`make ui-assets` deterministically rebuilds the two tintable motifs from their retained
source sheets; `make check-ui` verifies dimensions, hashes, provenance files, ownership,
and the absence of downloaded archives.

**Ability VFX** are a reusable presentation library rather than art embedded in a
character model. Stable gameplay events select one of 41 recipes, and each recipe layers
CC0 flipbooks or particle shapes over the existing procedural particles, lights, rings,
and authoritative telegraphs. Scrapper adds saw return/catch and Shell
start/hit/collapse/restore feedback to its industrial sparks. Longshot adds a persistent
cyan cable and separate fire/hook/pull/land phases. Mortar adds a solid armed charge,
exact trigger/blast rings, and place/arm/detonate phases. Tank distinguishes
blue non-damaging Shoulder Jets from gold destructive Charge and draws teal reclaim
energy back from successful hits, and Guardian layers restorative rain and resonance
marks over its real field geometry. Imported recipe layers render at a shared `4.0×`
readability scale; gameplay ranges, hitboxes, field radii, and authoritative telegraphs
retain their simulation-authored dimensions.

Source and target metadata let short-lived effects follow the final animated hand,
shoulder, chest, foot, or center pose instead of remaining at the gameplay actor origin.
The renderer resolves the current Meshy bone pose every frame and retains approximate
sockets for primitive characters or an unmapped bone. The WORLD tab reports loaded
atlases, active layers, pool pressure, dropped effects, consumed events, spawned layers,
the last recipe, and the selected action's progress/blend; its recipe/action preview
controls allow direct visual checks without waiting for a combat outcome.

Seven generated atlases live under `build/assets/vfx/`; their curated sources, notices,
and manifest live under `resources/vfx/` and `data/vfx/`. Alpha smoke is depth-tested and
sorted, additive energy keeps depth testing without writing depth, and ground layers use
separate offsets below the targeting plane to avoid floor fighting. Missing imported
atlases skip only those layers—the procedural feedback and gameplay boundaries remain.
Generated cells bleed color beneath zero alpha and use transparent guards plus
half-texel-inset runtime UVs, preventing black mattes and neighboring-frame edges under
bilinear filtering. No-depth-write presentation passes flush raylib's immediate batch
before changing and restoring the depth mask, so transparent billboard quads cannot
become rectangular silhouettes in the depth-based ink outline.
See [the VFX pipeline](docs/VFX_PIPELINE.md).

**Post-processing** adds thresholded bloom, a vignette, a gentle contrast S-curve, and
FXAA-style edge smoothing. Toggle the pass and dial bloom strength on the WORLD tab.
The depth-based effects use the same 0.5-to-120 clip range as the 3D cameras rather than
raylib's much wider default range, concentrating depth precision around the arena.
Optional grain uses a stable screen-space pattern rather than generating new noise every
frame, so flat station panels retain texture without temporal flicker.
With post-processing enabled, the world renders into a project-scaled color/depth target
and is downsampled before the native-resolution HUD. `presentation.render_scale` ranges
from 1.0× to 2.0× on the VISUAL tab and defaults to 1.5×. Target dimensions derive from
the drawable framebuffer rather than logical UI coordinates, preserving that ratio when
the two differ on a HiDPI platform. The shader receives separate source- and
output-resolution uniforms, so resize and scale changes preserve both scene sampling
and authored screen-space effect sizes.
The WORLD tab also authors `presentation.match_camera_distance` from 20 to 60 world
units. Its tracked default is 38.013156—the length of the original `{0, 31, -22}`
follow offset—so changing it moves the camera along the established pitch without
altering aim lead or vertical framing.

**The VISUAL tab** in the command center is a full mix-and-match viewport rig: toon
banding and ink outlines, painterly (Kuwahara), pixelate, halftone comic dots,
posterize, saturation, brightness, bloom, vignette, film grain and chromatic fringe -
every effect an independent live slider, all persisted, all composable in one post
pass. HUD and menus draw after the pass, so the interface stays crisp whatever the
world looks like.

**The toon look** (VISUAL tab, on by default) turns the scene illustrative: lighting is
quantised into hard cel bands with specular and rim killed, ambient lifted and colours
saturated so shadow bands stay vivid, and a depth-based ink outline is drawn around
every silhouette in the post pass. The outline needs a sampleable depth texture, so the
scene renders into a hand-built scaled framebuffer (raylib's stock render texture keeps
depth in a renderbuffer). If scaled allocation fails, the renderer retries at native
resolution before falling back to direct world rendering; outlines quietly turn off
when no sampleable depth is available and everything else still works. The outline
transition is softened in screen space so it does not introduce a second source of hard
edge shimmer. Band count and ink strength are sliders, everything is live, and toggling
it off returns the original smooth-lit look.

## Imported environment

Gem Grab takes place aboard **Helios-9**, an abandoned ore-processing station built from
Kenney's Space Station Kit. The complete runtime-ready GLB collection lives under
`resources/environment/kenney_space_station/`; only 22 pieces used by this arena are
loaded at startup. The original orange atlas defines cargo and hull areas, while the
included purple variation marks the reactor and gem systems.

`environment.c` translates the live arena grid into presentation without replacing its
rules:

- Permanent wall tiles keep full-tile collision while their visible metal core is
  recessed behind modular wall faces, windows, blast doors, banners, and top panels.
  The tiled wall variants drop only their unused longitudinal end-cap triangles after
  loading, so perpendicular faces share an edge instead of overlaying differently
  coloured atlas regions down the corner. Per-tile base plinths remain inside their
  owning tiles, so adjacent wall blocks also avoid overlapping coplanar top surfaces.
- Destructible tiles render as cargo containers on one side and computer/power banks on
  the other. Damage tint and removal still follow the tile's health.
- Bushes are ruptured hydroponic beds. Their low station panels sit beneath the existing
  animated concealment grass.
- Spawn pads, reactor floor details, pipes, rails, support frames, and exterior work
  platforms are non-gameplay dressing.
- Floor rendering uses explicit deck (-0.08), inset-top (0.02), passive-decal (0.065),
  and targeting/effect (0.12) heights. The separation keeps imported panels, shadows,
  glows, and aim previews from competing for the same depth-buffer values.
- Both station texture atlases use generated mipmaps, trilinear filtering, and 8×
  anisotropic filtering, reducing texture shimmer on panels viewed at shallow angles
  or from the match camera.

The window requests four-sample MSAA and vertical synchronization. MSAA covers direct
rendering while post effects are off, during target recreation, and after a target
allocation failure; the scaled scene target provides the normal post path's additional
edge stability. Vsync owns frame pacing without a competing CPU frame limiter.

The source pack is CC0. `LICENSE.txt` and `SOURCE.md` beside the assets preserve its
license, origin, included formats, and texture-layout requirements. Missing station
models fall back independently to the generated cover, so collision never becomes
invisible.

## Imported characters

SCRAPPER, LONGSHOT, TANK, and GUARDIAN are played as rigged, animated character models -
on the menu podium, in character select, and in the arena. Mortar keeps its primitive
brawler in its accent colour. Menu and character-select previews hold a fixed direction
while their idle animation continues. Every candidate on both screens uses one
project-authorable showcase: 180° yaw, 0.90 scale, zero offset, camera
`(0, 2.7, -7.6)`, target `(0, 1.4, 0)`, and 40° vertical FOV. Candidate changes swap
only the model, so character placement and the hangar background remain stable. In a
match each model
picks its clip from movement (idle, walking, running or dashing), flashes on hit, ghosts
in bushes like everything else, and carries a red cast on enemies and a blue one on
allied bots so a grey model still reads friend-or-foe at a glance. The WORLD-tab toggle
turns rigged models off, and each kit falls back to primitives automatically if its file
is missing.

In a match the clip is picked from the movement direction relative to facing -
forward, backward and four diagonals - so backpedaling and circle-strafing animate
correctly instead of moonwalking, with playback rate following actual speed. Going
down plays a death clip that holds until just before the respawn. A stationary attack
keeps the authored idle as its base while a short, explicitly emitted action overlay,
facing, muzzle light, projectiles, and ability VFX sell the cast. That overlay blends
back out and repeated attacks restart it; it never borrows timing from concealment.
When an optional semantic action clip is unavailable, the renderer applies a procedural
pose. Grapple uses a three-beat reach/brace/tuck motion and Mine Deploy uses a full-body
kneel/plant/recovery motion; the primitive Mortar fallback also crouches while planting.
The bush-reveal timer never selects an animation. The
generic `combat_stance` clip is retained in the asset contract for a future continuous
blended aiming state rather than being played as an unsynchronized firing loop. Tracked
mesh-only models live under
`resources/characters/models/`; the shared twelve-clip `meshy_humanoid_v1` library and
small character-specific overrides live under `resources/characters/animations/`.
`make character-assets` validates the common 24-bone topology, retargets motion relative
to each model's animation rest pose, and writes self-contained raylib assets under
`build/assets/characters/`. Every embedded character texture is exactly 1024×1024 (1K);
`make check-character-assets` validates the model, animation, generated-output, root
motion, mesh-index, and texture contracts. Dense imports are split into raylib-safe
16-bit indexed primitives automatically; Longshot is the current high-detail example.
The current twelve-clip libraries do not contain authored action clips, so all current
rigged characters use those shared procedural `MAIN`, `SUPER`, `CAST`, `MOBILITY`,
`GUARD`, `GRAPPLE`, and `MINE_DEPLOY` overlays. Optional future clips may replace an
overlay by using `attack_main` (or `shoot`), `attack_super`, `cast`, `mobility`, `guard`,
`grapple`, or `mine_deploy`. raylib has no locomotion
crossfade, so base clip changes restart the cycle - a known small pop.

Raw Meshy/Tripo exports do NOT load correctly in raylib - they pass every load-time
check and then render as a collapsed spike-ball. The full story of why (raylib's
glTF loader implements a much narrower contract than the spec), the converter that
fixes it, the checklist for importing the next character, and the debugging traps to
avoid are in **[docs/CHARACTER_PIPELINE.md](docs/CHARACTER_PIPELINE.md)**. Short
version (a compatible Meshy `Character_output` model or standalone merged-animation GLB
is sufficient; repeated animation exports are not required):

    python3 tools/import_character.py <meshy-zip-dir-or-glb> \
      resources/characters/models/<name>.glb --id <name>
    make character-assets

## The five kits

| Kit | HP | Attack | Super |
|-----|----|--------|-------|
| **SCRAPPER** | 3800 | `RIPSAW` — 700 out + 700 back; Shift Shell absorbs 1,200 damage from 360° and heals 30% absorbed | `WRECKING DISC` — 1,100 per leg, breaks crates, outbound pull, return knockback |
| **LONGSHOT** | 2800 | Tight twin shot: two parallel 625-damage bolts, 1,250 combined, scaling from 50% to 100% with travel distance; Shift Grapple pulls up to 10 units | `RAILGUN` — piercing, hits everyone in a line |
| **MORTAR** | 3200 | Arcing lob that clears walls; Shift plants a 400-damage Concussion Mine | `BARRAGE` — three shells in a fan |
| **TANK** | 8009 | 6-pellet burst; heals about 49.9% of actual damage; Shift jets every 2.5s | `CHARGE` — dash that damages, knocks back and smashes crates |
| **GUARDIAN** | 3400 | Growing 3.4-radius rain field; nine 255-damage/263-healing pulses over 1.35s | `RESONANCE` — 14-range, 90° sound-wave cone; six 220-heal or 180-damage ticks over 2.1s |

Health and damage use Brawl Stars' numeric scale, so the damage numbers read familiarly.
Floating combat numbers appear only when the local player deals or receives damage,
provides or receives healing, self-heals, or participates in a shield absorption.
Bot-only damage, healing, regeneration, and shield exchanges stay hidden.

## Architecture

Brawl Arena is split by ownership rather than by file size alone:

```text
src/
├── core/          # limits, IDs, deterministic random
├── content/       # typed catalog, tuning persistence, external maps
├── game/          # deterministic match simulation and events
├── app/           # ownership, captured input, commands, main loop
├── presentation/  # assets, camera, effects, environment, rendering
├── ui/            # menu and HUD
└── devtools/      # command center and widgets
```

Reusable asset libraries live outside `src/`: character models/animations under
`resources/characters/`, interface art under `resources/ui/`, ability art under
`resources/vfx/`, and generated runtime outputs under `build/assets/`.

`App` separately owns match simulation, local controller state, presentation state,
screen flow, effective tuning, typed content, and configuration provenance. Match reset
clears only the match/controller/presentation regions.

Game systems receive only a `GameContext` containing session, tuning, and content. They
cannot see the camera or UI, do not read devices, and emit `GameEvent` records instead of
spawning visual effects. Input is captured into `PlayerInput`. `make
check-architecture` enforces these boundaries, while deterministic replay tests verify
them at runtime.

Maps are versioned packages listed in `data/maps/manifest.cfg`, with separate terrain,
gameplay-marker, visual-hint, and prop files. Every map is validated for dimensions,
symbols, sealed borders, spawns, one vent, and spawn-to-objective reachability before
startup continues.

See:

- [Architecture](docs/ARCHITECTURE.md)
- [Content and tuning](docs/CONTENT_AND_TUNING.md)
- [Map packages](docs/MAPS.md)
- [Development guide](docs/DEVELOPMENT.md)
- [Ability VFX pipeline](docs/VFX_PIPELINE.md)
- [Visual design field guide](docs/visual-design/index.html) — implemented Helios
  Broadcast styling and screen references, plus the historical pre-implementation audit
- [Helios Broadcast implementation plan](docs/visual-design/IMPLEMENTATION_PLAN.md) —
  implementation record, architecture, milestones, and acceptance gates

## Gem Grab

The match layer, and the default mode. Two teams of three - you plus two allied bots
against three enemies - race to hold ten gems.

A vent at the contested centre of the map coughs up a gem every few seconds. You collect
by walking over them, and **everything you are carrying scatters when you go down**, which
is what makes a big carrier worth hunting.

Holding the target count starts a fifteen-second countdown. Drop below it at any point and
the clock is wiped, not paused - you have to hold the lead for the whole countdown, which
is what makes the last gem tense.

When the match is decided everything **stops**: no AI, no movement, no shooting, and any
round still in flight is cleared so nothing hangs frozen in mid-air. Only effects and the
camera keep running, so the deciding blow finishes playing out. The result is held for a
few seconds - with the count shown - and then you are returned to the menu, or you can
activate the explicit **CONTINUE** action. A finished match that carries on playing
itself reads as a bug, not as a result.

The reactor ring leaves the vent approachable from the centre or either wide flank. Four
small L-shaped pylon groups interrupt cross-map fire without subdividing the atrium, while
breakable cargo and computer banks offer temporary safer rotations. Each docking bay has
three spawn points; the first `P` in the map is deliberately centre-back, so that remains
the human's starting slot.

Allied bots came almost free. The AI only ever asks whether another brawler is on the
opposing team, so putting bots on your side made them fight alongside you without a line
of new targeting code. They also break off to collect loose gems when they are out of
weapon range anyway, rather than walking past a gem to start a fight they cannot yet win.

Everything is tunable live on the **MATCH** tab: team size, gems to win, countdown length,
and how often the vent produces. Turning Gem Grab off returns the build to the free-form
sandbox it started as, with the static training bots.

## Next steps

1. **More kits** — a wall-piercing thrower would further round out the archetypes
2. **Gadgets and star powers** — the per-match consumable and passive layer
3. **Showdown** — free-for-all with a closing poison cloud and power-up cubes
4. **Animation blending** — smooth the visible pop when a rigged model changes clips

## Known gaps

- No sound. Raylib's audio module is available but nothing is wired up.
- No shadow mapping: shadows are blob decals, not cast from the key light.
- Locomotion clips switch without crossfading; explicit action overlays blend in and out.
- Mortar still uses the primitive fallback character.
- Headless tests cover version-3 configuration plus v1/v2 migration, Guardian behavior,
  Tank sustain/mobility/Charge, Scrapper returning discs/Shell lifecycle/AI/cover/ownership,
  Longshot twin-shot damage/spacing/trajectory/charge and Grapple
  timing/cover/cancellation, Mortar Mine
  arming/team/LOS/replacement/cleanup,
  out-of-combat regeneration timing and interruption, all-kit camera isolation, external
  maps, deterministic replay, events, presentation isolation, and deterministic
  character retargeting/asset contracts and stationary-fire animation isolation, plus
  deterministic VFX atlases, all-kit VFX mappings, recipe coverage, animation timing,
  and pool pressure. Graphical and interaction paths still require runtime checks.
