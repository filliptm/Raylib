/*******************************************************************************************
*   ASSETS
*
*   Procedural texture generation, unit meshes, the scene/post shaders, optional
*   per-kit rigged GLBs, and the static Kenney station models. Imported characters and
*   environment pieces fall back independently, so a missing file cannot stop startup.
********************************************************************************************/
#include "assets.h"
#include "generated_assets.h"
#include "vfx_catalog.h"

#include "rlgl.h"
#include "raymath.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// The arena and menu cameras never need raylib's 0.01..1000 default range. Moving the
// near plane out is the important part: it concentrates depth precision where the
// station's closely layered deck, panels, decals, and characters actually live.
static const float SCENE_CLIP_NEAR = 0.5f;
static const float SCENE_CLIP_FAR = 120.0f;

#define CHARACTER_RUNTIME_ROOT "build/assets/characters/"

static const char *CHARACTER_MODEL_PATHS[CLASS_COUNT] = {
    [CLASS_SHOTGUNNER] = CHARACTER_RUNTIME_ROOT "sentinel.glb",
    [CLASS_SNIPER] = CHARACTER_RUNTIME_ROOT "longshot.glb",
    [CLASS_BRUISER] = CHARACTER_RUNTIME_ROOT "ironclad_guardian.glb",
    [CLASS_HEALER] = CHARACTER_RUNTIME_ROOT "gaia_guardian.glb"
};

#define STATION_ROOT "resources/environment/kenney_space_station/models/"

static const char *STATION_MODEL_PATHS[STATION_MODEL_COUNT] = {
    [STATION_FLOOR_PANEL] = STATION_ROOT "floor-panel.glb",
    [STATION_FLOOR_DETAIL] = STATION_ROOT "floor-detail.glb",
    [STATION_STRUCTURE_PANEL] = STATION_ROOT "structure-panel.glb",
    [STATION_STRUCTURE] = STATION_ROOT "structure.glb",
    [STATION_STRUCTURE_BARRIER] = STATION_ROOT "structure-barrier.glb",
    [STATION_WALL] = STATION_ROOT "wall.glb",
    [STATION_WALL_CORNER] = STATION_ROOT "wall-corner.glb",
    [STATION_WALL_PILLAR] = STATION_ROOT "wall-pillar.glb",
    [STATION_WALL_WINDOW] = STATION_ROOT "wall-window.glb",
    [STATION_WALL_BANNER] = STATION_ROOT "wall-banner.glb",
    [STATION_DOOR_DOUBLE_CLOSED] = STATION_ROOT "door-double-closed.glb",
    [STATION_CONTAINER] = STATION_ROOT "container.glb",
    [STATION_CONTAINER_WIDE] = STATION_ROOT "container-wide.glb",
    [STATION_CONTAINER_TALL] = STATION_ROOT "container-tall.glb",
    [STATION_COMPUTER_SYSTEM] = STATION_ROOT "computer-system.glb",
    [STATION_COMPUTER_WIDE] = STATION_ROOT "computer-wide.glb",
    [STATION_DISPLAY_WALL] = STATION_ROOT "display-wall.glb",
    [STATION_PIPE] = STATION_ROOT "pipe.glb",
    [STATION_PIPE_BEND] = STATION_ROOT "pipe-bend.glb",
    [STATION_RAIL] = STATION_ROOT "rail.glb",
    [STATION_TABLE_DISPLAY_PLANET] = STATION_ROOT "table-display-planet.glb",
    [STATION_SKIP] = STATION_ROOT "skip.glb"
};

static void LoadVfxAtlases(Assets *assets)
{
    char catalogMessage[128];
    if (!VfxCatalogValidate(catalogMessage, sizeof(catalogMessage)))
    {
        TraceLog(LOG_WARNING, "VFX: catalog invalid: %s", catalogMessage);
        return;
    }

    int loaded = 0;
    for (int atlas = 0; atlas < VFX_ATLAS_COUNT; atlas++)
    {
        const char *path = VfxAtlasPath((VfxAtlasId)atlas);
        if (!path || !FileExists(path))
        {
            TraceLog(LOG_WARNING, "VFX: optional atlas missing: %s",
                     path ? path : "(unknown)");
            continue;
        }

        Texture2D texture = LoadTexture(path);
        if (texture.id == 0)
        {
            TraceLog(LOG_WARNING, "VFX: atlas failed to load: %s", path);
            continue;
        }
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
        assets->vfxAtlases[atlas] = texture;
        assets->vfxAtlasesOk[atlas] = true;
        loaded++;
    }
    TraceLog(LOG_INFO, "VFX: loaded %d/%d runtime atlases", loaded, VFX_ATLAS_COUNT);
}

void AssetsReloadVfxAtlases(Assets *assets)
{
    for (int atlas = 0; atlas < VFX_ATLAS_COUNT; atlas++)
    {
        if (assets->vfxAtlasesOk[atlas] && assets->vfxAtlases[atlas].id > 0)
            UnloadTexture(assets->vfxAtlases[atlas]);
        assets->vfxAtlases[atlas] = (Texture2D){ 0 };
        assets->vfxAtlasesOk[atlas] = false;
    }
    LoadVfxAtlases(assets);
}

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
"uniform float toonMode;\n"
"uniform float toonBands;\n"
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
"    if (toonMode > 0.5) wrapped = (floor(wrapped*toonBands) + 0.5)/toonBands;\n"
"    vec3 light = ambientColor*(1.0 + 0.5*toonMode) + sunColor*wrapped;\n"
"    vec3 H = normalize(L + V);\n"
"    float spec = pow(max(dot(N, H), 0.0), 42.0)*0.22*ndl*(1.0 - toonMode);\n"
"    for (int i = 0; i < lightCount; i++)\n"
"    {\n"
"        vec3 delta = lightPos[i] - fragPosition;\n"
"        float dist = length(delta);\n"
"        float att = 1.0/(1.0 + 0.28*dist + 0.12*dist*dist);\n"
"        float pdl = max(dot(N, normalize(delta)), 0.0)*0.65 + 0.35;\n"
"        light += lightColor[i]*att*pdl;\n"
"    }\n"
"    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0)*0.30*(1.0 - toonMode);\n"
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
"uniform sampler2D depthTex;\n"
"uniform vec2 sourceResolution;\n"
"uniform vec2 outputResolution;\n"
"uniform float bloomStrength;\n"
"uniform float vignetteStrength;\n"
"uniform float outlineStrength;\n"
"uniform float clipNear;\n"
"uniform float clipFar;\n"
"uniform float stylePixelate;\n"
"uniform float stylePainterly;\n"
"uniform float styleHalftone;\n"
"uniform float stylePosterize;\n"
"uniform float styleGrain;\n"
"uniform float styleCA;\n"
"uniform float styleSaturation;\n"
"uniform float styleBrightness;\n"
"uniform float styleExposure;\n"
"uniform float styleTonemap;\n"
"out vec4 finalColor;\n"
"float linDepth(vec2 uv)\n"
"{\n"
"    float z = texture(depthTex, uv).r*2.0 - 1.0;\n"
"    return (2.0*clipNear*clipFar)/(clipFar + clipNear - z*(clipFar - clipNear));\n"
"}\n"
"float sceneLuma(vec3 color)\n"
"{\n"
"    return dot(color, vec3(0.299, 0.587, 0.114));\n"
"}\n"
"vec3 sampleSceneAA(vec2 uv)\n"
"{\n"
"    vec2 px = 1.0/sourceResolution;\n"
"    vec3 rgbNW = texture(texture0, uv + vec2(-1.0, -1.0)*px).rgb;\n"
"    vec3 rgbNE = texture(texture0, uv + vec2( 1.0, -1.0)*px).rgb;\n"
"    vec3 rgbSW = texture(texture0, uv + vec2(-1.0,  1.0)*px).rgb;\n"
"    vec3 rgbSE = texture(texture0, uv + vec2( 1.0,  1.0)*px).rgb;\n"
"    vec3 rgbM  = texture(texture0, uv).rgb;\n"
"    float lumaNW = sceneLuma(rgbNW);\n"
"    float lumaNE = sceneLuma(rgbNE);\n"
"    float lumaSW = sceneLuma(rgbSW);\n"
"    float lumaSE = sceneLuma(rgbSE);\n"
"    float lumaM  = sceneLuma(rgbM);\n"
"    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));\n"
"    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));\n"
"    vec2 dir = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)),\n"
"                     ((lumaNW + lumaSW) - (lumaNE + lumaSE)));\n"
"    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE)*0.03125, 0.0078125);\n"
"    float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);\n"
"    dir = clamp(dir*rcpDirMin, vec2(-8.0), vec2(8.0))*px;\n"
"    vec3 rgbA = 0.5*(texture(texture0, uv + dir*(1.0/3.0 - 0.5)).rgb +\n"
"                     texture(texture0, uv + dir*(2.0/3.0 - 0.5)).rgb);\n"
"    vec3 rgbB = rgbA*0.5 + 0.25*(texture(texture0, uv + dir*-0.5).rgb +\n"
"                                  texture(texture0, uv + dir* 0.5).rgb);\n"
"    float lumaB = sceneLuma(rgbB);\n"
"    return (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;\n"
"}\n"
"vec3 sampleBright(vec2 uv)\n"
"{\n"
"    vec3 c = texture(texture0, uv).rgb;\n"
"    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
"    return c*smoothstep(0.45, 0.95, lum);\n"
"}\n"
"vec3 kuwahara(vec2 uv)\n"                                 // painterly: pick the flattest quadrant
"{\n"
"    vec2 px = 1.0/outputResolution;\n"
"    vec3 bestMean = vec3(0.0);\n"
"    float bestVar = 1e9;\n"
"    for (int q = 0; q < 4; q++)\n"
"    {\n"
"        vec2 dir = vec2((q == 0 || q == 3) ? -1.0 : 1.0, (q < 2) ? -1.0 : 1.0);\n"
"        vec3 sum = vec3(0.0), sq = vec3(0.0);\n"
"        for (int i = 0; i <= 3; i++)\n"
"        for (int j = 0; j <= 3; j++)\n"
"        {\n"
"            vec3 c = texture(texture0, uv + px*dir*vec2(float(i), float(j))).rgb;\n"
"            sum += c; sq += c*c;\n"
"        }\n"
"        vec3 mean = sum/16.0;\n"
"        vec3 vr = sq/16.0 - mean*mean;\n"
"        float v = vr.r + vr.g + vr.b;\n"
"        if (v < bestVar) { bestVar = v; bestMean = mean; }\n"
"    }\n"
"    return bestMean;\n"
"}\n"
"void main()\n"
"{\n"
"    vec2 uv = fragTexCoord;\n"
"\n"
"    if (stylePixelate > 0.003)\n"                          // chunky retro blocks
"    {\n"
"        float block = 1.0 + stylePixelate*11.0;\n"
"        uv = (floor(uv*outputResolution/block) + 0.5)*block/outputResolution;\n"
"    }\n"
"\n"
"    vec3 base;\n"
"    if (styleCA > 0.003)\n"                                // lens fringe, radial from centre
"    {\n"
"        vec2 off = (uv - 0.5)*styleCA*0.012;\n"
"        vec3 plus = sampleSceneAA(uv + off);\n"
"        vec3 middle = sampleSceneAA(uv);\n"
"        vec3 minus = sampleSceneAA(uv - off);\n"
"        base = vec3(plus.r, middle.g, minus.b);\n"
"    }\n"
"    else base = sampleSceneAA(uv);\n"
"\n"
"    if (stylePainterly > 0.003) base = mix(base, kuwahara(uv), stylePainterly);\n"
"    vec3 color = base;\n"
"\n"
"    if (bloomStrength > 0.001)\n"
"    {\n"
"        vec2 px = 1.0/outputResolution;\n"
"        vec3 bloom = vec3(0.0);\n"
"        for (int i = 0; i < 12; i++)\n"
"        {\n"
"            float a = float(i)*0.5235988;\n"
"            vec2 dir = vec2(cos(a), sin(a));\n"
"            bloom += sampleBright(uv + dir*px*3.0);\n"
"            bloom += sampleBright(uv + dir*px*7.0);\n"
"            bloom += sampleBright(uv + dir*px*13.0)*0.6;\n"
"        }\n"
"        color += (bloom/28.8)*bloomStrength;\n"
"    }\n"
"\n"
"    color *= styleExposure;\n"
"    if (styleTonemap > 0.003)\n"                            // ACES-style filmic roll-off
"    {\n"
"        vec3 mapped = (color*(2.51*color + 0.03))/(color*(2.43*color + 0.59) + 0.14);\n"
"        color = mix(color, clamp(mapped, 0.0, 1.0), styleTonemap);\n"
"    }\n"
"\n"
"    if (styleHalftone > 0.003)\n"                          // comic shading dots in the darks
"    {\n"
"        float lum = dot(color, vec3(0.299, 0.587, 0.114));\n"
"        vec2 p = mat2(0.707, -0.707, 0.707, 0.707)*(fragTexCoord*outputResolution/7.0);\n"
"        float d = length(fract(p) - 0.5);\n"
"        float radius = sqrt(clamp(1.0 - lum, 0.0, 1.0))*0.55;\n"
"        float dot_ = smoothstep(radius, radius - 0.14, d);\n"
"        float amt = dot_*(1.0 - smoothstep(0.55, 0.95, lum));\n"
"        color = mix(color, color*0.22, amt*styleHalftone);\n"
"    }\n"
"\n"
"    if (stylePosterize > 0.003)\n"                         // crush to flat paint steps
"        color = mix(color, floor(color*5.0 + 0.5)/5.0, stylePosterize);\n"
"\n"
"    if (outlineStrength > 0.001)\n"                        // ink lines from depth edges
"    {\n"
"        vec2 px = 2.2/outputResolution;\n"
"        float dc = linDepth(fragTexCoord);\n"
"        float d = 0.0;\n"
"        d = max(d, abs(linDepth(fragTexCoord + vec2(px.x, 0.0)) - dc));\n"
"        d = max(d, abs(linDepth(fragTexCoord - vec2(px.x, 0.0)) - dc));\n"
"        d = max(d, abs(linDepth(fragTexCoord + vec2(0.0, px.y)) - dc));\n"
"        d = max(d, abs(linDepth(fragTexCoord - vec2(0.0, px.y)) - dc));\n"
"        float t = 0.20 + dc*0.03;\n"
"        float edge = smoothstep(t, t*2.35, d);\n"
"        color = mix(color, vec3(0.02, 0.02, 0.05), edge*outlineStrength);\n"
"    }\n"
"\n"
"    float lum2 = dot(color, vec3(0.299, 0.587, 0.114));\n"
"    color = mix(vec3(lum2), color, styleSaturation);\n"
"    color *= styleBrightness;\n"
"    color = mix(color, color*color*(3.0 - 2.0*color), 0.18);\n"   // soft S-curve contrast
"\n"
"    if (styleGrain > 0.003)\n"                             // temporally stable film grain
"    {\n"
"        float n = fract(sin(dot(floor(fragTexCoord*outputResolution),\n"
"                                vec2(12.9898, 78.233)))*43758.5453);\n"
"        color += (n - 0.5)*0.16*styleGrain;\n"
"    }\n"
"\n"
"    vec2 centred = fragTexCoord - 0.5;\n"
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
"uniform float toonMode;\n"
"uniform float toonBands;\n"
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
"    if (toonMode > 0.5) ndl = (floor(ndl*toonBands) + 0.5)/toonBands;\n"
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

static int FindCharacterClip(const RiggedCharacter *character, const char *name)
{
    for (int i = 0; i < character->animCount; i++)
        if (strcmp(character->anims[i].name, name) == 0) return i;
    return -1;
}

static int FindCharacterBone(const RiggedCharacter *character, const char *name)
{
    for (int i = 0; i < character->model.boneCount; i++)
        if (strcmp(character->model.bones[i].name, name) == 0) return i;
    return -1;
}

// CPU-skins every vertex at one frame of one clip and returns the vertical span.
// Meshy models encode most of their apparent size in bone matrices, so raw mesh
// bounds are useless for sizing; this reproduces the skinned vertex shader.
static bool MeasurePosedSpanY(RiggedCharacter *character, int clip, int frame,
                              float *outLo, float *outHi)
{
    if (clip < 0 || clip >= character->animCount) return false;
    ModelAnimation anim = character->anims[clip];
    if (anim.frameCount <= 0) return false;
    if (frame < 0) frame = 0;
    if (frame >= anim.frameCount) frame = anim.frameCount - 1;
    UpdateModelAnimationBones(character->model, anim, frame);

    float lo = 1e30f, hi = -1e30f;
    for (int i = 0; i < character->model.meshCount; i++)
    {
        Mesh *mesh = &character->model.meshes[i];
        if (!mesh->vertices || !mesh->boneMatrices ||
            !mesh->boneIds || !mesh->boneWeights) continue;

        for (int k = 0; k < mesh->vertexCount; k++)
        {
            Vector3 v = { mesh->vertices[k*3], mesh->vertices[k*3 + 1],
                          mesh->vertices[k*3 + 2] };
            float y = 0.0f;
            for (int j = 0; j < 4; j++)
            {
                float weight = mesh->boneWeights[k*4 + j];
                int bone = mesh->boneIds[k*4 + j];
                if (weight <= 0.0f || bone < 0 || bone >= mesh->boneCount) continue;
                y += Vector3Transform(v, mesh->boneMatrices[bone]).y*weight;
            }
            if (y < lo) lo = y;
            if (y > hi) hi = y;
        }
    }
    if (hi <= lo) return false;
    *outLo = lo;
    *outHi = hi;
    return true;
}

// Zero is a valid clip index, so every optional clip must read "absent" as -1 even
// on the failure paths where the struct is cleared.
static void ResetOptionalClips(RiggedCharacter *character)
{
    character->clipDeath = -1;
    character->clipActionMain = -1;
    character->clipActionSuper = -1;
    character->clipActionCast = -1;
    character->clipActionMobility = -1;
    character->clipActionGuard = -1;
    character->clipActionGrapple = -1;
    character->clipActionMineDeploy = -1;
}

static void LoadRiggedCharacter(Assets *a, RiggedCharacter *character,
                                const char *path, const char *label)
{
    ResetOptionalClips(character);
    character->model = LoadModel(path);
    character->ok = IsModelValid(character->model) && character->model.meshCount > 0;

    if (!character->ok)
    {
        TraceLog(LOG_WARNING, "CHARACTER %s: %s not loaded, falling back to primitives",
                 label, path);
        return;
    }

    character->anims = LoadModelAnimations(path, &character->animCount);
    bool animationsValid = character->anims && character->animCount > 0;
    for (int i = 0; i < character->animCount; i++)
    {
        if (!IsModelAnimationValid(character->model, character->anims[i]))
        {
            TraceLog(LOG_WARNING, "CHARACTER %s: clip %s does not match the model skeleton",
                     label, character->anims[i].name);
            animationsValid = false;
        }
    }
    if (!animationsValid)
    {
        if (character->anims)
            UnloadModelAnimations(character->anims, character->animCount);
        UnloadModel(character->model);
        *character = (RiggedCharacter){ 0 };
        ResetOptionalClips(character);
        TraceLog(LOG_WARNING,
                 "CHARACTER %s: generated animations invalid, falling back to primitives",
                 label);
        return;
    }

    // Generated assets use stable semantic names. Optional clips retain compatible
    // fallbacks so an intentionally smaller future library can still render safely.
    character->clipIdle = FindCharacterClip(character, "idle");
    character->clipCombat = FindCharacterClip(character, "combat_stance");
    character->clipWalk = FindCharacterClip(character, "walk_forward");
    character->clipRunF = FindCharacterClip(character, "run_forward");
    character->clipRunB = FindCharacterClip(character, "run_backward");
    character->clipRunFL = FindCharacterClip(character, "run_forward_left");
    character->clipRunFR = FindCharacterClip(character, "run_forward_right");
    character->clipRunBL = FindCharacterClip(character, "run_back_left");
    character->clipRunBR = FindCharacterClip(character, "run_back_right");
    character->clipDeath = FindCharacterClip(character, "death");
    character->clipActionMain = FindCharacterClip(character, "attack_main");
    if (character->clipActionMain < 0)
        character->clipActionMain = FindCharacterClip(character, "shoot");
    character->clipActionSuper = FindCharacterClip(character, "attack_super");
    character->clipActionCast = FindCharacterClip(character, "cast");
    character->clipActionMobility = FindCharacterClip(character, "mobility");
    character->clipActionGuard = FindCharacterClip(character, "guard");
    character->clipActionGrapple = FindCharacterClip(character, "grapple");
    character->clipActionMineDeploy = FindCharacterClip(character, "mine_deploy");

    // Meshy's rig chains Hips -> Spine02 -> Spine01 -> Spine, with "Spine" as the
    // CHEST joint parenting both shoulders and the neck. The whole-torso pivot is
    // therefore Spine02 and the chest pivot is Spine, counter to what the names
    // suggest. Getting these backwards swings the entire upper body on every shot
    // recoil and anchors chest VFX at the belly.
    character->boneHips = FindCharacterBone(character, "Hips");
    character->boneSpine = FindCharacterBone(character, "Spine02");
    character->boneChest = FindCharacterBone(character, "Spine");
    character->boneHead = FindCharacterBone(character, "Head");
    character->boneLeftShoulder = FindCharacterBone(character, "LeftShoulder");
    character->boneLeftArm = FindCharacterBone(character, "LeftArm");
    character->boneLeftForeArm = FindCharacterBone(character, "LeftForeArm");
    character->boneLeftHand = FindCharacterBone(character, "LeftHand");
    character->boneRightShoulder = FindCharacterBone(character, "RightShoulder");
    character->boneRightArm = FindCharacterBone(character, "RightArm");
    character->boneRightForeArm = FindCharacterBone(character, "RightForeArm");
    character->boneRightHand = FindCharacterBone(character, "RightHand");
    character->boneLeftUpLeg = FindCharacterBone(character, "LeftUpLeg");
    character->boneLeftLeg = FindCharacterBone(character, "LeftLeg");
    character->boneRightUpLeg = FindCharacterBone(character, "RightUpLeg");
    character->boneRightLeg = FindCharacterBone(character, "RightLeg");
    character->boneLeftFoot = FindCharacterBone(character, "LeftFoot");
    character->boneRightFoot = FindCharacterBone(character, "RightFoot");

    character->clipIdle = character->clipIdle < 0 ? 0 : character->clipIdle;
    character->clipRunF = character->clipRunF < 0 ? character->clipIdle : character->clipRunF;
    character->clipWalk = character->clipWalk < 0 ? character->clipRunF : character->clipWalk;
    character->clipCombat = character->clipCombat < 0 ? character->clipIdle : character->clipCombat;
    character->clipRunB = character->clipRunB < 0 ? character->clipRunF : character->clipRunB;
    character->clipRunFL = character->clipRunFL < 0 ? character->clipRunF : character->clipRunFL;
    character->clipRunFR = character->clipRunFR < 0 ? character->clipRunF : character->clipRunFR;
    character->clipRunBL = character->clipRunBL < 0 ? character->clipRunB : character->clipRunBL;
    character->clipRunBR = character->clipRunBR < 0 ? character->clipRunB : character->clipRunBR;
    // A missing death clip holds the idle pose instead of silently remapping the
    // selector's -1 to whatever clip sits at index zero.
    character->clipDeath = character->clipDeath < 0 ? character->clipIdle : character->clipDeath;

    // Height and foot line are measured across several locomotion poses, not idle
    // alone. Some libraries author a crouched idle while overrides stand upright;
    // measuring only idle then gives equivalently sized rigs visibly different world
    // scales, and feet sink below the idle-derived floor as soon as a run clip
    // drops them lower. Height takes the tallest sampled pose; the ground line
    // takes the lowest sampled sole.
    float lo = 1e30f;
    float height = 0.0f;
    if (character->anims && character->animCount > 0)
    {
        int clips[3] = { character->clipIdle, character->clipWalk,
                         character->clipRunF };
        for (int c = 0; c < 3; c++)
        {
            bool duplicate = false;
            for (int p = 0; p < c; p++)
                if (clips[p] == clips[c]) duplicate = true;
            if (duplicate || clips[c] < 0 || clips[c] >= character->animCount)
                continue;

            int frameCount = character->anims[clips[c]].frameCount;
            for (int s = 0; s < 4; s++)
            {
                float sampleLo, sampleHi;
                if (!MeasurePosedSpanY(character, clips[c], (frameCount*s)/4,
                                       &sampleLo, &sampleHi))
                    continue;
                if (sampleLo < lo) lo = sampleLo;
                if (sampleHi - sampleLo > height) height = sampleHi - sampleLo;
            }
        }
    }

    if (height <= 0.0f)
    {
        BoundingBox bb = GetModelBoundingBox(character->model);
        lo = bb.min.y;
        height = bb.max.y - bb.min.y;
    }

    character->scale = height > 0.000001f ? CHARACTER_TARGET_H/height : 1.0f;
    character->footOffset = -lo*character->scale;

    if (a->skinnedOk)
        for (int i = 0; i < character->model.materialCount; i++)
            character->model.materials[i].shader = a->skinned;

    int vertexCount = 0;
    for (int i = 0; i < character->model.meshCount; i++)
        vertexCount += character->model.meshes[i].vertexCount;
    TraceLog(LOG_INFO,
             "CHARACTER %s: %d verts, %d bones, %d clips, posed height %.2f, scale %.5f",
             label, vertexCount, character->model.boneCount,
             character->animCount, height, character->scale);
}

static void ApplyStationTextureFiltering(Texture2D *texture)
{
    if (!texture || texture->id == 0)
        return;

    // Mipmaps stabilize minification from the match camera. Anisotropy is applied
    // after trilinear filtering because raylib configures it as a separate texture
    // parameter; it preserves the side-wall atlas at shallow viewing angles.
    GenTextureMipmaps(texture);
    SetTextureFilter(*texture, TEXTURE_FILTER_TRILINEAR);
    SetTextureFilter(*texture, TEXTURE_FILTER_ANISOTROPIC_8X);
}

static void LoadStationAssets(Assets *a)
{
    a->texStationOrange = LoadTexture(STATION_ROOT "Textures/colormap.png");
    a->texStationPurple = LoadTexture(STATION_ROOT "Textures/variation-a.png");
    a->stationTexturesOk = a->texStationOrange.id > 0 && a->texStationPurple.id > 0;

    ApplyStationTextureFiltering(&a->texStationOrange);
    ApplyStationTextureFiltering(&a->texStationPurple);

    int loaded = 0;
    for (int id = 0; id < STATION_MODEL_COUNT; id++)
    {
        StationModel *station = &a->station[id];
        const char *path = STATION_MODEL_PATHS[id];
        if (!path) continue;

        station->model = LoadModel(path);
        station->ok = IsModelValid(station->model) && station->model.meshCount > 0;
        if (station->ok) loaded++;
        else
            TraceLog(LOG_WARNING, "STATION: %s not loaded; using procedural fallback", path);
    }

    TraceLog(LOG_INFO, "STATION: loaded %d/%d models, orange atlas=%s, purple atlas=%s",
             loaded, STATION_MODEL_COUNT,
             a->texStationOrange.id > 0 ? "ready" : "missing",
             a->texStationPurple.id > 0 ? "ready" : "missing");
}

//------------------------------------------------------------------------------------
static int ScaledSceneDimension(int outputSize, float renderScale)
{
    if (renderScale < 1.0f) renderScale = 1.0f;
    if (renderScale > 2.0f) renderScale = 2.0f;
    int scaled = (int)ceilf((float)outputSize*renderScale);
    return scaled > outputSize ? scaled : outputSize;
}

static void UpdateSceneResolutionUniforms(Assets *a)
{
    if (!a->postOk || a->sceneTarget.texture.id == 0) return;

    Vector2 source = {
        (float)a->sceneTarget.texture.width,
        (float)a->sceneTarget.texture.height
    };
    Vector2 output = {
        (float)a->sceneOutputWidth,
        (float)a->sceneOutputHeight
    };
    SetShaderValue(a->post, a->locSourceResolution, &source, SHADER_UNIFORM_VEC2);
    SetShaderValue(a->post, a->locOutputResolution, &output, SHADER_UNIFORM_VEC2);
}

static void ReleaseSceneTarget(Assets *a)
{
    if (a->sceneTarget.id != 0 && a->depthOk)
    {
        rlUnloadFramebuffer(a->sceneTarget.id);
        rlUnloadTexture(a->sceneTarget.texture.id);
        rlUnloadTexture(a->sceneTarget.depth.id);
    }
    else if (a->sceneTarget.id != 0) UnloadRenderTexture(a->sceneTarget);
    a->sceneTarget = (RenderTexture2D){ 0 };
    a->depthOk = false;
    a->sceneOutputWidth = 0;
    a->sceneOutputHeight = 0;
    a->sceneRenderScale = 0.0f;
}

static bool CreateSceneTarget(Assets *a, int outputW, int outputH, float renderScale)
{
    a->sceneTarget = (RenderTexture2D){ 0 };
    a->depthOk = false;
    if (outputW < 1 || outputH < 1) return false;

    if (renderScale < 1.0f) renderScale = 1.0f;
    if (renderScale > 2.0f) renderScale = 2.0f;
    int targetW = ScaledSceneDimension(outputW, renderScale);
    int targetH = ScaledSceneDimension(outputH, renderScale);

    // LoadRenderTexture uses a depth renderbuffer. The outline pass samples depth, so
    // prefer a hand-built target with a depth texture and keep a direct fallback.
    a->sceneTarget.id = rlLoadFramebuffer();
    if (a->sceneTarget.id > 0)
    {
        rlEnableFramebuffer(a->sceneTarget.id);
        a->sceneTarget.texture.id = rlLoadTexture(NULL, targetW, targetH,
                                                  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        a->sceneTarget.texture.width = targetW;
        a->sceneTarget.texture.height = targetH;
        a->sceneTarget.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        a->sceneTarget.texture.mipmaps = 1;
        a->sceneTarget.depth.id = rlLoadTextureDepth(targetW, targetH, false);
        a->sceneTarget.depth.width = targetW;
        a->sceneTarget.depth.height = targetH;
        a->sceneTarget.depth.mipmaps = 1;
        rlFramebufferAttach(a->sceneTarget.id, a->sceneTarget.texture.id,
                            RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(a->sceneTarget.id, a->sceneTarget.depth.id,
                            RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
        a->depthOk = rlFramebufferComplete(a->sceneTarget.id);
        rlDisableFramebuffer();
    }

    if (!a->depthOk)
    {
        if (a->sceneTarget.id > 0) rlUnloadFramebuffer(a->sceneTarget.id);
        if (a->sceneTarget.texture.id > 0) rlUnloadTexture(a->sceneTarget.texture.id);
        if (a->sceneTarget.depth.id > 0) rlUnloadTexture(a->sceneTarget.depth.id);
        a->sceneTarget = LoadRenderTexture(targetW, targetH);
        TraceLog(LOG_WARNING, "TOON: depth texture unavailable, ink outlines disabled");
    }
    if (a->sceneTarget.texture.id == 0) return false;

    a->sceneOutputWidth = outputW;
    a->sceneOutputHeight = outputH;
    a->sceneRenderScale = renderScale;
    SetTextureFilter(a->sceneTarget.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(a->sceneTarget.texture, TEXTURE_WRAP_CLAMP);
    if (a->depthOk) SetTextureWrap(a->sceneTarget.depth, TEXTURE_WRAP_CLAMP);
    UpdateSceneResolutionUniforms(a);
    TraceLog(LOG_INFO, "POST: %dx%d output -> %dx%d scene target (%.2fx%s)",
             outputW, outputH, targetW, targetH, renderScale,
             a->depthOk ? ", sampleable depth" : "");
    return true;
}

bool AssetsLoad(Assets *a, int screenW, int screenH, float renderScale)
{
    *a = (Assets){ 0 };

    // BeginMode3D reads these rlgl-wide values for both the menu podium and match camera.
    // Keep the post shader in sync below; mismatched values corrupt linearized outlines.
    rlSetClipPlanes(SCENE_CLIP_NEAR, SCENE_CLIP_FAR);
    TraceLog(LOG_INFO, "RENDER: perspective clip range %.2f..%.1f",
             SCENE_CLIP_NEAR, SCENE_CLIP_FAR);

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
        a->locSourceResolution = GetShaderLocation(a->post, "sourceResolution");
        a->locOutputResolution = GetShaderLocation(a->post, "outputResolution");
        a->locBloom      = GetShaderLocation(a->post, "bloomStrength");
        a->locVignette   = GetShaderLocation(a->post, "vignetteStrength");
        a->locDepthTex   = GetShaderLocation(a->post, "depthTex");
        a->locOutline    = GetShaderLocation(a->post, "outlineStrength");
        a->locClipNear   = GetShaderLocation(a->post, "clipNear");
        a->locClipFar    = GetShaderLocation(a->post, "clipFar");
        a->locPixelate   = GetShaderLocation(a->post, "stylePixelate");
        a->locPainterly  = GetShaderLocation(a->post, "stylePainterly");
        a->locHalftone   = GetShaderLocation(a->post, "styleHalftone");
        a->locPosterize  = GetShaderLocation(a->post, "stylePosterize");
        a->locGrain      = GetShaderLocation(a->post, "styleGrain");
        a->locCA         = GetShaderLocation(a->post, "styleCA");
        a->locSaturation = GetShaderLocation(a->post, "styleSaturation");
        a->locBrightness = GetShaderLocation(a->post, "styleBrightness");
        a->locExposure   = GetShaderLocation(a->post, "styleExposure");
        a->locTonemap    = GetShaderLocation(a->post, "styleTonemap");

        SetShaderValue(a->post, a->locClipNear, &SCENE_CLIP_NEAR, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->post, a->locClipFar, &SCENE_CLIP_FAR, SHADER_UNIFORM_FLOAT);
    }

    a->locDither = GetShaderLocation(a->lighting, "dither");
    a->locToon = GetShaderLocation(a->lighting, "toonMode");
    a->locToonBands = GetShaderLocation(a->lighting, "toonBands");

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
        a->kToon       = GetShaderLocation(a->skinned, "toonMode");
        a->kToonBands  = GetShaderLocation(a->skinned, "toonBands");
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

    for (int cls = 0; cls < CLASS_COUNT; cls++)
        if (CHARACTER_MODEL_PATHS[cls])
            LoadRiggedCharacter(a, &a->characters[cls], CHARACTER_MODEL_PATHS[cls],
                                CLASS_NAMES[cls]);

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
        a->gToon          = GetShaderLocation(a->grass, "toonMode");
        a->gToonBands     = GetShaderLocation(a->grass, "toonBands");
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
    GeneratedAssetsLoad(a);
    LoadVfxAtlases(a);

    //--- Material -------------------------------------------------------------
    a->mat = LoadMaterialDefault();
    if (a->lightingOk) a->mat.shader = a->lighting;

    LoadStationAssets(a);

    a->grassMat = LoadMaterialDefault();
    if (a->grassOk) a->grassMat.shader = a->grass;
    a->grassMat.maps[MATERIAL_MAP_DIFFUSE].texture = a->texGrass;
    a->grassMat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;

    AssetsResizeViewport(a, screenW, screenH, renderScale);

    return a->lightingOk;
}

void AssetsUnload(Assets *a)
{
    for (int id = 0; id < STATION_MODEL_COUNT; id++)
    {
        StationModel *station = &a->station[id];
        if (!station->ok) continue;
        UnloadModel(station->model);
    }
    if (a->texStationOrange.id > 0) UnloadTexture(a->texStationOrange);
    if (a->texStationPurple.id > 0) UnloadTexture(a->texStationPurple);

    UnloadMesh(a->cube);
    UnloadMesh(a->sphere);
    UnloadMesh(a->cylinder);
    UnloadMesh(a->plane);
    UnloadMesh(a->shieldArc);
    UnloadMesh(a->dome);
    UnloadMesh(a->column);
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
    UnloadTexture(a->texStarfield);
    UnloadTexture(a->texPlanet);
    for (int atlas = 0; atlas < VFX_ATLAS_COUNT; atlas++)
        if (a->vfxAtlasesOk[atlas] && a->vfxAtlases[atlas].id > 0)
            UnloadTexture(a->vfxAtlases[atlas]);

    ReleaseSceneTarget(a);

    // The material borrows the shader, so drop its reference before unloading it.
    a->mat.shader = (Shader){ 0 };
    UnloadMaterial(a->mat);
    a->grassMat.shader = (Shader){ 0 };
    a->grassMat.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };
    UnloadMaterial(a->grassMat);

    if (a->lightingOk) UnloadShader(a->lighting);
    if (a->postOk) UnloadShader(a->post);
    if (a->grassOk) UnloadShader(a->grass);

    for (int cls = 0; cls < CLASS_COUNT; cls++)
    {
        RiggedCharacter *character = &a->characters[cls];
        if (!character->ok) continue;
        if (character->anims)
            UnloadModelAnimations(character->anims, character->animCount);
        for (int i = 0; i < character->model.materialCount; i++)
            character->model.materials[i].shader = (Shader){ 0 };
        UnloadModel(character->model);
    }
    if (a->skinnedOk) UnloadShader(a->skinned);
}

bool AssetsResizeViewport(Assets *a, int screenW, int screenH, float renderScale)
{
    if (!a || screenW < 1 || screenH < 1) return false;
    if (renderScale < 1.0f) renderScale = 1.0f;
    if (renderScale > 2.0f) renderScale = 2.0f;

    int targetW = ScaledSceneDimension(screenW, renderScale);
    int targetH = ScaledSceneDimension(screenH, renderScale);
    if (a->sceneOutputWidth == screenW &&
        a->sceneOutputHeight == screenH &&
        a->sceneTarget.texture.width == targetW &&
        a->sceneTarget.texture.height == targetH)
    {
        UpdateSceneResolutionUniforms(a);
        return true;
    }

    ReleaseSceneTarget(a);
    bool ok = CreateSceneTarget(a, screenW, screenH, renderScale);
    if (!ok && renderScale > 1.001f)
    {
        TraceLog(LOG_WARNING,
                 "POST: %.2fx scene target failed; retrying at native resolution",
                 renderScale);
        ReleaseSceneTarget(a);
        ok = CreateSceneTarget(a, screenW, screenH, 1.0f);
    }
    if (!ok)
        TraceLog(LOG_WARNING, "POST: viewport target recreation failed; using direct render");
    return ok;
}

//------------------------------------------------------------------------------------
void DrawLit(Assets *a, Mesh mesh, Matrix transform, Texture2D tex, Color tint,
             Vector2 uvScale, float emissive)
{
    if (a->lightingOk)
    {
        // Hundreds of lit draws per frame pass the identical {1,1}/0 values;
        // uploading them every call was pure uniform churn.
        if (!a->litStateValid ||
            uvScale.x != a->litUvScale.x || uvScale.y != a->litUvScale.y)
        {
            SetShaderValue(a->lighting, a->locUvScale, &uvScale, SHADER_UNIFORM_VEC2);
            a->litUvScale = uvScale;
        }
        if (!a->litStateValid || emissive != a->litEmissive)
        {
            SetShaderValue(a->lighting, a->locEmissive, &emissive, SHADER_UNIFORM_FLOAT);
            a->litEmissive = emissive;
        }
        a->litStateValid = true;
    }

    a->mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    a->mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;

    DrawMesh(mesh, a->mat, transform);
}

bool AssetsDrawStationModel(Assets *a, StationModelId id, Matrix transform,
                            StationPalette palette, Color tint, float emissive)
{
    if (id < 0 || id >= STATION_MODEL_COUNT) return false;

    StationModel *station = &a->station[id];
    if (!station->ok || station->model.meshCount <= 0) return false;

    Texture2D atlas = (palette == STATION_PALETTE_PURPLE)
                    ? a->texStationPurple : a->texStationOrange;
    Matrix modelTransform = MatrixMultiply(station->model.transform, transform);

    for (int i = 0; i < station->model.meshCount; i++)
    {
        Texture2D texture = atlas;
        if (texture.id == 0 && station->model.materialCount > 0)
        {
            int material = station->model.meshMaterial[i];
            if (material < 0 || material >= station->model.materialCount) material = 0;
            texture = station->model.materials[material].maps[MATERIAL_MAP_DIFFUSE].texture;
        }
        if (texture.id == 0) texture = a->texFlat;

        DrawLit(a, station->model.meshes[i], modelTransform, texture, tint,
                (Vector2){ 1.0f, 1.0f }, emissive);
    }
    return true;
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

static int ActionClipFor(const RiggedCharacter *character, CharacterActionId action)
{
    switch (action)
    {
        case CHARACTER_ACTION_MAIN: return character->clipActionMain;
        case CHARACTER_ACTION_SUPER: return character->clipActionSuper;
        case CHARACTER_ACTION_CAST: return character->clipActionCast;
        case CHARACTER_ACTION_MOBILITY: return character->clipActionMobility;
        case CHARACTER_ACTION_GUARD: return character->clipActionGuard;
        case CHARACTER_ACTION_GRAPPLE: return character->clipActionGrapple;
        case CHARACTER_ACTION_MINE_DEPLOY:
            return character->clipActionMineDeploy;
        default: return -1;
    }
}

static Transform BlendPoseTransform(Transform base, Transform action, float weight)
{
    return (Transform){
        .translation = Vector3Lerp(base.translation, action.translation, weight),
        .rotation = QuaternionSlerp(base.rotation, action.rotation, weight),
        .scale = Vector3Lerp(base.scale, action.scale, weight)
    };
}

// Samples a clip at a fractional frame, interpolating between the neighbouring
// authored poses. Whole-frame stepping made characters pose-step whenever the
// display ran faster than the clip rate or playback was slowed.
static void SamplePose(const ModelAnimation *anim, float frame, bool loop,
                       Transform *pose)
{
    float position = frame;
    int f0, f1;
    if (loop)
    {
        position = fmodf(position, (float)anim->frameCount);
        if (position < 0.0f) position += (float)anim->frameCount;
        f0 = (int)position;
        if (f0 >= anim->frameCount) f0 = anim->frameCount - 1;
        f1 = (f0 + 1)%anim->frameCount;
    }
    else
    {
        if (position < 0.0f) position = 0.0f;
        if (position > (float)(anim->frameCount - 1))
            position = (float)(anim->frameCount - 1);
        f0 = (int)position;
        f1 = (f0 + 1 < anim->frameCount) ? f0 + 1 : f0;
    }
    float t = position - (float)f0;
    for (int bone = 0; bone < anim->boneCount; bone++)
        pose[bone] = (t > 0.0001f)
                   ? BlendPoseTransform(anim->framePoses[f0][bone],
                                        anim->framePoses[f1][bone], t)
                   : anim->framePoses[f0][bone];
}

static bool BoneDescendsFrom(const RiggedCharacter *character, int bone, int root)
{
    for (int guard = 0; guard < character->model.boneCount && bone >= 0; guard++)
    {
        if (bone == root) return true;
        bone = character->model.bones[bone].parent;
    }
    return false;
}

static void RotatePoseSubtree(const RiggedCharacter *character, Transform *pose,
                              int root, Quaternion targetDelta, float weight)
{
    if (root < 0 || root >= character->model.boneCount || weight <= 0.001f) return;
    Quaternion delta = QuaternionSlerp(QuaternionIdentity(), targetDelta,
                                       Clamp(weight, 0.0f, 1.0f));
    Vector3 pivot = pose[root].translation;
    for (int bone = 0; bone < character->model.boneCount; bone++)
    {
        if (!BoneDescendsFrom(character, bone, root)) continue;
        Vector3 relative = Vector3Subtract(pose[bone].translation, pivot);
        pose[bone].translation =
            Vector3Add(pivot, Vector3RotateByQuaternion(relative, delta));
        pose[bone].rotation =
            QuaternionNormalize(QuaternionMultiply(delta, pose[bone].rotation));
    }
}

static void TranslatePoseSubtree(const RiggedCharacter *character, Transform *pose,
                                 int root, Vector3 translation, float weight)
{
    if (root < 0 || root >= character->model.boneCount || weight <= 0.001f) return;
    translation = Vector3Scale(translation, Clamp(weight, 0.0f, 1.0f));
    for (int bone = 0; bone < character->model.boneCount; bone++)
        if (BoneDescendsFrom(character, bone, root))
            pose[bone].translation =
                Vector3Add(pose[bone].translation, translation);
}

static void AimPoseArm(const RiggedCharacter *character, Transform *pose,
                       int shoulder, int hand, Vector3 targetDirection,
                       float weight)
{
    if (shoulder < 0 || hand < 0 || weight <= 0.001f) return;
    Vector3 direction =
        Vector3Subtract(pose[hand].translation, pose[shoulder].translation);
    if (Vector3Length(direction) <= 0.0001f) return;
    Vector3 from = Vector3Normalize(direction);
    Vector3 to = Vector3Normalize(targetDirection);

    // Antiparallel vectors make QuaternionFromVector3ToVector3 return a zero
    // quaternion, and slerping toward that scales the limb toward its shoulder.
    // Pick an explicit half-turn around any axis perpendicular to the arm instead.
    Quaternion delta;
    if (Vector3DotProduct(from, to) < -0.9995f)
    {
        Vector3 axis = Vector3CrossProduct((Vector3){ 0.0f, 1.0f, 0.0f }, from);
        if (Vector3Length(axis) <= 0.0001f)
            axis = Vector3CrossProduct((Vector3){ 1.0f, 0.0f, 0.0f }, from);
        delta = QuaternionFromAxisAngle(Vector3Normalize(axis), PI);
    }
    else delta = QuaternionFromVector3ToVector3(from, to);
    RotatePoseSubtree(character, pose, shoulder, delta, weight);
}

// Authored motion vocabulary: each parameterized motion rotates a bone group over
// its own eased envelope, so stacked motions compose a custom action pose. The
// sine envelope guarantees an identity pose at both boundaries.
static void ApplyAuthoredMotions(const RiggedCharacter *character, Transform *pose,
                                 const AttackMotion *motions, float seconds)
{
    for (int i = 0; i < MAX_ATTACK_MOTIONS; i++)
    {
        const AttackMotion *m = &motions[i];
        if (!m->used || m->kind == ATTACK_MOTION_NONE) continue;
        float t = (seconds - m->delay)/fmaxf(m->duration, 0.02f);
        if (t <= 0.0f || t >= 1.0f) continue;
        float envelope = sinf(t*PI);
        float weight = Clamp(envelope, 0.0f, 1.0f);
        float a = m->amplitude;

        switch (m->kind)
        {
            case ATTACK_MOTION_RECOIL:
                RotatePoseSubtree(character, pose, character->boneChest,
                                  QuaternionFromEuler(-0.18f*a, 0.0f, 0.0f), weight);
                break;
            case ATTACK_MOTION_RAISE_RIGHT_ARM:
                AimPoseArm(character, pose, character->boneRightShoulder,
                           character->boneRightHand,
                           (Vector3){ 0.15f, 0.35f + 0.45f*a, 0.85f }, weight);
                break;
            case ATTACK_MOTION_RAISE_LEFT_ARM:
                AimPoseArm(character, pose, character->boneLeftShoulder,
                           character->boneLeftHand,
                           (Vector3){ -0.15f, 0.35f + 0.45f*a, 0.85f }, weight);
                break;
            case ATTACK_MOTION_SWING_RIGHT:
            {
                float sweep = (-0.9f + 1.8f*t)*a;
                AimPoseArm(character, pose, character->boneRightShoulder,
                           character->boneRightHand,
                           (Vector3){ sinf(sweep), 0.25f, cosf(sweep) }, weight);
                break;
            }
            case ATTACK_MOTION_TWIST:
                RotatePoseSubtree(character, pose, character->boneSpine,
                                  QuaternionFromEuler(0.0f, 0.55f*a, 0.0f), weight);
                break;
            case ATTACK_MOTION_SLAM:
            {
                float pitch = 1.15f - 2.0f*t;
                AimPoseArm(character, pose, character->boneLeftShoulder,
                           character->boneLeftHand,
                           (Vector3){ -0.25f, pitch*a, 0.62f }, weight);
                AimPoseArm(character, pose, character->boneRightShoulder,
                           character->boneRightHand,
                           (Vector3){ 0.25f, pitch*a, 0.62f }, weight);
                RotatePoseSubtree(character, pose, character->boneChest,
                                  QuaternionFromEuler(0.20f*a*t, 0.0f, 0.0f), weight);
                break;
            }
            case ATTACK_MOTION_LEAN:
                RotatePoseSubtree(character, pose, character->boneSpine,
                                  QuaternionFromEuler(0.40f*a, 0.0f, 0.0f), weight);
                break;
            case ATTACK_MOTION_RAISE_BOTH:
                AimPoseArm(character, pose, character->boneLeftShoulder,
                           character->boneLeftHand,
                           (Vector3){ -0.22f, 0.40f + 0.45f*a, 0.72f }, weight);
                AimPoseArm(character, pose, character->boneRightShoulder,
                           character->boneRightHand,
                           (Vector3){ 0.22f, 0.40f + 0.45f*a, 0.72f }, weight);
                break;
            case ATTACK_MOTION_CONDUCT:
            {
                // Both arms high with a slow opposing sway, plus a gentle
                // lean-back: the conductor pose.
                float sway = sinf(t*PI*2.0f)*0.28f*a;
                AimPoseArm(character, pose, character->boneLeftShoulder,
                           character->boneLeftHand,
                           (Vector3){ -0.30f + sway, 0.85f*a, 0.55f }, weight);
                AimPoseArm(character, pose, character->boneRightShoulder,
                           character->boneRightHand,
                           (Vector3){ 0.30f + sway, 0.85f*a, 0.55f }, weight);
                RotatePoseSubtree(character, pose, character->boneSpine,
                                  QuaternionFromEuler(-0.14f*a, 0.0f, 0.0f),
                                  weight);
                break;
            }
            default: break;
        }
    }
}

static void ApplyProceduralAction(const RiggedCharacter *character, Transform *pose,
                                  CharacterActionId action, float progress,
                                  float weight)
{
    if (action <= CHARACTER_ACTION_NONE || weight <= 0.001f) return;
    float kick = weight;
    if (action == CHARACTER_ACTION_MAIN)
    {
        // A single-arm snap and small torso recoil reads as a fired shot without
        // assuming a particular weapon mesh or modifying gameplay aim.
        AimPoseArm(character, pose, character->boneRightShoulder,
                   character->boneRightHand, (Vector3){ 0.12f, 0.08f, 1.0f },
                   kick*0.92f);
        RotatePoseSubtree(character, pose, character->boneChest,
                          QuaternionFromEuler(-0.10f, -0.08f, 0.0f),
                          kick*(0.65f + 0.35f*sinf(progress*PI)));
    }
    else if (action == CHARACTER_ACTION_SUPER)
    {
        AimPoseArm(character, pose, character->boneLeftShoulder,
                   character->boneLeftHand, (Vector3){ -0.20f, 0.24f, 1.0f },
                   kick*0.88f);
        AimPoseArm(character, pose, character->boneRightShoulder,
                   character->boneRightHand, (Vector3){ 0.20f, 0.24f, 1.0f },
                   kick*0.88f);
        RotatePoseSubtree(character, pose, character->boneSpine,
                          QuaternionFromEuler(0.15f, 0.0f, 0.0f), kick*0.72f);
    }
    else if (action == CHARACTER_ACTION_CAST)
    {
        AimPoseArm(character, pose, character->boneLeftShoulder,
                   character->boneLeftHand, (Vector3){ -0.38f, 0.80f, 0.62f },
                   kick*0.90f);
        AimPoseArm(character, pose, character->boneRightShoulder,
                   character->boneRightHand, (Vector3){ 0.38f, 0.80f, 0.62f },
                   kick*0.90f);
        RotatePoseSubtree(character, pose, character->boneChest,
                          QuaternionFromEuler(-0.08f, 0.0f, 0.0f), kick*0.55f);
    }
    else if (action == CHARACTER_ACTION_MOBILITY)
    {
        RotatePoseSubtree(character, pose, character->boneSpine,
                          QuaternionFromEuler(0.30f, 0.0f, 0.0f), kick);
        AimPoseArm(character, pose, character->boneLeftShoulder,
                   character->boneLeftHand, (Vector3){ -0.30f, -0.18f, -1.0f },
                   kick*0.70f);
        AimPoseArm(character, pose, character->boneRightShoulder,
                   character->boneRightHand, (Vector3){ 0.30f, -0.18f, -1.0f },
                   kick*0.70f);
    }
    else if (action == CHARACTER_ACTION_GUARD)
    {
        AimPoseArm(character, pose, character->boneLeftShoulder,
                   character->boneLeftHand, (Vector3){ -0.70f, 0.36f, 0.70f },
                   kick*0.92f);
        AimPoseArm(character, pose, character->boneRightShoulder,
                   character->boneRightHand, (Vector3){ 0.70f, 0.36f, 0.70f },
                   kick*0.92f);
        RotatePoseSubtree(character, pose, character->boneSpine,
                         QuaternionFromEuler(-0.16f, 0.0f, 0.0f), kick*0.85f);
    }
    else if (action == CHARACTER_ACTION_GRAPPLE)
    {
        // Three readable beats: shoot the line, brace with an asymmetric stance,
        // then tuck into the pull. The gameplay timer supplies the full launch+pull
        // duration, so this pose stays synchronized even after tuning changes.
        float launch = Clamp(progress/0.24f, 0.0f, 1.0f);
        float pull = Clamp((progress - 0.18f)/0.48f, 0.0f, 1.0f);
        float land = Clamp((progress - 0.78f)/0.22f, 0.0f, 1.0f);
        float brace = pull*(1.0f - land);

        AimPoseArm(character, pose, character->boneRightShoulder,
                   character->boneRightHand,
                   (Vector3){ 0.20f, 0.36f - pull*0.25f, 1.0f },
                   kick*(0.70f + 0.30f*launch));
        AimPoseArm(character, pose, character->boneLeftShoulder,
                   character->boneLeftHand,
                   (Vector3){ -0.22f, 0.12f, 0.92f },
                   kick*(0.42f + 0.52f*brace));
        RotatePoseSubtree(character, pose, character->boneSpine,
                         QuaternionFromEuler(0.27f*brace,
                                             -0.14f*(1.0f - pull),
                                             -0.10f*brace),
                         kick);
        RotatePoseSubtree(character, pose, character->boneLeftUpLeg,
                         QuaternionFromEuler(-0.24f*brace, 0.0f, 0.12f),
                         kick*brace);
        RotatePoseSubtree(character, pose, character->boneRightUpLeg,
                         QuaternionFromEuler(0.18f*brace, 0.0f, -0.10f),
                         kick*brace);
        TranslatePoseSubtree(character, pose, character->boneHips,
                             (Vector3){ 0.0f, -0.08f, -0.10f },
                             kick*brace);
    }
    else if (action == CHARACTER_ACTION_MINE_DEPLOY)
    {
        // Mortar folds into a compact kneel, plants the charge with both hands,
        // then springs back up. This is intentionally distinct from the generic cast.
        float down = Clamp(progress/0.30f, 0.0f, 1.0f);
        float up = Clamp((progress - 0.68f)/0.32f, 0.0f, 1.0f);
        float crouch = down*(1.0f - up);
        float plant = Clamp((progress - 0.16f)/0.34f, 0.0f, 1.0f);

        TranslatePoseSubtree(character, pose, character->boneHips,
                             (Vector3){ 0.0f, -0.32f, 0.07f },
                             kick*crouch);
        RotatePoseSubtree(character, pose, character->boneSpine,
                         QuaternionFromEuler(-0.42f*crouch, 0.0f, 0.0f),
                         kick);
        AimPoseArm(character, pose, character->boneLeftShoulder,
                   character->boneLeftHand,
                   (Vector3){ -0.30f, -0.86f, 0.42f },
                   kick*plant);
        AimPoseArm(character, pose, character->boneRightShoulder,
                   character->boneRightHand,
                   (Vector3){ 0.30f, -0.86f, 0.42f },
                   kick*plant);
        RotatePoseSubtree(character, pose, character->boneLeftUpLeg,
                         QuaternionFromEuler(-0.58f*crouch, 0.0f, 0.10f),
                         kick);
        RotatePoseSubtree(character, pose, character->boneRightUpLeg,
                         QuaternionFromEuler(-0.58f*crouch, 0.0f, -0.10f),
                         kick);
        RotatePoseSubtree(character, pose, character->boneLeftLeg,
                         QuaternionFromEuler(0.52f*crouch, 0.0f, 0.0f),
                         kick);
        RotatePoseSubtree(character, pose, character->boneRightLeg,
                         QuaternionFromEuler(0.52f*crouch, 0.0f, 0.0f),
                         kick);
    }
}

static void StoreSocket(CharacterSocketPose *sockets, VfxSocket socket,
                        const Transform *pose, int bone, Matrix world,
                        int boneCount)
{
    if (!sockets || socket <= VFX_SOCKET_NONE || socket >= VFX_SOCKET_COUNT ||
        bone < 0 || bone >= boneCount)
        return;
    sockets->positions[socket] =
        Vector3Transform(pose[bone].translation, world);
    sockets->valid[socket] = true;
}

static void StoreCharacterSockets(const RiggedCharacter *character,
                                  const Transform *pose, Matrix world,
                                  CharacterSocketPose *sockets)
{
    if (!sockets) return;
    // Render seeds an approximate pose before model drawing so a missing semantic
    // bone can retain a useful fallback instead of collapsing that socket to the
    // gameplay origin. Mapped Meshy bones replace those estimates below.
    sockets->rigged = true;
    StoreSocket(sockets, VFX_SOCKET_CENTER, pose, character->boneHips,
                world, character->model.boneCount);
    StoreSocket(sockets, VFX_SOCKET_CHEST, pose, character->boneChest,
                world, character->model.boneCount);
    StoreSocket(sockets, VFX_SOCKET_LEFT_HAND, pose, character->boneLeftHand,
                world, character->model.boneCount);
    StoreSocket(sockets, VFX_SOCKET_RIGHT_HAND, pose, character->boneRightHand,
                world, character->model.boneCount);
    StoreSocket(sockets, VFX_SOCKET_LEFT_SHOULDER, pose,
                character->boneLeftShoulder, world, character->model.boneCount);
    StoreSocket(sockets, VFX_SOCKET_RIGHT_SHOULDER, pose,
                character->boneRightShoulder, world, character->model.boneCount);
    StoreSocket(sockets, VFX_SOCKET_LEFT_FOOT, pose, character->boneLeftFoot,
                world, character->model.boneCount);
    StoreSocket(sockets, VFX_SOCKET_RIGHT_FOOT, pose, character->boneRightFoot,
                world, character->model.boneCount);
}

void AssetsSkinnedFrame(Assets *a, const Vector3 *lightPos,
                        const Vector3 *lightColor, int lightCount,
                        Vector3 viewPos)
{
    if (!a->skinnedOk) return;
    SetShaderValue(a->skinned, a->kViewPos, &viewPos, SHADER_UNIFORM_VEC3);
    if (lightCount > MAX_SHADER_LIGHTS) lightCount = MAX_SHADER_LIGHTS;
    if (lightCount > 0)
    {
        SetShaderValueV(a->skinned, a->kLightPos, lightPos, SHADER_UNIFORM_VEC3, lightCount);
        SetShaderValueV(a->skinned, a->kLightColor, lightColor, SHADER_UNIFORM_VEC3, lightCount);
    }
    SetShaderValue(a->skinned, a->kLightCount, &lightCount, SHADER_UNIFORM_INT);
}

void AssetsDrawCharacter(Assets *a, BrawlerClass cls, Vector3 position, float yaw, float scaleMul,
                         int animIndex, float frame, bool loop,
                         int fadeClip, float fadeFrame, float fadeAlpha,
                         Color tint, float dither,
                         float emissive, CharacterActionId action,
                         float actionProgress, float actionWeight,
                         const AttackMotion *authoredMotions, float actionSeconds,
                         CharacterSocketPose *socketPose)
{
    if (cls < 0 || cls >= CLASS_COUNT) return;
    RiggedCharacter *character = &a->characters[cls];
    if (!character->ok) return;

    if (a->skinnedOk)
    {
        SetShaderValue(a->skinned, a->kDither, &dither, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->skinned, a->kEmissive, &emissive, SHADER_UNIFORM_FLOAT);
    }

    float s = character->scale*scaleMul;
    Matrix m = MatrixScale(s, s, s);
    m = MatrixMultiply(m, MatrixRotateY(yaw));
    m = MatrixMultiply(m, MatrixTranslate(position.x,
                                          position.y + character->footOffset*scaleMul,
                                          position.z));

    // Posing writes into the model's shared bone matrices, so it has to happen
    // immediately before this draw rather than once per frame.
    if (character->anims && character->animCount > 0)
    {
        int idx = (animIndex < 0 || animIndex >= character->animCount) ? 0 : animIndex;
        ModelAnimation anim = character->anims[idx];
        if (anim.frameCount > 0)
        {
            if (anim.boneCount > 0 && anim.boneCount <= CHARACTER_BONE_LIMIT)
            {
                Transform pose[CHARACTER_BONE_LIMIT];
                SamplePose(&anim, frame, loop, pose);

                // Crossfade: the outgoing clip's pose, frozen at the moment of the
                // switch, eases away beneath the incoming clip.
                if (fadeAlpha < 0.999f && fadeClip >= 0 &&
                    fadeClip < character->animCount)
                {
                    ModelAnimation outgoing = character->anims[fadeClip];
                    if (outgoing.frameCount > 0 &&
                        outgoing.boneCount == anim.boneCount)
                    {
                        Transform fadePose[CHARACTER_BONE_LIMIT];
                        SamplePose(&outgoing, fadeFrame, true, fadePose);
                        float in = Clamp(fadeAlpha, 0.0f, 1.0f);
                        in = in*in*(3.0f - 2.0f*in);
                        for (int bone = 0; bone < anim.boneCount; bone++)
                            pose[bone] = BlendPoseTransform(fadePose[bone],
                                                            pose[bone], in);
                    }
                }

                // Authored motion stacks replace both the optional authored
                // clip and the procedural overlay for their ability.
                if (authoredMotions)
                {
                    ApplyAuthoredMotions(character, pose, authoredMotions,
                                         actionSeconds);
                }
                else
                {
                bool authoredApplied = false;
                int actionClip = ActionClipFor(character, action);
                if (actionWeight > 0.001f && actionClip >= 0 &&
                    actionClip < character->animCount)
                {
                    ModelAnimation authored = character->anims[actionClip];
                    if (authored.boneCount == anim.boneCount &&
                        authored.frameCount > 0)
                    {
                        float actionPos = Clamp(actionProgress, 0.0f, 1.0f)*
                                          (float)(authored.frameCount - 1);
                        Transform actionPose[CHARACTER_BONE_LIMIT];
                        SamplePose(&authored, actionPos, false, actionPose);
                        for (int bone = 0; bone < anim.boneCount; bone++)
                            pose[bone] = BlendPoseTransform(
                                pose[bone], actionPose[bone],
                                Clamp(actionWeight, 0.0f, 1.0f));
                        authoredApplied = true;
                    }
                }
                // A skeleton-mismatched authored clip must not silently disable the
                // action; the procedural overlay covers it like any missing clip.
                if (!authoredApplied)
                    ApplyProceduralAction(character, pose, action, actionProgress,
                                          actionWeight);
                }

                Transform *frames[1] = { pose };
                ModelAnimation composed = {
                    .boneCount = anim.boneCount,
                    .frameCount = 1,
                    .bones = anim.bones,
                    .framePoses = frames
                };
                UpdateModelAnimationBones(character->model, composed, 0);
                StoreCharacterSockets(character, pose, m, socketPose);
            }
            else
            {
                // Bone-limit fallback: stepped whole-frame playback.
                int f = (int)frame;
                if (loop)
                {
                    f %= anim.frameCount;
                    if (f < 0) f += anim.frameCount;
                }
                else f = (f < 0) ? 0 : (f >= anim.frameCount ? anim.frameCount - 1 : f);
                UpdateModelAnimationBones(character->model, anim, f);
            }
        }
    }

    for (int i = 0; i < character->model.meshCount; i++)
    {
        // Material.maps is a pointer, so the copy is shallow and the tint write
        // lands in the model's shared material. Restore it after the draw so no
        // other call site inherits this brawler's team tint.
        Material mat = character->model.materials[character->model.meshMaterial[i]];
        Color previous = mat.maps[MATERIAL_MAP_DIFFUSE].color;
        mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        DrawMesh(character->model.meshes[i], mat, m);
        mat.maps[MATERIAL_MAP_DIFFUSE].color = previous;
    }
}

void AssetsSetStyle(Assets *a, const Tuning *t, float time, float outlineStrength)
{
    if (!a->postOk) return;
    (void)time; // Retained in the presentation API for future time-based styles.

    SetShaderValue(a->post, a->locBloom, &t->bloom, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locVignette, &t->styleVignette, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locOutline, &outlineStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locPixelate, &t->stylePixelate, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locPainterly, &t->stylePainterly, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locHalftone, &t->styleHalftone, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locPosterize, &t->stylePosterize, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locGrain, &t->styleGrain, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locCA, &t->styleCA, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locSaturation, &t->styleSaturation, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locBrightness, &t->styleBrightness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locExposure, &t->styleExposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(a->post, a->locTonemap, &t->styleTonemap, SHADER_UNIFORM_FLOAT);
}

void AssetsSetToon(Assets *a, bool enabled, float bands)
{
    float mode = enabled ? 1.0f : 0.0f;
    if (bands < 2.0f) bands = 2.0f;

    if (a->lightingOk)
    {
        SetShaderValue(a->lighting, a->locToon, &mode, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->lighting, a->locToonBands, &bands, SHADER_UNIFORM_FLOAT);
    }
    if (a->skinnedOk)
    {
        SetShaderValue(a->skinned, a->kToon, &mode, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->skinned, a->kToonBands, &bands, SHADER_UNIFORM_FLOAT);
    }
    if (a->grassOk)
    {
        SetShaderValue(a->grass, a->gToon, &mode, SHADER_UNIFORM_FLOAT);
        SetShaderValue(a->grass, a->gToonBands, &bands, SHADER_UNIFORM_FLOAT);
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
