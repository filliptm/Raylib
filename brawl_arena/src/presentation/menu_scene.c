#include "menu_scene.h"

#include "character_animation.h"
#include "content_catalog.h"
#include "render.h"
#include "raymath.h"
#include <math.h>
#include <stddef.h>

#if defined(GRAPHICS_API_OPENGL_ES3)
#define BRAWL_GLSL_HEADER \
    "#version 300 es\n" \
    "precision highp float;\n" \
    "precision highp int;\n"
#else
#define BRAWL_GLSL_HEADER "#version 330\n"
#endif

static const char *FS_STICKER =
BRAWL_GLSL_HEADER
"in vec2 fragTexCoord;\n"
"in vec4 fragColor;\n"
"uniform sampler2D texture0;\n"
"uniform vec2 resolution;\n"
"uniform float innerPixels;\n"
"uniform float outerPixels;\n"
"uniform vec4 inkColor;\n"
"uniform vec4 paperColor;\n"
"out vec4 finalColor;\n"
"float sampleAlpha(vec2 direction, float pixels) {\n"
"    return texture(texture0, fragTexCoord + direction*pixels/resolution).a;\n"
"}\n"
"float ring16(float pixels) {\n"
"    float a = 0.0;\n"
"    a = max(a, sampleAlpha(vec2( 1.00000,  0.00000), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.92388,  0.38268), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.70711,  0.70711), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.38268,  0.92388), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.00000,  1.00000), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-0.38268,  0.92388), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-0.70711,  0.70711), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-0.92388,  0.38268), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-1.00000,  0.00000), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-0.92388, -0.38268), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-0.70711, -0.70711), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-0.38268, -0.92388), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.00000, -1.00000), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.38268, -0.92388), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.70711, -0.70711), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.92388, -0.38268), pixels));\n"
"    return a;\n"
"}\n"
"float ring8(float pixels) {\n"
"    float a = 0.0;\n"
"    a = max(a, sampleAlpha(vec2( 1.00000,  0.00000), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.70711,  0.70711), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.00000,  1.00000), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-0.70711,  0.70711), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-1.00000,  0.00000), pixels));\n"
"    a = max(a, sampleAlpha(vec2(-0.70711, -0.70711), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.00000, -1.00000), pixels));\n"
"    a = max(a, sampleAlpha(vec2( 0.70711, -0.70711), pixels));\n"
"    return a;\n"
"}\n"
"float roundMask(float pixels) {\n"
"    float edge = ring16(pixels);\n"
"    float middle = ring8(pixels*0.58);\n"
"    return smoothstep(0.015, 0.46, max(edge, middle));\n"
"}\n"
"void main() {\n"
"    vec4 base = texture(texture0, fragTexCoord)*fragColor;\n"
"    if (base.a > 0.02) { finalColor = base; return; }\n"
"    float inner = roundMask(innerPixels);\n"
"    if (inner > 0.02) { finalColor = vec4(inkColor.rgb, inner*inkColor.a); return; }\n"
"    float outer = roundMask(outerPixels);\n"
"    if (outer > 0.02) { finalColor = vec4(paperColor.rgb, outer); return; }\n"
"    discard;\n"
"}\n";

static void RebuildPreview(MenuScene *scene, const App *app, BrawlerClass kit)
{
    scene->preview = (Brawler){ 0 };
    scene->preview.team = TEAM_PLAYER;
    scene->preview.cls = kit;
    scene->preview.isPlayer = true;
    scene->preview.alive = true;
    scene->preview.visible = true;
    scene->preview.spawnScale = 1.0f;
    scene->preview.maxHealth = ContentCharacter(&app->content, kit)->maxHealth;
    scene->preview.health = scene->preview.maxHealth;
    scene->previewKit = kit;
    // The idle preview restarts from frame zero, matching the match renderer's
    // policy that a fresh clip never lands mid-stride.
    scene->previewTime = 0.0f;
}

void MenuSceneInit(MenuScene *scene, Assets *assets)
{
    *scene = (MenuScene){ 0 };
    scene->assets = assets;
    scene->previewKit = CLASS_COUNT;
    scene->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    scene->camera.fovy = 40.0f;
    scene->camera.projection = CAMERA_PERSPECTIVE;
    scene->stickerShader = LoadShaderFromMemory(NULL, FS_STICKER);
    if (scene->stickerShader.id > 0)
    {
        scene->stickerResolutionLoc =
            GetShaderLocation(scene->stickerShader, "resolution");
        scene->stickerInnerLoc =
            GetShaderLocation(scene->stickerShader, "innerPixels");
        scene->stickerOuterLoc =
            GetShaderLocation(scene->stickerShader, "outerPixels");
        scene->stickerInkLoc =
            GetShaderLocation(scene->stickerShader, "inkColor");
        scene->stickerPaperLoc =
            GetShaderLocation(scene->stickerShader, "paperColor");
    }
    else TraceLog(LOG_WARNING, "MENU: sticker shader unavailable; using direct preview");
}

void MenuSceneUnload(MenuScene *scene)
{
    if (!scene) return;
    if (scene->stickerTarget.id > 0) UnloadRenderTexture(scene->stickerTarget);
    if (scene->stickerShader.id > 0) UnloadShader(scene->stickerShader);
    *scene = (MenuScene){ 0 };
}

void MenuSceneUpdate(MenuScene *scene, const App *app, BrawlerClass candidate,
                     AppScreen screen, float dt)
{
    (void)screen;
    if (candidate < 0 || candidate >= CLASS_COUNT) candidate = CLASS_SHOTGUNNER;
    if (scene->previewKit != candidate) RebuildPreview(scene, app, candidate);
    scene->stageTime += dt;
    scene->previewTime += dt;
    scene->preview.bobPhase += dt*3.4f;

    const CharacterShowcaseDefinition *showcase =
        ContentCharacterShowcase(&app->content);
    float yaw = showcase->yawDegrees*DEG2RAD;
    scene->preview.position =
        (Vector3){ showcase->offset.x, showcase->offset.y, 0.0f };
    scene->preview.renderYaw = yaw;
    scene->preview.aimAngle = yaw;
    scene->camera.position = showcase->cameraPosition;
    scene->camera.target = showcase->cameraTarget;
    scene->camera.fovy = showcase->verticalFov;
}

static bool PrepareScene(MenuScene *scene, const App *app, BrawlerClass candidate,
                         Vector3 lightPos[2], Vector3 lightCol[2])
{
    Assets *a = scene->assets;
    if (!a || candidate < 0 || candidate >= CLASS_COUNT) return false;
    lightPos[0] = (Vector3){ -2.6f, 3.0f, -2.4f };
    lightPos[1] = (Vector3){ 2.8f, 2.4f, 1.6f };
    lightCol[0] = (Vector3){ 0.85f, 0.72f, 0.45f };
    lightCol[1] = (Vector3){ 0.32f, 0.48f, 0.80f };
    AssetsSetCamera(a, scene->camera.position);
    AssetsSetToon(a, app->tune.toon, app->tune.toonBands);
    AssetsSetLights(a, lightPos, lightCol, 2);
    return true;
}

void MenuSceneDrawStage(MenuScene *scene, const App *app, BrawlerClass candidate,
                        AppScreen screen)
{
    if (!scene || !app || candidate < 0 || candidate >= CLASS_COUNT) return;
    float scale = fminf(GetScreenWidth()/1280.0f, GetScreenHeight()/800.0f);
    float ox = (GetScreenWidth() - 1280.0f*scale)*0.5f;
    float oy = (GetScreenHeight() - 800.0f*scale)*0.5f;
    float centerX = ox + 640.0f*scale;
    float centerY = oy + (screen == SCREEN_BRAWLERS ? 610.0f : 612.0f)*scale;
    float rx = 258.0f*scale;
    float ry = 62.0f*scale;
    const CharacterUiStyle *style = ContentCharacterUiStyle(candidate);

    DrawEllipse((int)(centerX + 7.0f*scale), (int)(centerY + 9.0f*scale),
                rx, ry, (Color){ 0, 0, 0, 205 });
    DrawEllipse((int)centerX, (int)centerY, rx, ry, (Color){ 7, 16, 25, 255 });
    DrawEllipse((int)centerX, (int)(centerY - 2.0f*scale),
                rx - 10.0f*scale, ry - 10.0f*scale, style->primary);
    DrawEllipse((int)centerX, (int)(centerY - 3.0f*scale),
                rx - 24.0f*scale, ry - 20.0f*scale,
                (Color){ 7, 76, 141, 255 });

    Color pulse = style->secondary;
    pulse.a = (unsigned char)(150.0f +
        (0.5f + 0.5f*sinf(scene->stageTime*1.6f))*70.0f);
    for (int line = 0; line < 3; line++)
        DrawEllipseLines((int)centerX, (int)(centerY - 3.0f*scale),
                         rx - (28.0f + line)*scale,
                         ry - (23.0f + line)*scale, pulse);

    Color slash = { 7, 16, 25, 185 };
    for (int i = 0; i < 8; i++)
    {
        float x = centerX - rx*0.72f + i*rx*0.205f;
        DrawTriangle((Vector2){ x, centerY + ry*0.56f },
                     (Vector2){ x + 30.0f*scale, centerY - ry*0.50f },
                     (Vector2){ x + 11.0f*scale, centerY + ry*0.62f }, slash);
    }
}

static void DrawBrawlerRaw(MenuScene *scene, const App *app,
                           BrawlerClass candidate, AppScreen screen,
                           float scaleMultiplier)
{
    Vector3 lightPos[2];
    Vector3 lightCol[2];
    if (!PrepareScene(scene, app, candidate, lightPos, lightCol)) return;
    Assets *a = scene->assets;
    (void)screen;
    const CharacterShowcaseDefinition *showcase =
        ContentCharacterShowcase(&app->content);
    float scale = showcase->scale*scaleMultiplier;
    float yaw = showcase->yawDegrees*DEG2RAD;

    BeginMode3D(scene->camera);
        RiggedCharacter *character = &a->characters[candidate];
        bool modelKit = character->ok && app->tune.modelCharacter;
        if (modelKit)
        {
            AssetsSkinnedFrame(a, lightPos, lightCol, 2, scene->camera.position);
            AssetsDrawCharacter(a, candidate, scene->preview.position, yaw, scale,
                                character->clipIdle,
                                scene->previewTime*CHARACTER_CLIP_FPS,
                                true, -1, 0.0f, 1.0f, WHITE,
                                0.0f, 0.0f, CHARACTER_ACTION_NONE, 0.0f, 0.0f,
                                NULL, 0.0f, 0);
        }
        else
        {
            Brawler fallback = scene->preview;
            fallback.renderYaw = yaw;
            fallback.aimAngle = yaw;
            fallback.spawnScale = scale;
            Color accent = ContentCharacterUiStyle(candidate)->primary;
            RenderBrawlerModel(a, &fallback, scene->previewTime, 0.0f, &accent);
        }
    EndMode3D();
}

static bool EnsureStickerTarget(MenuScene *scene, int width, int height)
{
    if (!scene || width <= 0 || height <= 0 || scene->stickerShader.id == 0)
        return false;
    if (scene->stickerTarget.id > 0 &&
        scene->stickerWidth == width && scene->stickerHeight == height)
        return true;
    if (scene->stickerTarget.id > 0)
        UnloadRenderTexture(scene->stickerTarget);
    scene->stickerTarget = LoadRenderTexture(width, height);
    scene->stickerWidth = width;
    scene->stickerHeight = height;
    scene->stickerReady = scene->stickerTarget.id > 0 &&
                          scene->stickerTarget.texture.id > 0;
    if (scene->stickerReady)
    {
        SetTextureFilter(scene->stickerTarget.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(scene->stickerTarget.texture, TEXTURE_WRAP_CLAMP);
    }
    else TraceLog(LOG_WARNING,
                  "MENU: sticker target allocation failed; using direct preview");
    return scene->stickerReady;
}

void MenuSceneRenderBrawler(MenuScene *scene, const App *app, BrawlerClass candidate,
                            AppScreen screen, int width, int height)
{
    if (!EnsureStickerTarget(scene, width, height)) return;
    BeginTextureMode(scene->stickerTarget);
        ClearBackground(BLANK);
        DrawBrawlerRaw(scene, app, candidate, screen, 1.0f);
    EndTextureMode();
}

void MenuSceneCompositeBrawler(MenuScene *scene, const App *app,
                               BrawlerClass candidate, AppScreen screen,
                               float entranceScale)
{
    if (!scene || !scene->stickerReady || scene->stickerTarget.texture.id == 0)
    {
        DrawBrawlerRaw(scene, app, candidate, screen, entranceScale);
        return;
    }

    float resolution[2] = {
        (float)scene->stickerWidth, (float)scene->stickerHeight
    };
    float inner = fmaxf(2.5f, scene->stickerWidth/590.0f);
    float outer = fmaxf(8.0f, scene->stickerWidth/168.0f);
    float ink[4] = { 7.0f/255.0f, 16.0f/255.0f, 25.0f/255.0f, 1.0f };
    float paper[4] = { 1.0f, 247.0f/255.0f, 219.0f/255.0f, 1.0f };
    SetShaderValue(scene->stickerShader, scene->stickerResolutionLoc,
                   resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(scene->stickerShader, scene->stickerInnerLoc,
                   &inner, SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene->stickerShader, scene->stickerOuterLoc,
                   &outer, SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene->stickerShader, scene->stickerInkLoc,
                   ink, SHADER_UNIFORM_VEC4);
    SetShaderValue(scene->stickerShader, scene->stickerPaperLoc,
                   paper, SHADER_UNIFORM_VEC4);
    BeginShaderMode(scene->stickerShader);
        float width = GetScreenWidth()*entranceScale;
        float height = GetScreenHeight()*entranceScale;
        DrawTexturePro(scene->stickerTarget.texture,
                       (Rectangle){ 0, 0, (float)scene->stickerWidth,
                                    -(float)scene->stickerHeight },
                       (Rectangle){ (GetScreenWidth() - width)*0.5f,
                                    (GetScreenHeight() - height)*0.5f,
                                    width, height },
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndShaderMode();
}
