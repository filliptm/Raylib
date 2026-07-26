/*******************************************************************************************
*   ASSETS
*
*   Procedural texture generation, unit meshes, and the scene / post shaders.
*   Nothing here is loaded from disk: textures are synthesised from value noise and the
*   shader source is embedded, so the game has no resource directory to lose.
********************************************************************************************/
#include "assets.h"
#include "rlgl.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>

//------------------------------------------------------------------------------------
// Scene shader. Half-Lambert key light, cheap Blinn specular, a rim term to lift
// silhouettes off the background, N point lights, and distance fog for depth.
//------------------------------------------------------------------------------------
static const char *VS_LIGHTING =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec2 vertexTexCoord;\n"
"in vec3 vertexNormal;\n"
"in vec4 vertexColor;\n"
"uniform mat4 mvp;\n"
"uniform mat4 matModel;\n"
"uniform mat4 matNormal;\n"
"uniform vec2 uvScale;\n"
"out vec3 fragPosition;\n"
"out vec2 fragTexCoord;\n"
"out vec4 fragColor;\n"
"out vec3 fragNormal;\n"
"void main()\n"
"{\n"
"    fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));\n"
"    fragTexCoord = vertexTexCoord*uvScale;\n"
"    fragColor = vertexColor;\n"
"    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));\n"
"    gl_Position = mvp*vec4(vertexPosition, 1.0);\n"
"}\n";

static const char *FS_LIGHTING =
"#version 330\n"
"in vec3 fragPosition;\n"
"in vec2 fragTexCoord;\n"
"in vec4 fragColor;\n"
"in vec3 fragNormal;\n"
"uniform sampler2D texture0;\n"
"uniform vec4 colDiffuse;\n"
"uniform vec3 viewPos;\n"
"uniform vec3 sunDir;\n"
"uniform vec3 sunColor;\n"
"uniform vec3 ambientColor;\n"
"uniform vec3 fogColor;\n"
"uniform float fogDensity;\n"
"uniform float emissive;\n"
"#define MAX_LIGHTS 8\n"
"uniform vec3 lightPos[MAX_LIGHTS];\n"
"uniform vec3 lightColor[MAX_LIGHTS];\n"
"uniform int lightCount;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
"    vec4 texel = texture(texture0, fragTexCoord);\n"
"    vec3 albedo = texel.rgb*colDiffuse.rgb*fragColor.rgb;\n"
"    float alpha = texel.a*colDiffuse.a*fragColor.a;\n"
"    if (alpha < 0.02) discard;\n"
"    vec3 N = normalize(fragNormal);\n"
"    vec3 V = normalize(viewPos - fragPosition);\n"
"    vec3 L = normalize(-sunDir);\n"
"    float ndl = max(dot(N, L), 0.0);\n"
"    float wrapped = ndl*0.72 + 0.28;\n"          // half-Lambert keeps shadowed sides readable
"    vec3 light = ambientColor + sunColor*wrapped;\n"
"    vec3 H = normalize(L + V);\n"
"    float spec = pow(max(dot(N, H), 0.0), 42.0)*0.22*ndl;\n"
"    for (int i = 0; i < lightCount; i++)\n"
"    {\n"
"        vec3 delta = lightPos[i] - fragPosition;\n"
"        float dist = length(delta);\n"
"        float att = 1.0/(1.0 + 0.28*dist + 0.12*dist*dist);\n"
"        float pdl = max(dot(N, normalize(delta)), 0.0)*0.65 + 0.35;\n"
"        light += lightColor[i]*att*pdl;\n"
"    }\n"
"    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0)*0.30;\n"
"    vec3 color = albedo*light + vec3(spec) + rim*sunColor*albedo;\n"
"    color = mix(color, albedo, clamp(emissive, 0.0, 1.0));\n"
"    float viewDist = length(viewPos - fragPosition);\n"
"    float fog = 1.0 - exp(-fogDensity*viewDist);\n"
"    color = mix(color, fogColor, clamp(fog, 0.0, 1.0));\n"
"    finalColor = vec4(color, alpha);\n"
"}\n";

//------------------------------------------------------------------------------------
// Post pass: threshold-and-blur bloom plus a vignette and a gentle contrast lift.
//------------------------------------------------------------------------------------
static const char *FS_POST =
"#version 330\n"
"in vec2 fragTexCoord;\n"
"in vec4 fragColor;\n"
"uniform sampler2D texture0;\n"
"uniform vec2 resolution;\n"
"uniform float bloomStrength;\n"
"uniform float vignetteStrength;\n"
"out vec4 finalColor;\n"
"vec3 sampleBright(vec2 uv)\n"
"{\n"
"    vec3 c = texture(texture0, uv).rgb;\n"
"    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
"    return c*smoothstep(0.55, 1.0, lum);\n"
"}\n"
"void main()\n"
"{\n"
"    vec2 uv = fragTexCoord;\n"
"    vec3 base = texture(texture0, uv).rgb;\n"
"    vec3 bloom = vec3(0.0);\n"
"    if (bloomStrength > 0.001)\n"
"    {\n"
"        vec2 px = 1.0/resolution;\n"
"        for (int i = 0; i < 12; i++)\n"
"        {\n"
"            float a = float(i)*0.5235988;\n"      // 12 taps around a circle
"            vec2 dir = vec2(cos(a), sin(a));\n"
"            bloom += sampleBright(uv + dir*px*3.0);\n"
"            bloom += sampleBright(uv + dir*px*7.0);\n"
"        }\n"
"        bloom /= 24.0;\n"
"    }\n"
"    vec3 color = base + bloom*bloomStrength;\n"
"    color = mix(color, color*color*(3.0 - 2.0*color), 0.18);\n"   // soft S-curve contrast
"    vec2 centred = uv - 0.5;\n"
"    float vig = 1.0 - dot(centred, centred)*vignetteStrength;\n"
"    color *= clamp(vig, 0.0, 1.0);\n"
"    finalColor = vec4(color, 1.0);\n"
"}\n";

//------------------------------------------------------------------------------------
// Value noise, used to give every surface some grain instead of flat colour.
//------------------------------------------------------------------------------------
static float Hash2(int x, int y, int seed)
{
    int h = x*374761393 + y*668265263 + seed*1442695040;
    h = (h ^ (h >> 13))*1274126177;
    return (float)((h ^ (h >> 16)) & 0xFFFFFF)/(float)0xFFFFFF;
}

static float ValueNoise(float x, float y, int seed)
{
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float xf = x - xi, yf = y - yi;

    float u = xf*xf*(3.0f - 2.0f*xf);
    float v = yf*yf*(3.0f - 2.0f*yf);

    float a = Hash2(xi, yi, seed),     b = Hash2(xi + 1, yi, seed);
    float c = Hash2(xi, yi + 1, seed), d = Hash2(xi + 1, yi + 1, seed);

    return (a*(1 - u) + b*u)*(1 - v) + (c*(1 - u) + d*u)*v;
}

static float Fbm(float x, float y, int seed, int octaves)
{
    float sum = 0.0f, amp = 0.5f, freq = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; i++)
    {
        sum += ValueNoise(x*freq, y*freq, seed + i*17)*amp;
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum/norm;
}

static unsigned char ClampByte(float v)
{
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (unsigned char)v;
}

// Builds a texture from a per-pixel callback. Tiling is the caller's problem.
typedef void (*PixelFn)(int x, int y, int size, Color *out);

static Texture2D MakeTexture(int size, PixelFn fn, bool mipmaps)
{
    Color *pixels = (Color *)malloc((size_t)size*size*sizeof(Color));
    if (!pixels) return (Texture2D){ 0 };

    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            fn(x, y, size, &pixels[y*size + x]);

    Image img = {
        .data = pixels, .width = size, .height = size,
        .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    Texture2D tex = LoadTextureFromImage(img);
    free(pixels);

    if (mipmaps)
    {
        GenTextureMipmaps(&tex);
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    }
    else SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);

    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    return tex;
}

//------------------------------------------------------------------------------------
// Individual surfaces
//------------------------------------------------------------------------------------
static void PxFloor(int x, int y, int size, Color *out)
{
    float fx = (float)x/size, fy = (float)y/size;
    float grain = Fbm(fx*8.0f, fy*8.0f, 11, 4);
    float coarse = Fbm(fx*2.5f, fy*2.5f, 91, 3);

    float base = 74.0f + grain*20.0f + coarse*14.0f;

    // Grout line around the tile edge so the arena grid reads without extra geometry.
    int edge = size/32;
    bool onEdge = (x < edge || y < edge || x >= size - edge || y >= size - edge);
    if (onEdge) base *= 0.72f;

    // Faint inner border highlight
    if ((x == edge || y == edge || x == size - edge - 1 || y == size - edge - 1)) base *= 1.22f;

    out->r = ClampByte(base*0.82f);
    out->g = ClampByte(base*0.92f);
    out->b = ClampByte(base*1.12f);
    out->a = 255;
}

static void PxWall(int x, int y, int size, Color *out)
{
    float fx = (float)x/size, fy = (float)y/size;
    float grain = Fbm(fx*10.0f, fy*10.0f, 23, 4);
    float blotch = Fbm(fx*3.0f, fy*3.0f, 77, 3);

    float base = 108.0f + grain*30.0f + blotch*22.0f;

    // Chipped corners: darken a scatter of speckles
    if (Hash2(x, y, 5) > 0.985f) base *= 0.7f;

    out->r = ClampByte(base*0.94f);
    out->g = ClampByte(base*0.98f);
    out->b = ClampByte(base*1.10f);
    out->a = 255;
}

static void PxCrate(int x, int y, int size, Color *out)
{
    float fx = (float)x/size, fy = (float)y/size;

    // Horizontal planks with dark gaps between them
    int planks = 4;
    float plankY = fy*planks;
    float withinPlank = plankY - floorf(plankY);
    float gap = (withinPlank < 0.06f || withinPlank > 0.94f) ? 0.55f : 1.0f;

    // Wood grain runs along the plank
    float grain = Fbm(fx*22.0f, floorf(plankY)*9.7f + fy*70.0f, 31, 3);
    float base = 150.0f + grain*46.0f;
    base *= gap;

    float r = base*1.02f, g = base*0.72f, b = base*0.42f;

    // Metal banding down the left and right edges
    int band = size/10;
    if (x < band || x >= size - band)
    {
        float metal = 118.0f + Fbm(fx*30.0f, fy*30.0f, 61, 2)*40.0f;
        r = metal*0.96f; g = metal*1.0f; b = metal*1.08f;
    }

    out->r = ClampByte(r);
    out->g = ClampByte(g);
    out->b = ClampByte(b);
    out->a = 255;
}

static void PxBush(int x, int y, int size, Color *out)
{
    float fx = (float)x/size, fy = (float)y/size;
    float leaves = Fbm(fx*14.0f, fy*14.0f, 43, 4);
    float clumps = Fbm(fx*5.0f, fy*5.0f, 13, 3);

    float v = leaves*0.65f + clumps*0.35f;

    out->r = ClampByte(38.0f + v*54.0f);
    out->g = ClampByte(96.0f + v*104.0f);
    out->b = ClampByte(44.0f + v*44.0f);
    out->a = 255;
}

static void PxMetal(int x, int y, int size, Color *out)
{
    float fx = (float)x/size, fy = (float)y/size;
    float brushed = Fbm(fx*60.0f, fy*4.0f, 71, 3);
    float base = 96.0f + brushed*54.0f;

    out->r = ClampByte(base*0.95f);
    out->g = ClampByte(base*0.99f);
    out->b = ClampByte(base*1.12f);
    out->a = 255;
}

static void PxCloth(int x, int y, int size, Color *out)
{
    float fx = (float)x/size, fy = (float)y/size;
    float weave = Fbm(fx*26.0f, fy*26.0f, 97, 3);

    // Kept near-white so colDiffuse controls the team colour.
    float base = 214.0f + weave*41.0f;
    out->r = out->g = out->b = ClampByte(base);
    out->a = 255;
}

static void PxFlat(int x, int y, int size, Color *out)
{
    (void)x; (void)y; (void)size;
    *out = (Color){ 255, 255, 255, 255 };
}

static void PxGlow(int x, int y, int size, Color *out)
{
    float cx = (x + 0.5f)/size - 0.5f;
    float cy = (y + 0.5f)/size - 0.5f;
    float d = sqrtf(cx*cx + cy*cy)*2.0f;

    float a = 1.0f - d;
    if (a < 0.0f) a = 0.0f;
    a = a*a;                     // tighter, softer falloff than a linear ramp

    out->r = out->g = out->b = 255;
    out->a = ClampByte(a*255.0f);
}

//------------------------------------------------------------------------------------
bool AssetsLoad(Assets *a, int screenW, int screenH)
{
    *a = (Assets){ 0 };

    //--- Shaders --------------------------------------------------------------
    a->lighting = LoadShaderFromMemory(VS_LIGHTING, FS_LIGHTING);
    a->lightingOk = (a->lighting.id > 0) && (a->lighting.locs != NULL);

    if (a->lightingOk)
    {
        a->lighting.locs[SHADER_LOC_MATRIX_MODEL]  = GetShaderLocation(a->lighting, "matModel");
        a->lighting.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(a->lighting, "matNormal");

        a->locViewPos    = GetShaderLocation(a->lighting, "viewPos");
        a->locSunDir     = GetShaderLocation(a->lighting, "sunDir");
        a->locSunColor   = GetShaderLocation(a->lighting, "sunColor");
        a->locAmbient    = GetShaderLocation(a->lighting, "ambientColor");
        a->locFogColor   = GetShaderLocation(a->lighting, "fogColor");
        a->locFogDensity = GetShaderLocation(a->lighting, "fogDensity");
        a->locUvScale    = GetShaderLocation(a->lighting, "uvScale");
        a->locEmissive   = GetShaderLocation(a->lighting, "emissive");
        a->locLightPos   = GetShaderLocation(a->lighting, "lightPos");
        a->locLightColor = GetShaderLocation(a->lighting, "lightColor");
        a->locLightCount = GetShaderLocation(a->lighting, "lightCount");

        // Static scene lighting: a cool key from up and behind, warm ambient bounce.
        Vector3 sunDir = Vector3Normalize((Vector3){ -0.45f, -1.0f, 0.35f });
        Vector3 sunColor = { 1.05f, 1.00f, 0.92f };
        Vector3 ambient = { 0.30f, 0.34f, 0.46f };
        Vector3 fogColor = { 0.086f, 0.102f, 0.149f };
        float fogDensity = 0.0075f;

        SetShaderValue(a->lighting, a->locSunDir, &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->lighting, a->locSunColor, &sunColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->lighting, a->locAmbient, &ambient, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->lighting, a->locFogColor, &fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->lighting, a->locFogDensity, &fogDensity, SHADER_UNIFORM_FLOAT);
    }

    a->post = LoadShaderFromMemory(NULL, FS_POST);
    a->postOk = (a->post.id > 0) && (a->post.locs != NULL);
    if (a->postOk)
    {
        a->locResolution = GetShaderLocation(a->post, "resolution");
        a->locBloom      = GetShaderLocation(a->post, "bloomStrength");
        a->locVignette   = GetShaderLocation(a->post, "vignetteStrength");

        Vector2 res = { (float)screenW, (float)screenH };
        SetShaderValue(a->post, a->locResolution, &res, SHADER_UNIFORM_VEC2);
    }

    //--- Meshes ---------------------------------------------------------------
    a->cube     = GenMeshCube(1.0f, 1.0f, 1.0f);
    a->sphere   = GenMeshSphere(1.0f, 14, 20);
    a->cylinder = GenMeshCylinder(1.0f, 1.0f, 18);
    a->plane    = GenMeshPlane(1.0f, 1.0f, 1, 1);

    //--- Textures -------------------------------------------------------------
    a->texFloor = MakeTexture(256, PxFloor, true);
    a->texWall  = MakeTexture(256, PxWall, true);
    a->texCrate = MakeTexture(256, PxCrate, true);
    a->texBush  = MakeTexture(128, PxBush, true);
    a->texMetal = MakeTexture(128, PxMetal, true);
    a->texCloth = MakeTexture(128, PxCloth, true);
    a->texFlat  = MakeTexture(4, PxFlat, false);
    a->texGlow  = MakeTexture(128, PxGlow, false);

    //--- Material -------------------------------------------------------------
    a->mat = LoadMaterialDefault();
    if (a->lightingOk) a->mat.shader = a->lighting;

    a->sceneTarget = LoadRenderTexture(screenW, screenH);
    SetTextureFilter(a->sceneTarget.texture, TEXTURE_FILTER_BILINEAR);

    return a->lightingOk;
}

void AssetsUnload(Assets *a)
{
    UnloadMesh(a->cube);
    UnloadMesh(a->sphere);
    UnloadMesh(a->cylinder);
    UnloadMesh(a->plane);

    UnloadTexture(a->texFloor);
    UnloadTexture(a->texWall);
    UnloadTexture(a->texCrate);
    UnloadTexture(a->texBush);
    UnloadTexture(a->texMetal);
    UnloadTexture(a->texCloth);
    UnloadTexture(a->texFlat);
    UnloadTexture(a->texGlow);

    UnloadRenderTexture(a->sceneTarget);

    // The material borrows the shader, so drop its reference before unloading it.
    a->mat.shader = (Shader){ 0 };
    UnloadMaterial(a->mat);

    if (a->lightingOk) UnloadShader(a->lighting);
    if (a->postOk) UnloadShader(a->post);
}

//------------------------------------------------------------------------------------
void DrawLit(Assets *a, Mesh mesh, Matrix transform, Texture2D tex, Color tint,
             Vector2 uvScale, float emissive)
{
    if (a->lightingOk)
    {
        SetShaderValue(a->lighting, a->locUvScale, &uvScale, SHADER_UNIFORM_VEC2);
        SetShaderValue(a->lighting, a->locEmissive, &emissive, SHADER_UNIFORM_FLOAT);
    }

    a->mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    a->mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;

    DrawMesh(mesh, a->mat, transform);
}

void AssetsSetLights(Assets *a, const Vector3 *positions, const Vector3 *colors, int count)
{
    if (!a->lightingOk) return;
    if (count > MAX_SHADER_LIGHTS) count = MAX_SHADER_LIGHTS;

    if (count > 0)
    {
        SetShaderValueV(a->lighting, a->locLightPos, positions, SHADER_UNIFORM_VEC3, count);
        SetShaderValueV(a->lighting, a->locLightColor, colors, SHADER_UNIFORM_VEC3, count);
    }
    SetShaderValue(a->lighting, a->locLightCount, &count, SHADER_UNIFORM_INT);
}

void AssetsSetCamera(Assets *a, Vector3 viewPos)
{
    if (!a->lightingOk) return;
    SetShaderValue(a->lighting, a->locViewPos, &viewPos, SHADER_UNIFORM_VEC3);
}
