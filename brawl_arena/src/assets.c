/*******************************************************************************************
*   ASSETS
*
*   Procedural texture generation, unit meshes, and the scene / post shaders.
*   Nothing here is loaded from disk: textures are synthesised from value noise and the
*   shader source is embedded, so the game has no resource directory to lose.
********************************************************************************************/
#include "assets.h"

#define CHARACTER_MODEL_PATH "resources/sentinel.glb"
#include "rlgl.h"
#include "raymath.h"
#include <stdlib.h>
#include <string.h>
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
"uniform float dither;\n"
"#define MAX_LIGHTS 8\n"
"uniform vec3 lightPos[MAX_LIGHTS];\n"
"uniform vec3 lightColor[MAX_LIGHTS];\n"
"uniform int lightCount;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
"    if (dither > 0.001)\n"
"    {\n"
"        ivec2 p = ivec2(mod(gl_FragCoord.xy, 4.0));\n"
"        int idx = p.y*4 + p.x;\n"
"        float bayer[16] = float[16](0.0625,0.5625,0.1875,0.6875, 0.8125,0.3125,0.9375,0.4375,\n"
"                                    0.25,0.75,0.125,0.625, 1.0,0.5,0.875,0.375);\n"
"        if (bayer[idx] < dither) discard;\n"
"    }\n"
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
// Skinned character shader. Same lighting as the scene shader; the only difference is
// that the vertex stage poses the mesh from the bone matrices first.
//
// The attribute and uniform names are exact: raylib binds vertexBoneIds,
// vertexBoneWeights and boneMatrices by name, so a typo silently yields a T-pose.
//------------------------------------------------------------------------------------
static const char *VS_SKINNED =
"#version 330\n"
"#define MAX_BONE_NUM 128\n"
"in vec3 vertexPosition;\n"
"in vec2 vertexTexCoord;\n"
"in vec3 vertexNormal;\n"
"in vec4 vertexColor;\n"
"in vec4 vertexBoneIds;\n"
"in vec4 vertexBoneWeights;\n"
"uniform mat4 mvp;\n"
"uniform mat4 matModel;\n"
"uniform mat4 matNormal;\n"
"uniform mat4 boneMatrices[MAX_BONE_NUM];\n"
"uniform vec2 uvScale;\n"
"out vec3 fragPosition;\n"
"out vec2 fragTexCoord;\n"
"out vec4 fragColor;\n"
"out vec3 fragNormal;\n"
"void main()\n"
"{\n"
"    int b0 = int(vertexBoneIds.x);\n"
"    int b1 = int(vertexBoneIds.y);\n"
"    int b2 = int(vertexBoneIds.z);\n"
"    int b3 = int(vertexBoneIds.w);\n"
"    vec4 pos = vec4(vertexPosition, 1.0);\n"
"    vec4 skinned = vertexBoneWeights.x*(boneMatrices[b0]*pos)\n"
"                 + vertexBoneWeights.y*(boneMatrices[b1]*pos)\n"
"                 + vertexBoneWeights.z*(boneMatrices[b2]*pos)\n"
"                 + vertexBoneWeights.w*(boneMatrices[b3]*pos);\n"
"    vec4 nrm = vec4(vertexNormal, 0.0);\n"
"    vec4 sn = vertexBoneWeights.x*(boneMatrices[b0]*nrm)\n"
"            + vertexBoneWeights.y*(boneMatrices[b1]*nrm)\n"
"            + vertexBoneWeights.z*(boneMatrices[b2]*nrm)\n"
"            + vertexBoneWeights.w*(boneMatrices[b3]*nrm);\n"
"    fragPosition = vec3(matModel*skinned);\n"
"    fragTexCoord = vertexTexCoord*uvScale;\n"
"    fragColor = vertexColor;\n"
"    fragNormal = normalize(vec3(matNormal*vec4(sn.xyz, 1.0)));\n"
"    gl_Position = mvp*skinned;\n"
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
// Grass shader. Instanced cross-quads, alpha cut out rather than blended so the depth
// buffer resolves order for us and no per-frame sorting is needed.
//
// Bending is weighted by height up the blade, so roots stay planted and only the tips
// travel - that single detail is what separates grass that sways from grass that slides.
//------------------------------------------------------------------------------------
static const char *VS_GRASS =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec2 vertexTexCoord;\n"
"in vec3 vertexNormal;\n"
"in mat4 instanceTransform;\n"
"uniform mat4 mvp;\n"
"uniform mat4 matNormal;\n"
"uniform float time;\n"
"uniform float grassHeight;\n"
"uniform float windStrength;\n"
"uniform float windSpeed;\n"
"uniform vec2 windDir;\n"
"uniform float bendRadius;\n"
"uniform float bendStrength;\n"
"#define MAX_ACTORS 8\n"
"uniform vec3 actorPos[MAX_ACTORS];\n"
"uniform vec2 actorVel[MAX_ACTORS];\n"
"uniform int actorCount;\n"
"out vec3 fragPosition;\n"
"out vec2 fragTexCoord;\n"
"out vec3 fragNormal;\n"
"out float fragUp;\n"
"void main()\n"
"{\n"
"    vec3 local = vertexPosition;\n"
"    local.y *= grassHeight;\n"
"    vec3 world = vec3(instanceTransform*vec4(local, 1.0));\n"
"    vec3 root = vec3(instanceTransform[3][0], instanceTransform[3][1], instanceTransform[3][2]);\n"
"    float up = clamp(vertexPosition.y, 0.0, 1.0);\n"
"    float weight = up*up;\n"                       // quadratic: anchored at the root
"    float phase = dot(root.xz, vec2(0.42, 0.31));\n"
"    float sway = sin(time*windSpeed + phase)*0.65 + sin(time*windSpeed*1.73 + phase*1.4)*0.35;\n"
"    vec2 offset = windDir*sway*windStrength;\n"
"    for (int i = 0; i < actorCount; i++)\n"
"    {\n"
"        vec2 delta = root.xz - actorPos[i].xz;\n"
"        float dist = length(delta);\n"
"        if (dist < bendRadius)\n"
"        {\n"
"            float falloff = 1.0 - dist/bendRadius;\n"
"            falloff *= falloff;\n"
"            vec2 away = (dist > 0.001) ? delta/dist : vec2(1.0, 0.0);\n"
"            offset += (away*0.8 + actorVel[i]*0.3)*falloff*bendStrength;\n"
"        }\n"
"    }\n"
"    world.xz += offset*weight;\n"
"    world.y -= length(offset)*weight*0.22;\n"      // bending shortens rather than stretches
"    fragPosition = world;\n"
"    fragTexCoord = vertexTexCoord;\n"
"    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));\n"
"    fragUp = up;\n"
"    gl_Position = mvp*vec4(world, 1.0);\n"
"}\n";

static const char *FS_GRASS =
"#version 330\n"
"in vec3 fragPosition;\n"
"in vec2 fragTexCoord;\n"
"in vec3 fragNormal;\n"
"in float fragUp;\n"
"uniform sampler2D texture0;\n"
"uniform vec3 viewPos;\n"
"uniform vec3 sunDir;\n"
"uniform vec3 sunColor;\n"
"uniform vec3 ambientColor;\n"
"uniform vec3 fogColor;\n"
"uniform float fogDensity;\n"
"uniform vec3 baseColor;\n"
"uniform vec3 tipColor;\n"
"#define MAX_LIGHTS 8\n"
"uniform vec3 lightPos[MAX_LIGHTS];\n"
"uniform vec3 lightColor[MAX_LIGHTS];\n"
"uniform int lightCount;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
"    vec4 texel = texture(texture0, fragTexCoord);\n"
"    if (texel.a < 0.45) discard;\n"                // cutout: order-independent
"    vec3 albedo = mix(baseColor, tipColor, fragUp)*texel.rgb;\n"
"    vec3 N = normalize(fragNormal);\n"
"    vec3 V = normalize(viewPos - fragPosition);\n"
"    if (dot(N, V) < 0.0) N = -N;\n"                // foliage is two sided
"    vec3 L = normalize(-sunDir);\n"
"    float ndl = max(dot(N, L), 0.0)*0.6 + 0.4;\n"
"    vec3 light = ambientColor + sunColor*ndl;\n"
"    for (int i = 0; i < lightCount; i++)\n"
"    {\n"
"        vec3 d = lightPos[i] - fragPosition;\n"
"        float dist = length(d);\n"
"        float att = 1.0/(1.0 + 0.28*dist + 0.12*dist*dist);\n"
"        light += lightColor[i]*att;\n"
"    }\n"
"    vec3 color = albedo*light;\n"
"    color += sunColor*pow(fragUp, 3.0)*0.10;\n"    // tips catch a little extra sun
"    float viewDist = length(viewPos - fragPosition);\n"
"    float fog = 1.0 - exp(-fogDensity*viewDist);\n"
"    color = mix(color, fogColor, clamp(fog, 0.0, 1.0));\n"
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

// Riveted steel plate. Crates are warm planked wood that you can blow apart; permanent
// walls are cold, bolted and panelled, so the two never get confused at a glance.
static void PxWall(int x, int y, int size, Color *out)
{
    float u = (float)x/size;
    float v = (float)y/size;             // 0 at the top of the face
    float up = 1.0f - v;

    float grain = Fbm(u*26.0f, v*6.0f, 23, 3);      // brushed, mostly horizontal
    float weather = Fbm(u*3.5f, v*3.5f, 77, 3);
    float base = 104.0f + grain*26.0f + weather*22.0f;

    // Grubbier toward the floor so the block feels planted rather than floating.
    base *= 0.76f + up*0.32f;

    // Recessed seams split the face into four bolted plates, plus an outer groove.
    bool seam = (fabsf(u - 0.5f) < 0.042f) || (fabsf(v - 0.5f) < 0.042f);
    bool border = (u < 0.052f) || (u > 0.948f) || (v < 0.052f) || (v > 0.948f);
    if (seam || border) base *= 0.50f;

    float r = base*0.91f, g = base*0.97f, b = base*1.19f;

    // A bolt in the outer corner of each plate.
    const float bolts[4][2] = { { 0.18f, 0.18f }, { 0.82f, 0.18f },
                                { 0.18f, 0.82f }, { 0.82f, 0.82f } };
    for (int i = 0; i < 4; i++)
    {
        float dx = u - bolts[i][0], dy = v - bolts[i][1];
        float d = sqrtf(dx*dx + dy*dy);
        if (d < 0.034f)
        {
            // Brighter on the upper-left of the stud so it reads as domed.
            float lit = 1.62f - (d/0.034f)*0.55f - (dx + dy)*2.4f;
            if (lit < 0.55f) lit = 0.55f;
            r = base*lit*1.02f; g = base*lit*1.06f; b = base*lit*1.18f;
        }
    }

    // Scattered pitting
    if (Hash2(x, y, 5) > 0.988f) { r *= 0.68f; g *= 0.68f; b *= 0.72f; }

    out->r = ClampByte(r);
    out->g = ClampByte(g);
    out->b = ClampByte(b);
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

// A card of tapered blades with hard alpha edges, ready for cutout rendering.
// Row 0 is the top of the image, which is the tip of the blade.
static void PxGrass(int x, int y, int size, Color *out)
{
    const int BLADES = 5;
    float u = (float)x/size;
    float t = 1.0f - (float)y/size;          // 0 at the root, 1 at the tip

    out->r = out->g = out->b = 255;
    out->a = 0;

    for (int i = 0; i < BLADES; i++)
    {
        float seed = Hash2(i, 7, 3);
        float baseX = (i + 0.5f)/BLADES + (seed - 0.5f)*0.10f;
        float lean = (seed - 0.5f)*0.42f;
        float height = 0.68f + seed*0.32f;
        if (t > height) continue;

        float along = t/height;
        float halfWidth = (0.055f + seed*0.022f)*(1.0f - along*0.92f);
        float centre = baseX + lean*along*along;

        if (fabsf(u - centre) < halfWidth)
        {
            // Slight shading variation between blades so the clump is not uniform.
            float shade = 0.80f + seed*0.20f + Fbm(u*20.0f, t*6.0f, 5, 2)*0.14f;
            out->r = ClampByte(255.0f*shade);
            out->g = ClampByte(255.0f*shade);
            out->b = ClampByte(255.0f*shade);
            out->a = 255;
            return;
        }
    }
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
// Three quads at 60 degrees to each other. A clump then reads as grass from any angle
// on the ground plane, which a single billboard never manages.
//------------------------------------------------------------------------------------
static Mesh MakeGrassBlade(void)
{
    const int QUADS = 3;
    Mesh m = { 0 };
    m.vertexCount = QUADS*4;
    m.triangleCount = QUADS*2;

    m.vertices  = (float *)MemAlloc(m.vertexCount*3*sizeof(float));
    m.texcoords = (float *)MemAlloc(m.vertexCount*2*sizeof(float));
    m.normals   = (float *)MemAlloc(m.vertexCount*3*sizeof(float));
    m.indices   = (unsigned short *)MemAlloc(m.triangleCount*3*sizeof(unsigned short));

    for (int q = 0; q < QUADS; q++)
    {
        float angle = (q/(float)QUADS)*PI;      // 0, 60, 120 degrees
        float cx = cosf(angle)*0.5f, cz = sinf(angle)*0.5f;

        // Normal points along the quad, but tilted upward: pure face normals make
        // foliage read as hard-edged cardboard under a directional light.
        float nx = -sinf(angle), nz = cosf(angle);
        Vector3 n = Vector3Normalize((Vector3){ nx*0.45f, 0.78f, nz*0.45f });

        int v = q*4;
        const float px[4] = { -cx,  cx,  cx, -cx };
        const float pz[4] = { -cz,  cz,  cz, -cz };
        const float py[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        const float uu[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
        const float vv[4] = { 1.0f, 1.0f, 0.0f, 0.0f };

        for (int i = 0; i < 4; i++)
        {
            m.vertices[(v + i)*3 + 0] = px[i];
            m.vertices[(v + i)*3 + 1] = py[i];
            m.vertices[(v + i)*3 + 2] = pz[i];
            m.texcoords[(v + i)*2 + 0] = uu[i];
            m.texcoords[(v + i)*2 + 1] = vv[i];
            m.normals[(v + i)*3 + 0] = n.x;
            m.normals[(v + i)*3 + 1] = n.y;
            m.normals[(v + i)*3 + 2] = n.z;
        }

        int t = q*6;
        m.indices[t + 0] = (unsigned short)(v + 0);
        m.indices[t + 1] = (unsigned short)(v + 1);
        m.indices[t + 2] = (unsigned short)(v + 2);
        m.indices[t + 3] = (unsigned short)(v + 0);
        m.indices[t + 4] = (unsigned short)(v + 2);
        m.indices[t + 5] = (unsigned short)(v + 3);
    }

    UploadMesh(&m, false);
    return m;
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

    a->locDither = GetShaderLocation(a->lighting, "dither");

    //--- Skinned character ----------------------------------------------------
    a->skinned = LoadShaderFromMemory(VS_SKINNED, FS_LIGHTING);
    a->skinnedOk = (a->skinned.id > 0) && (a->skinned.locs != NULL);

    if (a->skinnedOk)
    {
        a->skinned.locs[SHADER_LOC_MATRIX_MODEL]  = GetShaderLocation(a->skinned, "matModel");
        a->skinned.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(a->skinned, "matNormal");
        a->skinned.locs[SHADER_LOC_BONE_MATRICES] = GetShaderLocation(a->skinned, "boneMatrices");

        a->kViewPos    = GetShaderLocation(a->skinned, "viewPos");
        a->kSunDir     = GetShaderLocation(a->skinned, "sunDir");
        a->kSunColor   = GetShaderLocation(a->skinned, "sunColor");
        a->kAmbient    = GetShaderLocation(a->skinned, "ambientColor");
        a->kFogColor   = GetShaderLocation(a->skinned, "fogColor");
        a->kFogDensity = GetShaderLocation(a->skinned, "fogDensity");
        a->kUvScale    = GetShaderLocation(a->skinned, "uvScale");
        a->kEmissive   = GetShaderLocation(a->skinned, "emissive");
        a->kDither     = GetShaderLocation(a->skinned, "dither");
        a->kLightPos   = GetShaderLocation(a->skinned, "lightPos");
        a->kLightColor = GetShaderLocation(a->skinned, "lightColor");
        a->kLightCount = GetShaderLocation(a->skinned, "lightCount");

        Vector3 sunDir = Vector3Normalize((Vector3){ -0.45f, -1.0f, 0.35f });
        Vector3 sunColor = { 1.05f, 1.00f, 0.92f };
        Vector3 ambient = { 0.34f, 0.38f, 0.50f };
        Vector3 fogColor = { 0.086f, 0.102f, 0.149f };
        float fogDensity = 0.0075f;
        Vector2 uv = { 1.0f, 1.0f };
        float zero = 0.0f;

        SetShaderValue(a->skinned, a->kSunDir, &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->skinned, a->kSunColor, &sunColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->skinned, a->kAmbient, &ambient, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->skinned, a->kFogColor, &fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->skinned, a->kFogDensity, &fogDensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->skinned, a->kUvScale, &uv, SHADER_UNIFORM_VEC2);
        SetShaderValue(a->skinned, a->kEmissive, &zero, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->skinned, a->kDither, &zero, SHADER_UNIFORM_FLOAT);
    }

    a->character = LoadModel(CHARACTER_MODEL_PATH);
    a->characterOk = IsModelValid(a->character) && a->character.meshCount > 0;

    if (a->characterOk)
    {
        a->charAnims = LoadModelAnimations(CHARACTER_MODEL_PATH, &a->charAnimCount);

        // Resolve clips by name so a regenerated file with reordered tracks cannot
        // silently swap running for T-pose.
        a->clipIdle = 1; a->clipRunning = 2; a->clipWalking = 3;
        for (int i = 0; i < a->charAnimCount; i++)
        {
            if (strcmp(a->charAnims[i].name, "idle") == 0) a->clipIdle = i;
            else if (strcmp(a->charAnims[i].name, "running") == 0) a->clipRunning = i;
            else if (strcmp(a->charAnims[i].name, "walking") == 0) a->clipWalking = i;
        }

        // Normalise whatever the source units are, so swapping in another model does not
        // mean re-tuning every scale in the game.
        //
        // This must reproduce exactly what the vertex shader does. A rigged export can
        // store the mesh at an arbitrary scale and carry the real size in the bone
        // matrices - this one has 0.017-unit vertices and bone matrices that blow them
        // up by four orders of magnitude. Measuring raw vertices, or CPU-skinned ones,
        // both gave the wrong answer; only the GPU bone matrices agree with what is
        // actually drawn.
        float lo = 1e30f, hi = -1e30f;

        if (a->charAnims && a->charAnimCount > 0)
        {
            UpdateModelAnimationBones(a->character, a->charAnims[0], 0);

            for (int i = 0; i < a->character.meshCount; i++)
            {
                Mesh *mesh = &a->character.meshes[i];
                if (!mesh->vertices || !mesh->boneMatrices || !mesh->boneIds || !mesh->boneWeights)
                    continue;

                for (int k = 0; k < mesh->vertexCount; k++)
                {
                    Vector3 v = { mesh->vertices[k*3 + 0],
                                  mesh->vertices[k*3 + 1],
                                  mesh->vertices[k*3 + 2] };
                    float y = 0.0f;

                    for (int j = 0; j < 4; j++)
                    {
                        float wgt = mesh->boneWeights[k*4 + j];
                        if (wgt <= 0.0f) continue;

                        int bone = mesh->boneIds[k*4 + j];
                        if (bone < 0 || bone >= mesh->boneCount) continue;

                        Vector3 t = Vector3Transform(v, mesh->boneMatrices[bone]);
                        y += t.y*wgt;
                    }

                    if (y < lo) lo = y;
                    if (y > hi) hi = y;
                }
            }
        }

        if (hi <= lo)
        {
            BoundingBox bb = GetModelBoundingBox(a->character);
            lo = bb.min.y; hi = bb.max.y;
        }

        float height = hi - lo;
        a->charScale = (height > 0.000001f) ? (CHARACTER_TARGET_H/height) : 1.0f;
        a->charFootOffset = -lo*a->charScale;

        if (a->skinnedOk)
            for (int i = 0; i < a->character.materialCount; i++)
                a->character.materials[i].shader = a->skinned;

        TraceLog(LOG_INFO, "CHARACTER: %d verts, %d bones, %d clips, posed height %.2f, scale %.5f",
                 a->character.meshes[0].vertexCount, a->character.boneCount,
                 a->charAnimCount, height, a->charScale);
    }
    else TraceLog(LOG_WARNING, "CHARACTER: %s not loaded, falling back to primitives",
                  CHARACTER_MODEL_PATH);

    //--- Grass shader ---------------------------------------------------------
    a->grass = LoadShaderFromMemory(VS_GRASS, FS_GRASS);
    a->grassOk = (a->grass.id > 0) && (a->grass.locs != NULL);

    if (a->grassOk)
    {
        // raylib feeds DrawMeshInstanced's per-instance matrix through whatever
        // attribute SHADER_LOC_MATRIX_MODEL points at, so it must be redirected from
        // the usual matModel uniform to our instanceTransform attribute.
        a->grass.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(a->grass, "mvp");
        a->grass.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(a->grass, "matNormal");
        a->grass.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(a->grass, "instanceTransform");

        a->gTime          = GetShaderLocation(a->grass, "time");
        a->gHeight        = GetShaderLocation(a->grass, "grassHeight");
        a->gWindStrength  = GetShaderLocation(a->grass, "windStrength");
        a->gWindSpeed     = GetShaderLocation(a->grass, "windSpeed");
        a->gWindDir       = GetShaderLocation(a->grass, "windDir");
        a->gBendRadius    = GetShaderLocation(a->grass, "bendRadius");
        a->gBendStrength  = GetShaderLocation(a->grass, "bendStrength");
        a->gActorPos      = GetShaderLocation(a->grass, "actorPos");
        a->gActorVel      = GetShaderLocation(a->grass, "actorVel");
        a->gActorCount    = GetShaderLocation(a->grass, "actorCount");
        a->gViewPos       = GetShaderLocation(a->grass, "viewPos");
        a->gSunDir        = GetShaderLocation(a->grass, "sunDir");
        a->gSunColor      = GetShaderLocation(a->grass, "sunColor");
        a->gAmbient       = GetShaderLocation(a->grass, "ambientColor");
        a->gFogColor      = GetShaderLocation(a->grass, "fogColor");
        a->gFogDensity    = GetShaderLocation(a->grass, "fogDensity");
        a->gBaseColor     = GetShaderLocation(a->grass, "baseColor");
        a->gTipColor      = GetShaderLocation(a->grass, "tipColor");
        a->gLightPos      = GetShaderLocation(a->grass, "lightPos");
        a->gLightColor    = GetShaderLocation(a->grass, "lightColor");
        a->gLightCount    = GetShaderLocation(a->grass, "lightCount");

        Vector3 sunDir = Vector3Normalize((Vector3){ -0.45f, -1.0f, 0.35f });
        Vector3 sunColor = { 1.05f, 1.00f, 0.92f };
        Vector3 ambient = { 0.30f, 0.34f, 0.46f };
        Vector3 fogColor = { 0.086f, 0.102f, 0.149f };
        Vector3 baseColor = { 0.10f, 0.26f, 0.13f };     // shaded down at the roots
        Vector3 tipColor = { 0.44f, 0.80f, 0.34f };
        float fogDensity = 0.0075f;
        Vector2 windDir = { 0.82f, 0.57f };

        SetShaderValue(a->grass, a->gSunDir, &sunDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->grass, a->gSunColor, &sunColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->grass, a->gAmbient, &ambient, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->grass, a->gFogColor, &fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->grass, a->gFogDensity, &fogDensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->grass, a->gBaseColor, &baseColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->grass, a->gTipColor, &tipColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->grass, a->gWindDir, &windDir, SHADER_UNIFORM_VEC2);
    }

    //--- Meshes ---------------------------------------------------------------
    a->cube     = GenMeshCube(1.0f, 1.0f, 1.0f);
    a->sphere   = GenMeshSphere(1.0f, 14, 20);
    a->cylinder = GenMeshCylinder(1.0f, 1.0f, 18);
    a->plane    = GenMeshPlane(1.0f, 1.0f, 1, 1);
    a->grassBlade = MakeGrassBlade();

    //--- Textures -------------------------------------------------------------
    a->texFloor = MakeTexture(256, PxFloor, true);
    a->texWall  = MakeTexture(256, PxWall, true);
    a->texCrate = MakeTexture(256, PxCrate, true);
    a->texBush  = MakeTexture(128, PxBush, true);
    a->texMetal = MakeTexture(128, PxMetal, true);
    a->texCloth = MakeTexture(128, PxCloth, true);
    a->texFlat  = MakeTexture(4, PxFlat, false);
    a->texGlow  = MakeTexture(128, PxGlow, false);
    a->texGrass = MakeTexture(256, PxGrass, true);

    //--- Material -------------------------------------------------------------
    a->mat = LoadMaterialDefault();
    if (a->lightingOk) a->mat.shader = a->lighting;

    a->grassMat = LoadMaterialDefault();
    if (a->grassOk) a->grassMat.shader = a->grass;
    a->grassMat.maps[MATERIAL_MAP_DIFFUSE].texture = a->texGrass;
    a->grassMat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;

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
    UnloadMesh(a->grassBlade);

    UnloadTexture(a->texFloor);
    UnloadTexture(a->texWall);
    UnloadTexture(a->texCrate);
    UnloadTexture(a->texBush);
    UnloadTexture(a->texMetal);
    UnloadTexture(a->texCloth);
    UnloadTexture(a->texFlat);
    UnloadTexture(a->texGlow);
    UnloadTexture(a->texGrass);

    UnloadRenderTexture(a->sceneTarget);

    // The material borrows the shader, so drop its reference before unloading it.
    a->mat.shader = (Shader){ 0 };
    UnloadMaterial(a->mat);
    a->grassMat.shader = (Shader){ 0 };
    a->grassMat.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };
    UnloadMaterial(a->grassMat);

    if (a->lightingOk) UnloadShader(a->lighting);
    if (a->postOk) UnloadShader(a->post);
    if (a->grassOk) UnloadShader(a->grass);

    if (a->characterOk)
    {
        if (a->charAnims) UnloadModelAnimations(a->charAnims, a->charAnimCount);
        for (int i = 0; i < a->character.materialCount; i++)
            a->character.materials[i].shader = (Shader){ 0 };
        UnloadModel(a->character);
    }
    if (a->skinnedOk) UnloadShader(a->skinned);
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

void AssetsDrawCharacter(Assets *a, Vector3 position, float yaw, float scaleMul,
                         int animIndex, float frame, Color tint, float dither, float emissive,
                         const Vector3 *lightPos, const Vector3 *lightColor, int lightCount,
                         Vector3 viewPos)
{
    if (!a->characterOk) return;

    if (a->skinnedOk)
    {
        SetShaderValue(a->skinned, a->kViewPos, &viewPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(a->skinned, a->kDither, &dither, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->skinned, a->kEmissive, &emissive, SHADER_UNIFORM_FLOAT);
        if (lightCount > MAX_SHADER_LIGHTS) lightCount = MAX_SHADER_LIGHTS;
        if (lightCount > 0)
        {
            SetShaderValueV(a->skinned, a->kLightPos, lightPos, SHADER_UNIFORM_VEC3, lightCount);
            SetShaderValueV(a->skinned, a->kLightColor, lightColor, SHADER_UNIFORM_VEC3, lightCount);
        }
        SetShaderValue(a->skinned, a->kLightCount, &lightCount, SHADER_UNIFORM_INT);
    }

    // Posing writes into the model's shared bone matrices, so it has to happen
    // immediately before this draw rather than once per frame.
    if (a->charAnims && a->charAnimCount > 0)
    {
        int idx = (animIndex < 0 || animIndex >= a->charAnimCount) ? 0 : animIndex;
        ModelAnimation anim = a->charAnims[idx];
        if (anim.frameCount > 0)
        {
            int f = (int)frame % anim.frameCount;
            if (f < 0) f += anim.frameCount;
            UpdateModelAnimationBones(a->character, anim, f);
        }
    }

    float s = a->charScale*scaleMul;
    Matrix m = MatrixScale(s, s, s);
    m = MatrixMultiply(m, MatrixRotateY(yaw));
    m = MatrixMultiply(m, MatrixTranslate(position.x,
                                          position.y + a->charFootOffset*scaleMul,
                                          position.z));

    for (int i = 0; i < a->character.meshCount; i++)
    {
        Material mat = a->character.materials[a->character.meshMaterial[i]];
        mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        DrawMesh(a->character.meshes[i], mat, m);
    }
}

void AssetsSetDither(Assets *a, float amount)
{
    if (!a->lightingOk) return;
    SetShaderValue(a->lighting, a->locDither, &amount, SHADER_UNIFORM_FLOAT);
}

void AssetsGrassFrame(Assets *a, const Tuning *t, float time, Vector3 viewPos,
                      const Vector3 *actorPos, const Vector2 *actorVel, int actorCount,
                      const Vector3 *lightPos, const Vector3 *lightColor, int lightCount)
{
    if (!a->grassOk) return;

    if (actorCount > MAX_SHADER_LIGHTS) actorCount = MAX_SHADER_LIGHTS;
    if (lightCount > MAX_SHADER_LIGHTS) lightCount = MAX_SHADER_LIGHTS;

    SetShaderValue(a->grass, a->gTime, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->grass, a->gHeight, &t->grassHeight, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->grass, a->gWindStrength, &t->windStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->grass, a->gWindSpeed, &t->windSpeed, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->grass, a->gBendRadius, &t->grassBendRadius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->grass, a->gBendStrength, &t->grassBendStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->grass, a->gViewPos, &viewPos, SHADER_UNIFORM_VEC3);

    if (actorCount > 0)
    {
        SetShaderValueV(a->grass, a->gActorPos, actorPos, SHADER_UNIFORM_VEC3, actorCount);
        SetShaderValueV(a->grass, a->gActorVel, actorVel, SHADER_UNIFORM_VEC2, actorCount);
    }
    SetShaderValue(a->grass, a->gActorCount, &actorCount, SHADER_UNIFORM_INT);

    if (lightCount > 0)
    {
        SetShaderValueV(a->grass, a->gLightPos, lightPos, SHADER_UNIFORM_VEC3, lightCount);
        SetShaderValueV(a->grass, a->gLightColor, lightColor, SHADER_UNIFORM_VEC3, lightCount);
    }
    SetShaderValue(a->grass, a->gLightCount, &lightCount, SHADER_UNIFORM_INT);
}
