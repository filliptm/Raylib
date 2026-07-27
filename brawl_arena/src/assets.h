#ifndef ASSETS_H
#define ASSETS_H

#include "types.h"

// Everything the renderer needs that is built once at startup: procedural textures,
// unit meshes, and the two shaders. No external files - textures are generated and
// shader source is embedded, so the binary is self-contained.
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

    // post uniforms
    int locResolution, locBloom, locVignette, locDepthTex, locOutline;
    int locStyleTime, locPixelate, locPainterly, locHalftone, locPosterize;
    int locGrain, locCA, locSaturation, locBrightness;
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

    // Skinned character. Optional: if the file is missing the game falls back to the
    // primitive brawlers, so a bad asset can never stop it starting.
    Shader skinned;
    bool skinnedOk;
    int kViewPos, kSunDir, kSunColor, kAmbient, kFogColor, kFogDensity;
    int kUvScale, kEmissive, kDither, kLightPos, kLightColor, kLightCount;

    Model character;
    ModelAnimation *charAnims;
    int charAnimCount;
    bool characterOk;
    // Clips resolved by name, not file order. Directionals are -1 when the model
    // lacks them and the draw code falls back to the forward run.
    int clipIdle, clipCombat, clipWalk;
    int clipRunF, clipRunB, clipRunFL, clipRunFR, clipRunBL, clipRunBR;
    int clipDeath;
    float charScale;        // normalises the source model to CHARACTER_TARGET_H
    float charFootOffset;   // lifts it so the feet land on y = 0

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

    RenderTexture2D sceneTarget;
} Assets;

bool AssetsLoad(Assets *a, int screenW, int screenH);
void AssetsUnload(Assets *a);

// Draw a mesh through the lighting shader. uvScale tiles the texture; emissive lifts
// the surface out of the lighting equation (0 = fully lit, 1 = self-lit).
void DrawLit(Assets *a, Mesh mesh, Matrix transform, Texture2D tex, Color tint,
             Vector2 uvScale, float emissive);

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
void AssetsDrawCharacter(Assets *a, Vector3 position, float yaw, float scaleMul,
                         int animIndex, float frame, bool loop, Color tint, float dither,
                         float emissive, const Vector3 *lightPos, const Vector3 *lightColor,
                         int lightCount, Vector3 viewPos);

// Per-frame grass uniforms. Actors are the brawlers that push blades aside.
void AssetsGrassFrame(Assets *a, const Tuning *t, float time, Vector3 viewPos,
                      const Vector3 *actorPos, const Vector2 *actorVel, int actorCount,
                      const Vector3 *lightPos, const Vector3 *lightColor, int lightCount);

#endif // ASSETS_H
