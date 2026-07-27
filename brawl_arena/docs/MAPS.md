# Brawl Arena map packages

Last code-verified: 2026-07-26

Maps are tracked, versioned content under `data/maps/`. Collision/game rules are separate
from visual decoration so artists can change station dressing without changing
walkability, and designers can change cover without editing renderer code.

## Catalog

`data/maps/manifest.cfg` is loaded at startup:

```ini
version=1
default=helios_9
map=helios_9
map=training_court
```

Directory names and `map=` values are stable map IDs. The catalog permits up to
`MAX_MAPS` (currently eight), rejects duplicate IDs, and requires `default` to identify a
listed map.

The current packages are:

- `helios_9`: the primary 33×23 arena with cover, concealment, and station props.
- `training_court`: a smaller fixture used to prove dimensions and content are not
  hard-coded to Helios-9.

The World tab's Next Map action advances the selected catalog index and requests a match
rebuild.

## Package layout

```text
data/maps/<map-id>/
├── map.cfg
├── terrain.layer
├── gameplay.layer
├── visual.layer
└── props.cfg
```

`map.cfg` uses `key=value`:

```ini
version=1
id=helios_9
name=Helios-9
width=33
height=23
tile_size=2.0
terrain=terrain.layer
gameplay=gameplay.layer
visual=visual.layer
props=props.cfg
```

IDs are stable programmatic names; `name` is display text. Dimensions must be positive
and no larger than 64×64. Every layer must contain exactly `height` rows, each exactly
`width` characters. A line beginning with `;` is ignored by the layer reader.

## Terrain layer

Terrain owns collision and concealment:

| Symbol | Runtime tile | Behavior |
|---|---|---|
| `.` | floor | walkable |
| `#` | wall | permanent solid cover |
| `c` | crate | solid, destructible cover |
| `b` | bush | walkable concealment/grass |

Every outer-border cell must be `#`. Gameplay markers cannot overlap `#` or `c`.
Crate health comes from effective project tuning when `ArenaLoad()` constructs runtime
tiles.

## Gameplay layer

Gameplay owns match landmarks:

| Symbol | Meaning |
|---|---|
| `.` | no marker |
| `P` | player-team spawn |
| `E` | enemy-team spawn |
| `V` | Gem Grab vent/objective |

A map needs at least one spawn for each side and exactly one vent. Every spawn and vent
must be on walkable terrain. The loader performs a four-direction flood fill and rejects
any spawn that cannot reach the vent.

Marker order is stable within row scan order. The first player spawn is human slot zero;
later spawns serve allied bots. When a requested slot exceeds a map's explicit spawn
count, arena spawn selection wraps through available entries.

## Visual layer

Visual cells select palette and presentation hints without changing collision:

| Symbol | Palette | Hint |
|---|---|---|
| `.` | orange | default |
| `p` | purple | default |
| `d` / `D` | orange / purple | door |
| `s` / `S` | orange / purple | display |
| `a` / `A` | orange / purple | floor accent |
| `c` / `C` | orange / purple | cargo |
| `t` / `T` | orange / purple | tech |

Lowercase selects the orange station palette; uppercase selects purple where a paired
symbol exists.

## Props

`props.cfg` contains whitespace-separated records:

```text
# asset x y z yaw_degrees scale_x scale_y scale_z palette emissive
floor_panel 0.0 -0.820 0.0 45 3.2 2.8 3.2 purple 0.08
```

Rules:

- Asset ID is at most 47 characters and must resolve through the environment's supported
  station-model table.
- Position and yaw are world-space values.
- Every scale component must be greater than zero.
- Palette is `orange` or `purple`.
- Emissive strength is from 0 to 1.
- Blank lines and lines beginning with `#` are ignored.
- A map may contain up to `MAX_MAP_PROPS` (currently 64) props.

Unknown/unavailable station assets fall back according to the environment renderer's
asset policy; invalid record syntax rejects the map.

## Adding a map

1. Copy `training_court` as a small starting package.
2. Give the directory and `map.cfg id` the same new stable ID.
3. Set dimensions and make all three layers exact.
4. Seal the terrain border with walls.
5. Place at least one `P`, one `E`, and one `V` on walkable cells.
6. Verify every spawn can reach the vent.
7. Add visual hints and optional props.
8. Add `map=<id>` to `manifest.cfg`.
9. Extend `tests/test_arena.c` with expectations when the map is production content
   rather than a temporary fixture.
10. Run `make test`, then enter the map through the command center for a visual/collision
    alignment check.

The test validates runtime dimensions, IDs, spawn counts, crate health, sealed borders,
cover/concealment presence where expected, and spawn-to-objective reachability.
