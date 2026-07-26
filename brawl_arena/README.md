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
| `R` | Reset the match (keeps your tuning) |
| `ESC` | Quit |

## What's implemented

**Aim-and-release shooting.** Holding the fire button draws the actual shot on the
ground — the spread cone for direct-fire weapons, a landing circle and flight arc for
lobbed ones, a lunge line for the dash. The preview is clipped by walls, so you can see
exactly where a shot dies before you commit to it. A quick tap instead fires an
auto-aimed shot with target leading, standing in for mobile's tap-to-shoot.

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

### Everything you change is saved

Edits are written to `tuning.cfg` beside the binary about half a second after you stop
dragging, and reloaded on the next launch, so a tuning session survives a restart. The
file is plain `key value` text and is safe to hand-edit or check by eye; unknown and
missing keys are ignored, and values are range-clamped on load so a bad file cannot put
the game in an unplayable state.

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

**Projectiles** are a bright core plus additive glow billboards with a fading trail, and
particles ride the same additive pass so sparks actually glow. Smoke is alpha-blended
separately. Shadows are soft radial ground decals rather than hard cylinders.

**Post-processing** adds thresholded bloom, a vignette and a gentle contrast S-curve.
Toggle it and dial bloom strength on the WORLD tab.

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

## Next steps

The combat layer is in place; match rules are what's missing. The natural order:

1. **Gem Grab** — gem spawner in the middle, carry counter, the 15-second countdown at 10
2. **Teammates** — the bot AI already works as ally logic, it just needs team goals
3. **More kits** — a healer and a wall-piercing thrower would round out the archetypes
4. **Gadgets and star powers** — the per-match consumable and passive layer
5. **Showdown** — free-for-all with a closing poison cloud and power-up cubes

## Known gaps

- No sound. Raylib's audio module is available but nothing is wired up.
- No shadow mapping: shadows are blob decals, not cast from the key light.
- No win condition — bots and player respawn forever. `R` resets the match.
- With bots on `FIGHT`, three against one is deliberately unfair; it's a sparring range,
  not a balanced match. Spawns are placed so nobody starts inside weapon range.
- Characters are drawn from primitives rather than models, matching the approach used in
  `squad_runner/`.
