# Brawl Arena on iPhone

Brawl Arena ships a generated Xcode project for a landscape iPhone build. The iOS target
uses the same C application, deterministic game simulation, content, maps, and generated
assets as the desktop target.

## Requirements

- Xcode 26 or newer with an iOS platform installed.
- CMake 3.25 or newer.
- Git and Python 3 for dependency and asset generation.
- An Apple Development team for a signed device build.

The bootstrap script fetches `ghera/raylib-iOS` at the pinned commit
`0fa3228e90c7f2717b6ad7858b3438326d957059`. It applies the tracked
`patches/raylib-ios-es3.patch` for Xcode/Apple type compatibility and app-background
flush behavior. `apple/ThirdParty`, `apple/build`, and `apple/DerivedData` are generated
or fetched local state and are ignored by Git.

## Generate and build

From `brawl_arena`:

```bash
make ios-bootstrap
make ios-project
make ios-simulator
```

`ios-simulator` targets an `iPhone 17 Pro` by default. Override it with:

```bash
make ios-simulator IOS_SIMULATOR="iPhone 17 Pro Max"
```

For a signed device build:

```bash
make ios-device BRAWL_DEVELOPMENT_TEAM=YOUR_TEAM_ID
```

The generated project is `apple/build/BrawlArenaIOS.xcodeproj`. It uses automatic
signing and the bundle identifier `com.filliptm.brawlarena`. To install an already
signed build from the command line:

```bash
xcrun devicectl device install app \
  --device "Your iPhone" \
  apple/build/Debug-iphoneos/BrawlArena.app

xcrun devicectl device process launch \
  --device "Your iPhone" \
  --terminate-existing \
  com.filliptm.brawlarena
```

The minimum deployment target is iOS 15.6. The app is full-screen and supports landscape
left/right orientations.

## Touch controls

- Left floating stick: camera-relative direction; movement is stopped inside the dead
  zone and full speed outside it.
- Right floating stick: hold and drag to aim the main attack; release to fire. A quick
  tap keeps the existing nearest-target auto-aim behavior.
- `SUPER`: hold and drag to aim; release to activate when charged.
- `SKILL`: press for instant secondaries, hold for shields, or drag/release Longshot's
  grapple.
- Pause button: return to the launch deck.

Every touch is tracked by its stable platform ID, so movement, attack aim, and an ability
can be held independently. The control HUD uses iPhone safe-area insets, 44-point-or-
larger targets, cooldown/readiness rings, and a dim idle state. Move and Attack artwork
is additionally translucent over the arena; Super and Skill retain their established
visual treatment.

## Runtime policy

The bundle places project content under `BrawlAssets`. At startup the app changes to that
read-only resource root. Personal state is written under the app's Application Support
directory as `tuning.local.cfg`, `profile.cfg`, and the legacy-import path `tuning.cfg`.
Backgrounding resets captured touches and flushes dirty configuration.

iOS uses OpenGL ES 3 shader sources through ANGLE/Metal, caps world rendering at native
logical scale, disables the desktop post-processing pass, and retains 4× MSAA. Imported
rigged GLBs are animated with raylib CPU skinning on iOS and drawn through the ordinary
OpenGL ES 3 lighting shader; this avoids invalid vertex positions from the pinned ANGLE
GPU-bone path while preserving the packaged models and animations. Desktop retains GPU
skinning. The command center and VFX Studio are desktop authoring tools and are not
exposed in the mobile shell. Home, roster, Controls, Settings, downed, and result screens
use a dedicated safe-width landscape composition. The match camera preserves the
project-authored distance but renders at 80% of it, with a 20-unit floor, for closer
phone framing.

For automated graphical smoke checks, launching with
`BRAWL_IOS_SMOKE_MATCH=1` opens directly into a match without changing persisted
configuration. This environment variable is absent during normal launches.

## Verification

Compilation does not validate touch feel or GPU output. Before distribution:

1. Build and launch the menu in an iPhone simulator.
2. Launch a match and inspect the arena and all touch controls in both landscape
   orientations.
3. On a physical iPhone, verify simultaneous movement/aim, attack release, Super,
   every secondary behavior, pause/background/resume, and profile persistence.
4. Run the desktop architecture, UI, config, test, and sanitizer targets.

The current raylib iOS dependency is a pinned third-party fork rather than an official
raylib 5.5 mobile release. Revalidate the compatibility patch and both simulator/device
builds before changing that revision.
