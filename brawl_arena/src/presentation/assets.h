#ifndef ASSETS_H
#define ASSETS_H

#include "content_types.h"
#include "presentation_types.h"

#define CHARACTER_BONE_LIMIT 128

typedef struct RiggedCharacter {
    Model model;
    ModelAnimation *anims;
    int animCount;
    bool ok;

    // Clips are resolved by name, not file order. Directionals are filled with
    // compatible fallbacks when a source model does not contain every track.
    int clipIdle, clipCombat, clipWalk;
    int clipRunF, clipRunB, clipRunFL, clipRunFR, clipRunBL, clipRunBR;
    int clipDeath;
    // Optional library clips. Current generated characters use the shared
    // procedural action overlay when these tracks are absent.
    int clipActionMain, clipActionSuper, clipActionCast, clipActionMobility;
    int clipActionGuard, clipActionGrapple, clipActionMineDeploy;
    int boneHips, boneSpine, boneChest;
    int boneHead;
    int boneLeftShoulder, boneLeftArm, boneLeftForeArm, boneLeftHand;
    int boneRightShoulder, boneRightArm, boneRightForeArm, boneRightHand;
    int boneLeftUpLeg, boneLeftLeg;
    int boneRightUpLeg, boneRightLeg;
    int boneLeftFoot, boneRightFoot;
    float scale;            // normalises source units to CHARACTER_TARGET_H
    float footOffset;       // lifts the model so its feet land on y = 0
} RiggedCharacter;

// Static pieces from Kenney's Space Station Kit. The complete GLB collection is kept
// under resources/ for future arenas, while this fixed runtime set is the subset used
// by the current Helios-9 map.
typedef enum StationModelId {
    STATION_FLOOR_PANEL = 0,
    STATION_FLOOR_DETAIL,
    STATION_STRUCTURE_PANEL,
    STATION_STRUCTURE,
    STATION_STRUCTURE_BARRIER,
    STATION_WALL,
    STATION_WALL_CORNER,
    STATION_WALL_PILLAR,
    STATION_WALL_WINDOW,
    STATION_WALL_BANNER,
    STATION_DOOR_DOUBLE_CLOSED,
    STATION_CONTAINER,
    STATION_CONTAINER_WIDE,
    STATION_CONTAINER_TALL,
    STATION_COMPUTER_SYSTEM,
    STATION_COMPUTER_WIDE,
    STATION_DISPLAY_WALL,
    STATION_PIPE,
    STATION_PIPE_BEND,
    STATION_RAIL,
    STATION_TABLE_DISPLAY_PLANET,
    STATION_SKIP,
    STATION_MODEL_COUNT
} StationModelId;

typedef enum StationPalette {
    STATION_PALETTE_ORANGE = 0,
    STATION_PALETTE_PURPLE
} StationPalette;

typedef struct StationModel {
    Model model;
    bool ok;
} StationModel;

typedef enum VfxAtlasId {
    VFX_ATLAS_SHAPES = 0,
    VFX_ATLAS_EXPLOSION,
    VFX_ATLAS_WATER,
    VFX_ATLAS_ENERGY_LOOP,
    VFX_ATLAS_AIR_BURST,
    VFX_ATLAS_DIVINE_IMPACT,
    VFX_ATLAS_SMOKE_LOOP,
    VFX_ATLAS_COUNT
} VfxAtlasId;

// Everything the renderer needs that is built once at startup: generated textures,
// unit meshes, shaders, optional per-kit characters, and static station models.
typedef struct Assets {
    // Shaders
    Shader lighting;
    Shader post;
    bool lightingOk;
    bool postOk;

    // lighting uniforms
    int locViewPos, locSunDir, locSunColor, locAmbient;
    int locFogColor, locFogDensity, locUvScale, locEmissive;
    int locLightPos, locLightColor, locLightCount;

    // Last-uploaded per-draw lighting uniforms. Nearly every lit draw uses the same
    // uv scale and emissive, so redundant glUniform traffic is skipped.
    Vector2 litUvScale;
    float litEmissive;
    bool litStateValid;

    // post uniforms
    int locResolution, locBloom, locVignette, locDepthTex, locOutline;
    int locClipNear, locClipFar;
    int locStyleTime, locPixelate, locPainterly, locHalftone, locPosterize;
    int locGrain, locCA, locSaturation, locBrightness;
    int locExposure, locTonemap;
    bool depthOk;           // scene target carries a sampleable depth texture

    // toon uniforms, one pair per lit shader
    int locToon, locToonBands;      // scene
    int kToon, kToonBands;          // skinned
    int gToon, gToonBands;          // grass

    // Grass: instanced, wind-animated, alpha-cutout blades
    Shader grass;
    bool grassOk;
    Mesh grassBlade;
    Material grassMat;
    Texture2D texGrass;

    int gTime, gHeight, gWindStrength, gWindSpeed, gWindDir;
    int gActorPos, gActorVel, gActorCount, gBendRadius, gBendStrength;
    int gViewPos, gSunDir, gSunColor, gAmbient, gFogColor, gFogDensity;
    int gBaseColor, gTipColor, gLightPos, gLightColor, gLightCount;

    // Screen-door transparency on the scene shader, used for concealed brawlers.
    int locDither;

    // Skinned characters. Each slot is optional: a missing or bad file falls back to
    // that kit's primitive brawler without stopping the game.
    Shader skinned;
    bool skinnedOk;
    int kViewPos, kSunDir, kSunColor, kAmbient, kFogColor, kFogDensity;
    int kUvScale, kEmissive, kDither, kLightPos, kLightColor, kLightCount;

    RiggedCharacter characters[CLASS_COUNT];

    // Static environment models share two atlas textures and the scene lighting shader.
    // Each model is optional so the procedural arena remains a valid fallback.
    StationModel station[STATION_MODEL_COUNT];
    Texture2D texStationOrange;
    Texture2D texStationPurple;
    bool stationTexturesOk;

    // Unit meshes, scaled into place with a matrix at draw time
    Mesh cube;      // 1x1x1 centred on origin
    Mesh sphere;    // radius 1 centred on origin
    Mesh cylinder;  // radius 1, height 1, base at y=0
    Mesh plane;     // 1x1 on the XZ plane

    Material mat;   // shared material bound to the lighting shader

    // Textures
    Texture2D texFloor;
    Texture2D texWall;
    Texture2D texCrate;
    Texture2D texBush;
    Texture2D texMetal;
    Texture2D texCloth;
    Texture2D texFlat;      // 1x1 white
    Texture2D texGlow;      // soft radial falloff, used for glows and shadows
    Texture2D texStarfield; // deep-space backdrop with stars and nebula wisps
    Texture2D texPlanet;    // shaded planet disc with atmosphere rim, alpha edged

    // Optional presentation-only ability flipbooks. If an atlas is missing, the
    // event still produces the existing procedural particles/lights/telegraphs.
    Texture2D vfxAtlases[VFX_ATLAS_COUNT];
    bool vfxAtlasesOk[VFX_ATLAS_COUNT];

    RenderTexture2D sceneTarget;
} Assets;

bool AssetsLoad(Assets *a, int screenW, int screenH);
void AssetsUnload(Assets *a);
bool AssetsResizeViewport(Assets *a, int screenW, int screenH);

// Drops and reloads the VFX flipbook atlases from disk, for the studio's
// rebuild-and-reload flow. Safe when some atlases are missing.
void AssetsReloadVfxAtlases(Assets *a);

// Draw a mesh through the lighting shader. uvScale tiles the texture; emissive lifts
// the surface out of the lighting equation (0 = fully lit, 1 = self-lit).
void DrawLit(Assets *a, Mesh mesh, Matrix transform, Texture2D tex, Color tint,
             Vector2 uvScale, float emissive);

// Draws every mesh in one static station model using the shared orange or purple atlas.
// Returns false when the requested asset is unavailable so callers can draw a primitive
// fallback without making collision geometry invisible.
bool AssetsDrawStationModel(Assets *a, StationModelId id, Matrix transform,
                            StationPalette palette, Color tint, float emissive);

// Upload this frame's point lights.
void AssetsSetLights(Assets *a, const Vector3 *positions, const Vector3 *colors, int count);
void AssetsSetCamera(Assets *a, Vector3 viewPos);

// Screen-door transparency: 0 draws solid, higher values discard more pixels on a
// Bayer pattern. Avoids the sorting problems real alpha blending would bring.
void AssetsSetDither(Assets *a, float amount);

// Pushes the toon state to every lit shader. Call once per frame before drawing.
void AssetsSetToon(Assets *a, bool enabled, float bands);

// Uploads the whole viewport-style block to the post shader in one call.
void AssetsSetStyle(Assets *a, const Tuning *t, float time, float outlineStrength);

// Poses and draws the skinned character. `frame` is fractional; it is floored, so a
// caller can advance it at whatever rate suits. `dither` is the screen-door amount for
// bush concealment and `emissive` lifts the surface out of lighting (used for hit flash).
// `loop` wraps the frame; one-shot clips (death) clamp on their last frame instead.
// A current action is crossfaded over locomotion. When its optional authored clip is
// missing, a shared Meshy-biped procedural pose is used. `socketPose` receives the
// final posed joint locations in world space and may be NULL.
// `fadeClip`/`fadeFrame`/`fadeAlpha` crossfade out of a previous clip: the outgoing
// pose (frozen at `fadeFrame`) blends beneath the incoming clip with weight
// 1-fadeAlpha. Pass fadeClip -1 (and fadeAlpha 1) when no fade is active.
// Uploads the camera and light uniforms shared by every skinned character drawn
// this frame. Call once per scene before AssetsDrawCharacter; the identical arrays
// were previously re-uploaded for every single character.
void AssetsSkinnedFrame(Assets *a, const Vector3 *lightPos,
                        const Vector3 *lightColor, int lightCount,
                        Vector3 viewPos);

// `authoredMotions` (nullable) replaces the clip/procedural action overlay with a
// stacked motion vocabulary; `actionSeconds` is the action's age driving their
// per-motion envelopes.
void AssetsDrawCharacter(Assets *a, BrawlerClass cls, Vector3 position, float yaw, float scaleMul,
                         int animIndex, float frame, bool loop,
                         int fadeClip, float fadeFrame, float fadeAlpha,
                         Color tint, float dither,
                         float emissive, CharacterActionId action,
                         float actionProgress, float actionWeight,
                         const AttackMotion *authoredMotions, float actionSeconds,
                         CharacterSocketPose *socketPose);

// Per-frame grass uniforms. Actors are the brawlers that push blades aside.
void AssetsGrassFrame(Assets *a, const Tuning *t, float time, Vector3 viewPos,
                      const Vector3 *actorPos, const Vector2 *actorVel, int actorCount,
                      const Vector3 *lightPos, const Vector3 *lightColor, int lightCount);

#endif // ASSETS_H
