# Brawl Arena

A top-down 3D arena brawler in the Brawl Stars mould, built with raylib in C.

This build is the **combat-feel slice**: one arena, four kits, training-dummy bots, and
a live tuning panel. The goal was to get the part that's hardest to get right — how it
feels to move, aim and shoot — working before layering objectives on top.

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
| Tap `LMB` or `SPACE` | Quick shot, auto-aimed at the nearest visible enemy |
| Hold `RMB` | Aim your super (only when charged) |
| Release `RMB` | Fire the super |
| `1` – `4` | Swap kit on the spot |
| `TAB` | Open / close the command center |
| `ESC` | Back a screen — select to menu, match to menu, menu quits |
| `R` | Reset the match (keeps your tuning) |
| `ESC` | Quit |

## What's implemented

**Aim-and-release shooting.** Holding the fire button draws the actual shot on the ground
as a solid shape, matched to how the weapon behaves:

- **A filled cone** for spread weapons, opening to the real spread angle.
- **A thick beam** for single-shot weapons like the sniper, and for the dash charge.
- **A filled disc** for lobbed shots, one per shell, sitting where the splash will land —
  plus a dotted arc showing the flight path over any wall in between.

Every rib of the cone and both edges of the beam are raycast separately, so the shape is
clipped flat against whatever wall it runs into rather than passing through it: you can
read exactly where a shot dies before committing. Supers preview in gold, ordinary shots
in blue. A quick tap instead fires an auto-aimed shot with target leading, standing in
for mobile's tap-to-shoot.

**Turn-and-fire.** An auto-aimed shot snaps the brawler around to face its target,
holds that facing for a beat, then eases back to whichever way it is running. The shot
itself still leaves instantly along the true aim line, so the turn is presentation only
and never costs you accuracy.

**Ammo economy.** Three pips, each reloading continuously and independently. You cannot
hold the trigger; you manage a small budget of shots and reposition while it refills.

**Supers charged by damage.** Every pellet that lands fills the meter, and a takedown
tops it up. No timers — aggression is what earns your super.

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

Everything gameplay reads at runtime lives in `World.tune` or the `WEAPONS[]` table, so
every control takes effect on the very next frame — no restart, no rebuild. Clicks over
the panel never reach the game, so tuning and playing don't fight each other.

| Tab | What's in it |
|-----|--------------|
| **BOTS** | Behaviour mode, bot count (0–7), mixed or fixed kits, respawn delay, and respawn / kill / heal buttons |
| **PLAYER** | Active kit, god mode, infinite ammo, move speed, acceleration, dash speed, respawn delay, plus charge-super and heal buttons |
| **KIT** | Live edit of the kit you're holding: health, damage, pellet count, spread, projectile speed, range, shot size, cooldown, reload, super charge rate, and the super's own damage / pellets / spread / range |
| **WORLD** | Time scale (slow-mo for reading projectiles), super gain multiplier, bush reveal range, reveal-on-fire duration, bloom and vignette, a debug overlay, and arena / score resets |

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

### Everything you change is saved

Edits are written to `tuning.cfg` beside the binary about half a second after you stop
dragging, and reloaded on the next launch. Because the write happens on change rather
than on exit, the settings survive the process being killed outright - verified by
tweaking five values, `kill -9`, and restarting to find all five loaded back. Changing
screen also flushes any pending write immediately, so a tweak made a moment before
quitting from the menu still lands.

The file is plain `key value` text and is safe to hand-edit or check by eye; unknown and
missing keys are ignored, and values are range-clamped on load so a bad file cannot put
the game in an unplayable state. Persisted state covers every command-center parameter,
all four kits' full stats, the selected brawler, and your win/loss/KO record.

To go back to the built-in defaults, either hit `Reset ALL tuning` on the WORLD tab
(`Reset this kit` on KIT does one kit) or just delete `tuning.cfg`.

## Look

There are no asset files. Every texture is synthesised at startup from value noise, and
both shaders are embedded as strings, so the binary is fully self-contained.

**Lit geometry.** The arena and brawlers are drawn as real meshes through a custom GLSL
330 shader: a half-Lambert key light so shadowed sides stay readable, a Blinn specular
highlight, a rim term that lifts silhouettes off the background, and distance fog for
depth. Immediate-mode `DrawSphere`/`DrawCapsule` calls were dropped because they do not
emit reliable normals — meshes from `GenMesh*` do.

**Dynamic lights.** Muzzle flashes, explosions, crate breaks, in-flight projectiles and
charged supers all emit real point lights that pool on the floor. Every candidate is
scored each frame and the strongest eight go to the GPU.

**Procedural materials.** Floor tiles with grout lines that produce the arena grid
without extra geometry, speckled concrete walls, planked wood crates with metal banding,
and leafy bush noise — all generated with a small value-noise/fbm function, mipmapped
and trilinear filtered.

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

**Post-processing** adds thresholded bloom, a vignette and a gentle contrast S-curve.
Toggle it and dial bloom strength on the WORLD tab.

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
still works. Band count and ink strength are sliders, everything is live, and toggling
it off returns the original smooth-lit look.

## Imported characters

The SCRAPPER is played as a rigged, animated character model - on the menu podium, in
character select, and in the arena. The other kits keep their primitive brawlers in
their accent colours. In a match the model picks its clip from movement (idle,
walking, running or dashing), flashes on hit, ghosts in bushes like everything else,
and carries a red cast on enemies and a blue one on allied bots so a grey model still
reads friend-or-foe at a glance. The WORLD-tab toggle turns it off, and the game falls
back to primitives automatically if the file is missing.

In a match the clip is picked from the movement direction relative to facing -
forward, backward and four diagonals - so backpedaling and circle-strafing animate
correctly instead of moonwalking, with playback rate following actual speed. Going
down plays a death clip that holds until just before the respawn, and a brawler that
just fired holds a combat stance. The model is an AI-generated Meshy export at
`resources/sentinel.glb` (12 clips, repacked from 24 MB to 1.7 MB). raylib has no
animation crossfade, so clip changes restart the cycle - a known small pop.

Raw Meshy/Tripo exports do NOT load correctly in raylib - they pass every load-time
check and then render as a collapsed spike-ball. The full story of why (raylib's
glTF loader implements a much narrower contract than the spec), the converter that
fixes it, the checklist for importing the next character, and the debugging traps to
avoid are in **[docs/CHARACTER_PIPELINE.md](docs/CHARACTER_PIPELINE.md)**. Short
version:

    python3 tools/fix_meshy_glb.py <meshy-export-dir> resources/<name>.glb

## The four kits

| Kit | HP | Attack | Super |
|-----|----|--------|-------|
| **SCRAPPER** | 3800 | 5-pellet spread, short range | `BUCKSHOT` — 9 pellets, breaks walls |
| **LONGSHOT** | 2800 | Single shot, damage scales up to 2× with travel distance | `RAILGUN` — piercing, hits everyone in a line |
| **MORTAR** | 3200 | Arcing lob that clears walls, splash on landing | `BARRAGE` — three shells in a fan |
| **TANK** | 5600 | 4-pellet burst, very short range | `CHARGE` — dash that damages, knocks back and smashes crates |

Health and damage use Brawl Stars' numeric scale, so the damage numbers read familiarly.

## Architecture

Every system takes a `World*`. There is no hidden global state, which keeps modules
independently testable and makes it obvious what each one touches.

```
src/
├── types.h      # all shared structs + tuning constants
├── arena.c/.h   # tile map, collision, line of sight, destructibles
├── brawler.c/.h # entities, health, ammo, super, dash, respawn, visibility
├── weapons.c/.h # the kit roster, projectile spawning, flight and impact
├── ai.c/.h      # bot state machine, steering and behaviour modes
├── command_center.c/.h  # live tuning panel + its immediate-mode widgets
├── assets.c/.h  # procedural textures, unit meshes, embedded shaders
├── config.c/.h  # tuning.cfg save / load / autosave
├── player.c/.h  # input to intent, aiming, tap vs hold
├── render.c/.h  # camera, world drawing, trajectory previews
├── effects.c/.h # particles, floating text, screen shake
├── hud.c/.h     # health bars and screen-space UI
└── main.c       # init and the game loop
```

Starting values live in `types.h` (as `#define`s) and `WEAPON_DEFAULTS[]` at the top of
`weapons.c`. At runtime the game reads `World.tune` and the mutable `WEAPONS[]` table,
which the command center edits. To change a default permanently, edit
`TuningSetDefaults()` or `WEAPON_DEFAULTS[]`; to explore, use the panel.

The map is a plain character grid at the top of `arena.c` (`#` wall, `c` crate,
`b` bush, `P` player spawn, `E` enemy spawn). Rows are padded and the border is forced
to wall, so you can edit the layout without counting characters.

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

The arena's middle was opened into a corridor specifically for this: the vent has to sit
somewhere worth fighting over. Each side has three spawn points, and the first one in the
map is the human's, so you start centre-back.

Allied bots came almost free. The AI only ever asks whether another brawler is on the
opposing team, so putting bots on your side made them fight alongside you without a line
of new targeting code. They also break off to collect loose gems when they are out of
weapon range anyway, rather than walking past a gem to start a fight they cannot yet win.

Everything is tunable live on the **MATCH** tab: team size, gems to win, countdown length,
and how often the vent produces. Turning Gem Grab off returns the build to the free-form
sandbox it started as, with the static training bots.

## Next steps

1. **More kits** — a healer and a wall-piercing thrower would round out the archetypes
2. **Gadgets and star powers** — the per-match consumable and passive layer
3. **Showdown** — free-for-all with a closing poison cloud and power-up cubes
4. **Animation blending** — needed before rigged character models are worth loading

## Known gaps

- No sound. Raylib's audio module is available but nothing is wired up.
- No shadow mapping: shadows are blob decals, not cast from the key light.
- No win condition — bots and player respawn forever. `R` resets the match.
- With bots on `FIGHT`, three against one is deliberately unfair; it's a sparring range,
  not a balanced match. Spawns are placed so nobody starts inside weapon range.
- Characters are drawn from primitives rather than models, matching the approach used in
  `squad_runner/`.
