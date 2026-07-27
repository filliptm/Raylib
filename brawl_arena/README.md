# Brawl Arena

A top-down 3D arena brawler in the Brawl Stars mould, built with raylib in C.

This build is a playable **Gem Grab combat slice**: one arena, five kits, 3v3 team play,
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

The game opens on a **main menu** with the selected brawler on a lit podium and everything
else arranged around the edges: profile and stats top-left, practice and quit top-right,
brawler select and controls down the left, the kit's name badge above the model, its stats
to the right, and the mode card and PLAY button along the bottom.

Clicking **BRAWLERS** opens character select: the brawler stands on the left, and the
roster scrolls on the right. Each entry carries the kit's name, health, damage, range,
reload, cooldown and ammo, plus a line describing its attack and its super. Those
descriptions are derived from the weapon data rather than written out, so they stay true
after the numbers are tuned.

The list wraps rather than stopping, so a short roster never hits a dead end at either
edge. Scroll with the wheel or drag it; a press only counts as a pick if the pointer
barely moved, so dragging never selects by accident.

Nothing in the menu is a placeholder. Every card does something real - selecting a kit,
toggling the rules, opening the tuning panel, starting a match, quitting - because a menu
full of dead buttons teaches you to stop clicking things.

`ESC` steps back a screen: select to menu, match to menu, menu to quit. raylib closes the
window on `ESC` by default, so that is explicitly disabled and handled here instead.

## Controls

| Input | Action |
|-------|--------|
| `WASD` / arrows | Move |
| Hold `LMB` | Aim — shows a live trajectory preview |
| Release `LMB` | Fire along the preview |
| Tap `LMB` or `SPACE` | Quick shot; Guardian prioritizes a badly hurt ally, otherwise targets the nearest visible enemy |
| Hold `RMB` | Aim your super (only when charged) |
| Release `RMB` | Fire the super |
| `1` – `5` | Swap kit on the spot |
| `TAB` | Open / close the command center |
| `ESC` | Back a screen — select to menu, match to menu, menu quits |
| `R` | Reset the match (keeps your tuning) |

## What's implemented

**Aim-and-release shooting.** Holding the fire button draws the actual shot on the ground
as a solid shape, matched to how the weapon behaves:

- **A filled cone** for spread weapons, opening to the real spread angle.
- **A thick beam** for single-shot weapons like the sniper, and for the dash charge.
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
  nearest bush below 30% health, and fire supers only when the shot will connect.

## The command center

Press `TAB`. It opens on launch, because this build is a sandbox.

Effective tuning lives in `World.tune` or the `WEAPONS[]` table and is sourced from the
tracked `config/gameplay.cfg`. Sliders apply immediately and autosave an ignored local
draft; roster and rule changes that alter match construction request a rebuild. Clicks
over the panel never reach the game, and each tab scrolls independently when its controls
are taller than the window.

| Tab | What's in it |
|-----|--------------|
| **MATCH** | Gem Grab toggle, team size, target count, countdown/vent timing, match state, and objective actions |
| **BOTS** | Behaviour mode, bot count (0–7), mixed or fixed kits, respawn delay, AI health thresholds/probe distance, and respawn / kill / heal buttons |
| **PLAYER** | Active kit, god mode, infinite ammo, move speed, acceleration, dash speed, respawn delay, plus charge-super and heal buttons |
| **KIT** | Live edit of the active kit, including ammo capacity. Guardian exposes rain duration/pulse/growth and Resonance duration/tick/wave controls, plus reset/save-as-project actions |
| **STYLE** | Post-processing master switch, toon controls, painterly/pixel/print effects, color grade, bloom, vignette, grain, and chromatic fringe |
| **WORLD** | Time scale, super gain, crate/result timing, stealth, grass, rendering, debug, draft discard, and `SAVE ALL AS PROJECT DEFAULTS` |

Editing a kit's max health updates living brawlers of that class immediately, keeping
their health ratio, so you can feel a change without respawning.

The debug overlay draws each brawler's weapon range and body radius, plus a sight line
to the player — green when that bot can actually see you, red when a wall or bush is
blocking it. It's the fastest way to check the stealth rules are doing what you expect.

### The sandbox

**PRACTICE** from the menu drops you into a firing range: three static targets fanned out
ahead of you at stepped distances, no objective, no return fire, and the command center
already open. `TAB` hides the panel if you just want the range. Reading weapon range and damage falloff off the screen
is the whole point, so the targets spawn in front of you rather than at the far end of
the map, and the spawner nudges them clear of walls and bushes so nothing hides.

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
are always local. `profile.cfg` separately owns the selected kit and win/loss/KO record.

The command center shows `PROJECT DEFAULTS` or `PROJECT + LOCAL DRAFT (N)`:

- `SAVE KIT AS PROJECT DEFAULT` promotes only the active kit.
- `SAVE ALL AS PROJECT DEFAULTS` promotes all live project-scoped values.
- Reset/discard actions restore the tracked project values and clear matching draft keys.

Promotion validates and atomically writes the tracked file. It creates an ordinary Git
working-tree modification but does not run `git commit`.

If the new local file is absent, an existing version-1/version-2 `tuning.cfg` is imported
once into the split draft/profile files without modifying the original. Version-1
Guardian bolt/Sanctuary numbers are discarded because they do not describe the new
rain/Resonance behavior. See [`config/README.md`](config/README.md) for the schema and
authoring workflow.

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

**Post-processing** adds thresholded bloom, a vignette, a gentle contrast S-curve, and
FXAA-style edge smoothing. Toggle the pass and dial bloom strength on the WORLD tab.
The depth-based effects use the same 0.5-to-120 clip range as the 3D cameras rather than
raylib's much wider default range, concentrating depth precision around the arena.

**The STYLE tab** in the command center is a full mix-and-match viewport rig: toon
banding and ink outlines, painterly (Kuwahara), pixelate, halftone comic dots,
posterize, saturation, brightness, bloom, vignette, film grain and chromatic fringe -
every effect an independent live slider, all persisted, all composable in one post
pass. HUD and menus draw after the pass, so the interface stays crisp whatever the
world looks like.

**The toon look** (STYLE tab, on by default) turns the scene illustrative: lighting is
quantised into hard cel bands with specular and rim killed, ambient lifted and colours
saturated so shadow bands stay vivid, and a depth-based ink outline is drawn around
every silhouette in the post pass. The outline needs a sampleable depth texture, so the
scene renders into a hand-built framebuffer (raylib's stock render texture keeps depth
in a renderbuffer); if that setup fails, outlines quietly turn off and everything else
still works. The outline transition is softened in screen space so it does not introduce
a second source of hard edge shimmer. Band count and ink strength are sliders, everything
is live, and toggling it off returns the original smooth-lit look.

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
- Destructible tiles render as cargo containers on one side and computer/power banks on
  the other. Damage tint and removal still follow the tile's health.
- Bushes are ruptured hydroponic beds. Their low station panels sit beneath the existing
  animated concealment grass.
- Spawn pads, reactor floor details, pipes, rails, support frames, and exterior work
  platforms are non-gameplay dressing.
- Floor rendering uses explicit deck (-0.08), inset-top (0.02), passive-decal (0.065),
  and targeting/effect (0.12) heights. The separation keeps imported panels, shadows,
  glows, and aim previews from competing for the same depth-buffer values.
- Both station texture atlases use generated mipmaps with trilinear filtering, reducing
  texture shimmer on panels viewed at shallow angles or from the match camera.

The window requests four-sample MSAA and vertical synchronization. Its frame ceiling
matches the active monitor refresh rate, falling back to 60 Hz when that rate is
unavailable, which avoids uneven 60 Hz pacing on 100/120 Hz displays.

The source pack is CC0. `LICENSE.txt` and `SOURCE.md` beside the assets preserve its
license, origin, included formats, and texture-layout requirements. Missing station
models fall back independently to the generated cover, so collision never becomes
invisible.

## Imported characters

SCRAPPER, TANK, and GUARDIAN are played as rigged, animated character models - on the
menu podium, in character select, and in the arena. Longshot and Mortar keep their
primitive brawlers in their accent colours. In a match each model picks its clip from
movement (idle, walking, running or dashing), flashes on hit, ghosts in bushes like
everything else, and carries a red cast on enemies and a blue one on allied bots so a
grey model still reads friend-or-foe at a glance. The WORLD-tab toggle turns rigged
models off, and each kit falls back to primitives automatically if its file is missing.

In a match the clip is picked from the movement direction relative to facing -
forward, backward and four diagonals - so backpedaling and circle-strafing animate
correctly instead of moonwalking, with playback rate following actual speed. Going
down plays a death clip that holds until just before the respawn, and a brawler that
just fired holds a combat stance. The AI-generated Meshy runtime assets are
`resources/sentinel.glb` for Scrapper (13 clips, about 4.3 MiB),
`resources/ironclad_guardian.glb` for Tank (13 clips, about 1.6 MiB), and
`resources/gaia_guardian.glb` for Guardian (13 clips, about 1.7 MiB).
raylib has no animation crossfade, so clip changes restart the cycle - a known small pop.

Raw Meshy/Tripo exports do NOT load correctly in raylib - they pass every load-time
check and then render as a collapsed spike-ball. The full story of why (raylib's
glTF loader implements a much narrower contract than the spec), the converter that
fixes it, the checklist for importing the next character, and the debugging traps to
avoid are in **[docs/CHARACTER_PIPELINE.md](docs/CHARACTER_PIPELINE.md)**. Short
version:

    python3 tools/fix_meshy_glb.py <meshy-export-dir> resources/<name>.glb

## The five kits

| Kit | HP | Attack | Super |
|-----|----|--------|-------|
| **SCRAPPER** | 3800 | 5-pellet spread, short range | `BUCKSHOT` — 9 pellets, breaks walls |
| **LONGSHOT** | 2800 | Single shot, scales from 50% to 100% damage with travel distance | `RAILGUN` — piercing, hits everyone in a line |
| **MORTAR** | 3200 | Arcing lob that clears walls, splash on landing | `BARRAGE` — three shells in a fan |
| **TANK** | 5600 | 4-pellet burst, very short range | `CHARGE` — dash that damages, knocks back and smashes crates |
| **GUARDIAN** | 3400 | Growing 3.4-radius rain field; nine 100-point damage/healing pulses over 1.35s | `RESONANCE` — 14-range, 90° sound-wave cone; six 220-heal or 180-damage ticks over 2.1s |

Health and damage use Brawl Stars' numeric scale, so the damage numbers read familiarly.

## Architecture

Gameplay systems consistently take a `World*`, which makes their shared state explicit.
The application still has a few deliberate globals: the mutable `WEAPONS[]` table, root
`World`/`Assets` ownership, and static UI/render caches.

```
src/
├── types.h      # all shared structs + tuning constants
├── arena.c/.h   # tile map, collision, line of sight, destructibles
├── brawler.c/.h # entities, health, ammo, timed marks, dash, respawn, visibility
├── weapons.c/.h # kit roster, projectiles, rain/sound fields, flight and impact
├── ai.c/.h      # bot state machine, steering and behaviour modes
├── command_center.c/.h  # live tuning panel + its immediate-mode widgets
├── assets.c/.h  # procedural assets, shaders, rigged characters, station GLBs
├── config.c/.h  # canonical config, local draft/profile, validation and promotion
├── environment.c/.h # collision-aligned Helios-9 presentation + set dressing
├── player.c/.h  # input to intent, aiming, tap vs hold
├── render.c/.h  # camera, world drawing, trajectory previews
├── effects.c/.h # particles, floating text, match-state camera shake
├── hud.c/.h     # health bars and screen-space UI
└── main.c       # init and the game loop
```

`config/gameplay.cfg` is the normal source of starting values. `TuningSetDefaults()` and
`WEAPON_DEFAULTS[]` are recovery values used to report a broken/missing installation,
validate isolated fixtures, and import older saves. Runtime reads `World.tune` and the
mutable `WEAPONS[]` table; the command center edits them and can explicitly promote the
effective result back into the tracked canonical file.

The map is a plain 33×23 character grid at the top of `arena.c` (`#` wall, `c` crate,
`b` bush, `P` player spawn, `E` enemy spawn). Rows are padded and the border is forced
to wall. Helios-9 uses a mirrored open-atrium layout: short docking-bay shields,
destructible flank stations, four central pylon groups, and hydroponic pockets separated
by broad movement bands. Cover is concentrated into recognizable strategic points rather
than repeated across each row.

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
click straight through. A finished match that carries on playing itself reads as a bug,
not as a result.

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
- Character animation clips switch without crossfading.
- Longshot and Mortar still use the primitive fallback characters.
- Headless tests cover configuration, Guardian timing, and combat camera behavior;
  graphical and interaction paths still require runtime checks.
