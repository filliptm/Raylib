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
    int locResolution, locBloom, locVignette;

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

#endif // ASSETS_H
