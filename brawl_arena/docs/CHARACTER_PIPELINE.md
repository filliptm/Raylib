# Importing rigged characters (and why AI exports collapse in raylib)

This documents the raylib repair and reusable-animation pipeline. If you only remember
one thing: **never load a raw Meshy/Tripo export directly. Import the rigged
`Character_output` model into the tracked model library, then let the build retarget the
shared clips.**

```
python3 tools/import_character.py <meshy-zip-dir-or-glb> \
  resources/characters/models/<character>.glb --id <character>
make character-assets
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

## What the low-level fixer does

`tools/fix_meshy_glb.py` (stdlib + Pillow):

- Computes the uniform scale `k` hidden in the IBMs (here: exactly 100).
- Rewrites every joint's static TRS so the node hierarchy composes to
  `k · inverse(IBM)` — the bind pose raylib will reconstruct, at **unit scale**, in
  the same space as the animation channels.
- Bakes `k` into the mesh vertex positions so the skin still matches.
- Strips transforms from all non-joint nodes (Armature wrapper, mesh node).
- Adds constant filler channels for any unkeyed joint path, so the static rewrite can
  never leak into animation.
- Can merge either per-animation GLBs or a merged-animation GLB for legacy conversion
  and animation-library authoring. Normal character imports stage only the
  `Character_output` GLB because repeated clips are no longer required.
- Standardizes every embedded texture to exactly **1024×1024 (1K)**. This is the
  runtime character-texture contract; do not generate 512px character assets.
- Verifies numerically: TRS reconstruction error and world-vs-inverse-IBM error are
  printed and should be ~1e-5 or better.

`tools/import_character.py` accepts a Meshy ZIP, directory, individual
`Character_output` GLB, or standalone merged-animation GLB. It stages the selected model
through the fixer, records the animation-coordinate reference pose needed for
retargeting, strips all animation clips, compacts the GLB, and validates the mesh-only
result. A `Character_output`/T-pose clip is preferred (recognized in any Meshy spelling,
including the raw `Armature|clip0|baselayer` name); when a merged source omits one, the
importer captures the original joint statics before bind repair as the reference pose.

An un-retopologized export (Meshy sometimes ships 100k+ triangles) is decimated to the
**8,000-triangle** import target by `tools/character_pipeline/decimate.py`. Raw Meshy
meshes are triangle soup (almost no vertex sharing), so vertices are first **welded by
position** to restore surface connectivity — collapsing a soup directly just deletes
triangles and shreds the surface. Quadric collapse (`fast_simplification`) then reduces
the welded surface; joints/weights transfer from the nearest original vertex, normals
are recomputed from the decimated surface, and — because the raw export's per-triangle
noise atlas is meaningless on any other topology — the texture is **baked into
per-vertex colors** (the lit shaders multiply albedo by vertex color; meshes without
colors default to white) with a plain white 1024x1024 PNG keeping the texture contract
satisfied. Validation enforces whole-character budgets of **16,000 triangles / 32,000
vertices** — a dense mesh can no longer slip through by being partitioned. The importer
still splits any primitive above 65,535 vertices into u16-indexed chunks, which is the
safe format raylib actually uploads, and byte-identical embedded textures are
deduplicated.

## Asset ownership and build flow

Tracked source assets and generated runtime files are deliberately different:

```text
resources/characters/models/<id>.glb
        + resources/characters/animations/meshy_humanoid_v1.glb
        + optional resources/characters/animations/overrides/<id>.glb
        + data/characters/asset_manifest.json
        |
        v  tools/build_character_assets.py
build/assets/characters/<id>.glb
```

The tracked model is the character library: mesh, materials, skin, repaired bind nodes,
one or more 1K embedded PNGs, and animation-rest-pose metadata, with no clips. The tracked
animation files are mesh-free libraries containing a donor skeleton, reference pose, and
full-TRS clips. The generated output bakes the selected clips onto the target rig and is
self-contained because that is the most reliable format for raylib.

Current assets:

| Kit | Tracked model | Mesh | Rig | Generated output |
|---|---|---:|---:|---|
| Scrapper | `resources/characters/models/sentinel.glb` | 5,210 vertices | 24 bones | `build/assets/characters/sentinel.glb` |
| Longshot | `resources/characters/models/longshot.glb` | 8,000 triangles, vertex-colored (welded + decimated from a 226k-vertex raw export) | 24 bones | `build/assets/characters/longshot.glb` |
| Tank | `resources/characters/models/ironclad_guardian.glb` | 4,888 vertices | 24 bones | `build/assets/characters/ironclad_guardian.glb` |
| Guardian | `resources/characters/models/gaia_guardian.glb` | 5,070 vertices | 24 bones | `build/assets/characters/gaia_guardian.glb` |

The canonical library contains twelve clips and is about 0.37 MiB. Scrapper has a small
override pack for idle, hit reaction, and backpedal; Guardian has one for its distinctive
idle. Generated outputs contain twelve clips ordered exactly as `CANONICAL_CLIPS`
regardless of how the libraries were assembled, with any optional action clips appended
after the canonical block; texture deduplication puts every output at about 2.5–3.1 MiB.
Build outputs are ignored and recreated after `make clean`.

To author a new library or override, first use the low-level fixer to create a compatible
combined donor GLB from the relevant Meshy animation export, then map source names to
canonical names:

```bash
python3 tools/build_animation_library.py donor.glb output.glb \
  --id library_version \
  --clip Meshy_Source_Name=canonical_name \
  --clip Another_Source_Name=another_canonical_name
```

The base library must provide all twelve canonical clips. An override may provide any
subset; later libraries listed for a character replace earlier clips with the same
canonical name.

`AssetsLoad()` measures the GPU-equivalent idle pose and normalizes every character to
`CHARACTER_TARGET_H` (currently 3.1 world units).

## Standard animation set

The generated runtime contract uses exact semantic names:

| Canonical clip | Used for |
|---|---|
| `idle` | standing and podium |
| `combat_stance` | reserved for a future blended aiming state |
| `walk_forward` | lower-speed forward movement |
| `run_forward` | forward movement and dash fallback |
| `run_backward` | backpedaling while aiming |
| `run_forward_left`, `run_forward_right` | forward diagonals |
| `run_back_left`, `run_back_right` | rear diagonals |
| `hit_reaction` | available for future hit one-shot |
| `launched_hit` | available for future knockback one-shot |
| `death` | KO pose held until respawn |

Optional action libraries may add `attack_main` (or the compatible alias `shoot`),
`attack_super`, `cast`, `mobility`, `guard`, `grapple`, and `mine_deploy`. `victory`
remains a future
presentation seam. A generic jump remains excluded because the game has no jump
mechanic.

Rules that matter more than the list:

- **Every clip must be in-place - no root motion.** The game moves the character; a
  clip that translates its root makes the feet slide.
- **One versioned library, exact semantic output names.** Raw Meshy names are mapped once
  when authoring the library, not rediscovered for every character.
- Locomotion and idle loop; action, death, and future emote clips are one-shots.

Engine-side status: implemented. The clip is chosen from the movement direction
relative to facing (forward, backward and four diagonals - a pure strafe picks the
nearest diagonal), playback rate follows actual speed so feet track the ground, death
plays as a one-shot that holds its final pose until just before respawn, and stationary
casts retain idle as their locomotion base. Successful gameplay actions emit a separate
presentation-only `MAIN`, `SUPER`, `CAST`, `MOBILITY`, `GUARD`, `GRAPPLE`, or
`MINE_DEPLOY` event. The renderer applies a
short blend envelope over the locomotion pose, using an optional semantic action clip
when one exists and a restrained upper-body procedural fallback otherwise. Repeated
actions restart that envelope. The current twelve-clip shared and override libraries do
not contain semantic action clips, so Scrapper, Longshot, Tank, and Guardian currently
use the procedural overlays. Magnetic Scrap Shell reuses the internal `guard` semantic
action; its fallback braces both arms while the 360-degree bubble carries directionless
defense readability. Grapple is a three-phase reach/brace/tuck pose, while Mine Deploy
uses the hips and leg chains for a kneel/plant/recovery pose. The remaining overlays are
restrained upper-body actions. They use the shared Meshy shoulder, arm, hand, spine,
chest, hip, and leg names rather than being authored per character.

The same final composed pose supplies semantic `CENTER`, `CHEST`, hand, shoulder, and
foot sockets to ability VFX. Missing mappings retain the approximate socket pose seeded
by the renderer, so an incomplete rig does not collapse an effect to the actor origin.

Concealment's `revealTimer` is deliberately not animation state and cannot control
either the base clip or the action duration. The generic combat stance remains in the
runtime contract but is not selected until the presentation layer has a continuous
blended aiming state. Runtime clips are resolved by exact canonical name with compatible
movement fallbacks. Still open: authored per-character action/emote clips and locomotion
crossfade blending.

The tool accepts both Meshy export styles: one GLB per animation (clip named from the
filename) and the newer single merged-animations GLB (each clip keeps its own name,
lowercased). The file carrying the most clips becomes the base.

## How shared animation retargeting works

Implemented build-time reuse maps joints by name and requires an identical name/parent
topology fingerprint. It does **not** assume that equal bone count means equal pose. The
four current Meshy rigs share the same topology, but measured local bind differences
reach about 26 source units and 170 degrees.

Each model and animation library therefore stores the animation-coordinate reference
pose that existed before raylib's bind repair. Every clip is resampled onto a shared
per-clip time grid, the donor hierarchy is composed per frame, and rotation deltas are
transferred in **world space**:

```text
world delta       = donor animated world rotation × inverse(donor rest world rotation)
target world pose = world delta × target rest world rotation
target local pose = inverse(target parent world pose) × target world pose
```

Applying deltas in local joint frames instead (the previous approach) silently assumes
the donor and target rest poses use similar joint axes; a character whose export has no
T-pose - its rest is an action stance - came out visually shredded because every delta
rotated around the wrong axis. The world-space transfer reduces to replaying the donor
clip exactly when the rest poses coincide. Translation keeps the target's bone lengths,
maps motion through the donor/target parent reference axes, and scales motion by
rig-height ratio; the root follows the donor's scaled world trajectory so a rest-stance
hip offset does not shift every frame. Horizontal root drift is removed while preserving
in-place sway. The output keys every joint's translation, rotation, and scale.

`tools/check_character_assets.py` rejects mismatched topology, incomplete channels,
unsupported interpolation, non-finite keys, missing canonical clips, stale metadata,
horizontal root drift, and invalid textures. raylib then validates every generated clip
against the loaded model again before accepting the character.

## Checklist for adding a new character

1. In Meshy, download the rigged **GLB** `Character_output` model, the ZIP containing it,
   or a compatible standalone merged-animation GLB. FBX/USDZ are not supported.
   Re-exporting the standard animation set is unnecessary.
2. Prefer a retopologized export (~4–8k tris). A raw high-poly export is accepted but
   is decimated to the 8,000-triangle import target, which costs surface detail the
   retopologized export would have kept. Then import:

   ```bash
   python3 tools/import_character.py /path/to/export.zip \
     resources/characters/models/<name>.glb --id <name>
   ```

3. Add its ID, class, tracked model path, optional override packs, and generated output
   to `data/characters/asset_manifest.json`.
4. Register the generated `build/assets/characters/<name>.glb` for its kit in
   `CHARACTER_MODEL_PATHS` in `src/presentation/assets.c`.
5. Run `make character-assets` and `make check-character-assets`. Import errors should
   report reconstruction error around 1e-5, 24 compatible joints, and one or more
   1024×1024 textures. A topology mismatch is a hard failure, not an invitation to force
   the file. Dense triangle input is decimated to the import target and validated against
   the 16,000-triangle / 32,000-vertex whole-character budget.
6. Run the Python pipeline test and normal C suite, then inspect every movement direction,
   idle, combat stance, and death pose:

   ```bash
   python3 tests/test_character_pipeline.py
   make test
   ```

   The log line to look for is `CHARACTER <KIT>: <verts> verts, <bones> bones, <clips>
   clips, posed height <H>, scale <s>` — posed height should be a sane skeleton-space
   number (tens to hundreds), not thousands.
7. Look at the menu podium. The model must stand on the disc, facing the camera,
   idle-animating. If it's a spike-ball or invisible, re-read this document.

Hard limits: ≤128 bones (`MAX_BONE_NUM` in the skinned shader), one skin per file,
LINEAR/STEP keyframes, embedded PNG textures at exactly 1024×1024, ≤65,535 vertices
per runtime primitive with unsigned 8/16-bit indices, and ≤16,000 triangles /
≤32,000 vertices per whole character. The importer decimates and partitions dense
triangle primitives automatically and rejects unsupported dense morph/non-triangle data
instead of allowing raylib to truncate 32-bit indices. Decimation requires
`fast_simplification` (`python3 -m pip install fast_simplification`); imports that are
already inside the budget do not need it.

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
- **Test runs kept destroying the player's saved settings.** Older verification runs
  deleted or overwrote `tuning.cfg`, which surfaced as “my settings randomly reset.”
  Automated runs now isolate all four paths with `BRAWL_PROJECT_CONFIG`,
  `BRAWL_TUNING`, `BRAWL_PROFILE`, and `BRAWL_LEGACY_TUNING`; never point a test at the
  real local files or tracked `config/gameplay.cfg`.
- **zsh aborts a whole command when any glob fails to match.** An older cleanup command
  mixed a tuning filename with `ig_*.png`; when the image glob had no match, nothing was
  deleted and stale state survived. Delete known temporary files by exact name, and
  treat unmatched globs carefully.
