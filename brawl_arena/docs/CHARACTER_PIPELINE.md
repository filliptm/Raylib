# Importing rigged characters (and why AI exports collapse in raylib)

This documents a full day of debugging so nobody repeats it. If you only remember one
thing: **never load a raw Meshy/Tripo export directly — run it through
`tools/fix_meshy_glb.py` first.**

```
python3 tools/fix_meshy_glb.py <meshy-export-dir> resources/sentinel.glb
```

The symptom of skipping this step is not an error. The model loads "successfully"
(`IsModelValid` true, clips report `matchesSkeleton=1`) and renders as a collapsed
spiky star, or as nothing at all. Every load-time check passes; only the picture is
wrong.

---

## raylib 5.5's actual skinning contract

Verified against `rmodels.c` from the 5.5 tag — not the glTF spec, and not guesswork.
raylib is far stricter than the spec:

1. **The skin's `inverseBindMatrices` are never read.** The bind pose is rebuilt from
   each joint's *node world transform* (`cgltf_node_transform_world` +
   `MatrixDecompose`, rmodels.c ~5736). If the node rest pose is not the bind pose,
   skinning is silently wrong.

2. **Animation frames are composed through bone parents only**
   (`BuildPoseFromParentJoints`, ~4164). Transforms on nodes *above* the skeleton
   root — scene wrappers, "Armature" nodes — are **included in the bind pose but
   excluded from animation**. Any transform on such a node desyncs the two spaces.

3. **The bind pose must have scale exactly 1.** `UpdateModelAnimationBones` (~2282)
   inverts the bind TRS component-wise but *drops the `1/scale` factor on
   translation*, and its final matrix compose multiplies translation by scale. Both
   are only correct when every bind scale is 1. A uniform 0.01 bind scale renders the
   model ~90× too large and off-camera.

4. **Unkeyed animation channels fall back to the node's static TRS** (~6165). If you
   rewrite node statics (as our fixer does), every joint must have full T/R/S keys in
   every clip, or filler channels must be added. Meshy keys all three on every joint,
   and the tool inserts constant channels wherever a future export doesn't.

5. **Mesh vertices are pre-transformed by the mesh node's world matrix** at load
   (~5395), contrary to the spec (which says a skinned mesh ignores its node
   transform). Keep all non-joint nodes at identity.

6. `BuildPoseFromParentJoints` also **does not apply parent scale to child
   translations**, so animated joint local scales must stay ~1.

A file that satisfies all of this looks exactly like a Blender export: node rest pose
== bind pose, unit scales everywhere, no transforms on non-joint nodes, vertices in
skeleton space, full TRS keys. That is why raylib's own `greenman.glb` works and raw
AI exports don't.

## What Meshy actually ships

- Mesh vertices at **1/100 scale** relative to the skeleton.
- The real bind pose hidden in the **inverseBindMatrices, which carry a 100× scale**
  raylib never sees.
- An **`Armature` wrapper node with scale 0.01** above the skeleton root — included
  in raylib's bind, excluded from raylib's animation (contract points 1–3 violated).
- A **4096×4096 texture that is 99.9% of the file** (27.8 MB of a 29 MB GLB) for a
  character ~70 px tall on screen.
- **One GLB per animation**, each carrying a full copy of mesh + skin + texture:
  117 MB for one character with three clips.

None of this is corrupt — it is legal glTF. It is simply outside raylib's contract.

## What the fixer does

`tools/fix_meshy_glb.py` (self-contained, stdlib + optional Pillow):

- Computes the uniform scale `k` hidden in the IBMs (here: exactly 100).
- Rewrites every joint's static TRS so the node hierarchy composes to
  `k · inverse(IBM)` — the bind pose raylib will reconstruct, at **unit scale**, in
  the same space as the animation channels.
- Bakes `k` into the mesh vertex positions so the skin still matches.
- Strips transforms from all non-joint nodes (Armature wrapper, mesh node).
- Adds constant filler channels for any unkeyed joint path, so the static rewrite can
  never leak into animation.
- Merges the per-animation GLBs into one file with named clips
  (`tpose`, `idle`, `running`, `walking`).
- Downscales the texture (512 px is generous at this camera distance).
- Verifies numerically: TRS reconstruction error and world-vs-inverse-IBM error are
  printed and should be ~1e-5 or better.

Result for the Sentinel: **117 MB in four files → 0.88 MB in one**, loading as
4,197 verts / 24 bones / 4 clips, posed height ~170 skeleton units, auto-normalised
to 2.0 world units by `AssetsLoad`.

## Standard animation set

Every character export uses the same clips, chosen from Meshy's library under the same
names, so any new character drops into the game with zero code changes.

Core - required:

| Clip | Used for |
|---|---|
| Idle | standing |
| Run forward | moving toward facing |
| Run backward | backpedaling while aiming |
| Strafe left / Strafe right | circle-strafing (export both; do not rely on mirroring) |
| Shoot | standing fire / recoil |
| Death (knockdown) | played on KO before the respawn |
| Emote / victory | result screen and select podium |

Optional, grab when available: walk forward (else run is play-rate scaled for slow
movement), dash/charge lunge (dash supers), hit flinch.

Deliberately excluded: a generic jump. There is no jump mechanic, and a leap belongs
to whichever kit's super eventually needs it.

Rules that matter more than the list:

- **Every clip must be in-place - no root motion.** The game moves the character; a
  clip that translates its root makes the feet slide.
- **Same library animations, same names, every character.** Clip names come from the
  Meshy filenames (`Animation_<Name>_withSkin`) and the game resolves clips by name.
- Locomotion and idle loop; shoot, death and emote are one-shots.

Engine-side status: implemented. The clip is chosen from the movement direction
relative to facing (forward, backward and four diagonals - a pure strafe picks the
nearest diagonal), playback rate follows actual speed so feet track the ground, death
plays as a one-shot that holds its final pose until just before respawn, and a
recently-fired brawler holds the combat stance instead of relaxing to idle. Clips are
resolved by substring (idle, combat, running, walking, backward, forwardleft/right,
backleft/right, dead) with graceful fallbacks when a set is incomplete. Still open:
shoot and emote one-shots, and crossfade blending.

The tool accepts both Meshy export styles: one GLB per animation (clip named from the
filename) and the newer single merged-animations GLB (each clip keeps its own name,
lowercased). The file carrying the most clips becomes the base.

## Checklist for adding a new character

1. In Meshy, download the **GLB** (not FBX/USDZ — raylib loads neither) with skin,
   plus each animation as its own GLB. Keep polycount modest (~4–8k tris).
2. Drop all the GLBs in one directory. The T-pose file must contain
   `Character_output` in its name (Meshy's default).
3. `python3 tools/fix_meshy_glb.py <dir> resources/<name>.glb`
4. Check the tool's output: reconstruction errors ~1e-5, expected clip list, texture
   line present.
5. Point `CHARACTER_MODEL_PATH` in `src/assets.c` at the file and run. The log line
   to look for: `CHARACTER: <verts> verts, <bones> bones, <clips> clips, posed height
   <H>, scale <s>` — posed height should be a sane skeleton-space number (tens to
   hundreds), not thousands.
6. Look at the menu podium. The model must stand on the disc, facing the camera,
   idle-animating. If it's a spike-ball or invisible, re-read this document.

Hard limits: ≤128 bones (`MAX_BONE_NUM` in the skinned shader), one skin per file,
LINEAR/STEP keyframes, PNG textures.

## The debugging playbook that cracked it

Worth keeping, because every step here either found a bug or disproved a theory:

- **Control test first.** Loading raylib's own Blender-exported `greenman.glb`
  through our exact draw path rendered perfectly on the first try. That one run
  separated "our code is broken" from "the asset is broken" for the entire session.
- **Read the loader's source; don't guess.** Fetching `rmodels.c` for the installed
  version and reading `LoadGLTF` / `UpdateModelAnimationBones` /
  `BuildPoseFromParentJoints` produced the contract above. Every earlier fix attempt
  based on the glTF *spec* failed, because raylib doesn't implement the spec.
- **Reproduce the shader's math CPU-side.** A probe that skins vertices on the CPU
  from the same `boneIds`/`boneWeights`/`boneMatrices` the GPU receives, printing
  bounds per clip and per frame, is what proved the data correct when the screen
  still showed nothing — and earlier, what proved the scale wrong when the screen
  showed a spike-ball.
- **Bisect the render path when data and picture disagree.** Drawing three variants
  in one frame (skinned, unskinned through the scene shader, stock `DrawModel`)
  pinpoints the failing stage immediately.

### Traps we actually fell into (all self-inflicted)

- **Stale screenshot.** A capture probe wrote files under old names; the analysis
  read a leftover image from a previous broken build and declared a working fix
  "still broken". Name verification captures uniquely, and check the file's mtime
  against the run.
- **Wrong material slot.** "Texture is 1×1" was a misread — the mesh used material 1,
  the probe read material 0. Print all slots plus `meshMaterial[]`.
- **Same-second mtime race.** Editing a source file and running `make` within the
  same second can leave `.c` and `.o` with equal timestamps, so make skips the
  rebuild and you test a binary without your change. `touch` the files first, and
  verify probe strings actually exist in the binary with `strings`.
- **Two probes firing on the same frame** produced two identical screenshots and a
  false "animation is frozen". Space captures by a second or more and diff them
  numerically.
- **The Makefile had no header dependencies.** Editing `types.h` (struct layouts
  shared by every module) rebuilt only the touched `.c` files, so stale objects read
  struct fields at old offsets. The symptoms looked impossible - a function verified
  correct set a field its caller then read as zero, and the menu podium vanished -
  because writer and reader disagreed about where fields lived. Every `.o` now depends
  on every header. If behaviour turns impossible right after a header edit, suspect
  stale objects first.
- **Test runs kept destroying the player's saved settings.** Verification runs deleted
  `tuning.cfg` for deterministic defaults, and parallel test instances autosaved over
  it - which surfaced as "my settings randomly reset". Automated runs must set
  `BRAWL_TUNING=/tmp/some.cfg` so they never touch the real file.
- **zsh aborts a whole command when any glob fails to match.** `rm -f tuning.cfg
  ig_*.png` deleted *nothing* when no `ig_*.png` existed, so a stale `tuning.cfg`
  survived and the next run spawned the wrong kit - which looked exactly like a code
  bug in the spawn logic. Delete files by exact name, or glob with care.
