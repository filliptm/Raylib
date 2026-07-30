#include "vfx.h"

#include "environment.h"
#include "render_state.h"
#include "raymath.h"
#include "rlgl.h"

#include <math.h>

static int AllocateInstance(PresentationState *presentation, int priority)
{
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
        if (!presentation->vfx[i].active) return i;

    // Cosmetic layers are allowed to degrade gracefully. Replace the oldest layer
    // only when the new recipe is at least as important; gameplay telegraphs live
    // outside this pool and therefore cannot be evicted.
    int candidate = -1;
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
    {
        const VfxInstance *instance = &presentation->vfx[i];
        if (instance->priority > priority) continue;
        if (candidate < 0 ||
            instance->priority < presentation->vfx[candidate].priority ||
            (instance->priority == presentation->vfx[candidate].priority &&
             instance->age > presentation->vfx[candidate].age))
            candidate = i;
    }
    return candidate;
}

int VfxSpawnEvent(PresentationState *presentation, const GameEvent *event)
{
    if (!presentation || !event || event->type != GAME_EVENT_VFX) return 0;
    const VfxRecipeDefinition *recipe = VfxCatalogGet(event->vfxId);
    if (!recipe) return 0;

    int spawned = 0;
    for (int layer = 0; layer < recipe->layerCount; layer++)
    {
        int slot = AllocateInstance(presentation, recipe->priority);
        if (slot < 0)
        {
            presentation->droppedVfx++;
            continue;
        }

        VfxInstance *instance = &presentation->vfx[slot];
        *instance = (VfxInstance){
            .position = event->position,
            .endPosition = event->endPosition,
            .eventColor = event->color.a > 0 ? event->color : WHITE,
            .effect = event->vfxId,
            .angle = event->angle,
            .size = event->size > 0.001f ? event->size : recipe->defaultSize,
            .depthBias = (slot%4)*0.002f,
            .layerIndex = layer,
            .priority = recipe->priority,
            .sourceBrawler = event->sourceBrawler,
            .targetBrawler = event->targetBrawler,
            .startSocket = event->startSocket,
            .endSocket = event->endSocket,
            .active = true
        };
        spawned++;
    }
    return spawned;
}

int VfxSpawnPreview(PresentationState *presentation, VfxEffectId effect,
                    Vector3 position, Vector3 endPosition, float angle,
                    float size, Color color)
{
    GameEvent event = {
        .type = GAME_EVENT_VFX,
        .position = position,
        .endPosition = endPosition,
        .color = color,
        .angle = angle,
        .size = size,
        .vfxId = effect,
        .sourceBrawler = -1,
        .targetBrawler = -1
    };
    return VfxSpawnEvent(presentation, &event);
}

int VfxFrameForLayer(const VfxLayerDefinition *layer, float age)
{
    if (!layer || age < layer->delay || layer->frameCount <= 0) return -1;
    float localAge = age - layer->delay;
    int offset = (int)floorf(localAge*layer->framesPerSecond);
    if (layer->loop)
        offset %= layer->frameCount;
    else if (offset >= layer->frameCount)
        offset = layer->frameCount - 1;
    if (offset < 0) offset = 0;
    return layer->firstFrame + offset;
}

void VfxUpdate(PresentationState *presentation, float dt)
{
    if (!presentation || dt < 0.0f) return;
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
    {
        VfxInstance *instance = &presentation->vfx[i];
        if (!instance->active) continue;
        const VfxRecipeDefinition *recipe = VfxCatalogGet(instance->effect);
        if (!recipe || instance->layerIndex < 0 ||
            instance->layerIndex >= recipe->layerCount)
        {
            instance->active = false;
            continue;
        }
        const VfxLayerDefinition *layer = &recipe->layers[instance->layerIndex];
        instance->age += dt;
        if (instance->age >= layer->delay + layer->duration)
            instance->active = false;
    }
}

int VfxActiveCount(const PresentationState *presentation)
{
    if (!presentation) return 0;
    int count = 0;
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
        if (presentation->vfx[i].active) count++;
    return count;
}

int VfxLoadedAtlasCount(const Assets *assets)
{
    if (!assets) return 0;
    int count = 0;
    for (int i = 0; i < VFX_ATLAS_COUNT; i++)
        if (assets->vfxAtlasesOk[i]) count++;
    return count;
}

static Vector3 ResolveSocket(const PresentationState *presentation, int brawler,
                             VfxSocket socket, Vector3 fallback)
{
    if (!presentation || brawler < 0 || brawler >= MAX_BRAWLERS ||
        socket <= VFX_SOCKET_NONE || socket >= VFX_SOCKET_COUNT)
        return fallback;
    const CharacterSocketPose *pose = &presentation->sockets[brawler];
    return pose->valid[socket] ? pose->positions[socket] : fallback;
}

static Color LayerColor(const VfxInstance *instance,
                        const VfxLayerDefinition *layer,
                        float alpha, bool reducedMotion)
{
    Color color = layer->useEventColor ? instance->eventColor : layer->color;
    float motionAlpha = reducedMotion ? 0.72f : 1.0f;
    float scaled = color.a*Clamp(alpha*motionAlpha, 0.0f, 1.0f);
    color.a = (unsigned char)Clamp(scaled, 0.0f, 255.0f);
    return color;
}

static Rectangle FrameRectangle(const Texture2D texture, VfxAtlasId atlas, int frame)
{
    int columns = 1, rows = 1, frames = 1;
    VfxAtlasGrid(atlas, &columns, &rows, &frames);
    if (frame < 0) frame = 0;
    if (frame >= frames) frame = frames - 1;
    float width = texture.width/(float)columns;
    float height = texture.height/(float)rows;
    // Address texel centers rather than atlas-cell borders. With bilinear filtering,
    // exact border UVs blend the final texel with the adjacent effect frame and can
    // reveal a hard rectangular fringe around otherwise transparent art.
    const float inset = 0.5f;
    return (Rectangle){
        (frame%columns)*width + inset,
        (frame/columns)*height + inset,
        width - inset*2.0f,
        height - inset*2.0f
    };
}

static void DrawGroundFrame(Texture2D texture, Rectangle source, Vector3 position,
                            float angle, float size, Color color)
{
    float half = size*0.5f;
    Vector3 right = { cosf(angle), 0.0f, -sinf(angle) };
    Vector3 forward = { sinf(angle), 0.0f, cosf(angle) };
    Vector3 r = Vector3Scale(right, half);
    Vector3 f = Vector3Scale(forward, half);
    Vector3 p0 = Vector3Subtract(Vector3Subtract(position, r), f);
    Vector3 p1 = Vector3Add(Vector3Subtract(position, f), r);
    Vector3 p2 = Vector3Add(Vector3Add(position, r), f);
    Vector3 p3 = Vector3Add(Vector3Subtract(position, r), f);

    float u0 = source.x/texture.width;
    float v0 = source.y/texture.height;
    float u1 = (source.x + source.width)/texture.width;
    float v1 = (source.y + source.height)/texture.height;

    rlSetTexture(texture.id);
    rlDisableBackfaceCulling();
    rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(u0, v0); rlVertex3f(p0.x, p0.y, p0.z);
        rlTexCoord2f(u1, v0); rlVertex3f(p1.x, p1.y, p1.z);
        rlTexCoord2f(u1, v1); rlVertex3f(p2.x, p2.y, p2.z);
        rlTexCoord2f(u0, v1); rlVertex3f(p3.x, p3.y, p3.z);
    rlEnd();
    rlEnableBackfaceCulling();
    rlSetTexture(0);
}

static void DrawBeamFrame(Texture2D texture, Rectangle source,
                          const VfxInstance *instance, float width,
                          float rotation, Color color, Camera3D camera)
{
    Vector3 start = instance->position;
    Vector3 end = instance->endPosition;
    float length = Vector3Distance(start, end);
    if (length < 0.05f) return;

    Vector3 middle = Vector3Lerp(start, end, 0.5f);
    Vector2 startScreen = GetWorldToScreen(start, camera);
    Vector2 endScreen = GetWorldToScreen(end, camera);
    float screenAngle = atan2f(endScreen.y - startScreen.y,
                              endScreen.x - startScreen.x)*RAD2DEG;

    DrawBillboardPro(camera, texture, source, middle, (Vector3){ 0, 1, 0 },
                     (Vector2){ length, width },
                     (Vector2){ length*0.5f, width*0.5f },
                     screenAngle + rotation, color);
}

static void DrawInstance(const VfxInstance *sourceInstance,
                         const PresentationState *presentation,
                         const Assets *assets, Camera3D camera,
                         bool reducedMotion)
{
    VfxInstance resolved = *sourceInstance;
    resolved.position = ResolveSocket(
        presentation, sourceInstance->sourceBrawler,
        sourceInstance->startSocket, sourceInstance->position);
    resolved.endPosition = ResolveSocket(
        presentation, sourceInstance->targetBrawler,
        sourceInstance->endSocket, sourceInstance->endPosition);
    if (sourceInstance->sourceBrawler >= 0 &&
        (sourceInstance->targetBrawler < 0 ||
         sourceInstance->endSocket == VFX_SOCKET_NONE))
        resolved.endPosition = Vector3Add(
            resolved.endPosition,
            Vector3Subtract(resolved.position, sourceInstance->position));
    const VfxInstance *instance = &resolved;
    const VfxRecipeDefinition *recipe = VfxCatalogGet(instance->effect);
    if (!recipe || instance->layerIndex < 0 ||
        instance->layerIndex >= recipe->layerCount) return;
    const VfxLayerDefinition *layer = &recipe->layers[instance->layerIndex];
    if (layer->atlas < 0 || layer->atlas >= VFX_ATLAS_COUNT ||
        !assets->vfxAtlasesOk[layer->atlas]) return;

    float localAge = instance->age - layer->delay;
    if (localAge < 0.0f || layer->duration <= 0.0f) return;
    float progress = Clamp(localAge/layer->duration, 0.0f, 1.0f);
    float scale = Lerp(layer->startScale, layer->endScale, progress)*
                  instance->size*VFX_RENDER_SCALE;
    float alpha = Lerp(layer->startAlpha, layer->endAlpha, progress);
    float rotation = layer->rotationDegrees + layer->rotationSpeed*localAge;
    int frame = VfxFrameForLayer(layer, instance->age);
    if (frame < 0 || scale <= 0.001f || alpha <= 0.001f) return;

    Texture2D texture = assets->vfxAtlases[layer->atlas];
    Rectangle source = FrameRectangle(texture, layer->atlas, frame);
    Color color = LayerColor(instance, layer, alpha, reducedMotion);

    Vector3 anchor = layer->anchor == VFX_ANCHOR_END
                   ? instance->endPosition : instance->position;
    if (layer->orientation == VFX_ORIENT_GROUND)
    {
        anchor.y = ARENA_DECAL_Y + layer->yOffset + instance->depthBias;
        DrawGroundFrame(texture, source, anchor,
                        instance->angle + rotation*DEG2RAD, scale, color);
    }
    else if (layer->orientation == VFX_ORIENT_BEAM)
    {
        DrawBeamFrame(texture, source, instance, scale, rotation, color, camera);
    }
    else
    {
        anchor.y += layer->yOffset;
        DrawBillboardPro(camera, texture, source, anchor, (Vector3){ 0, 1, 0 },
                         (Vector2){ scale, scale },
                         (Vector2){ scale*0.5f, scale*0.5f },
                         rotation, color);
    }
}

static float DistanceSquared(Vector3 a, Vector3 b)
{
    float x = a.x - b.x;
    float y = a.y - b.y;
    float z = a.z - b.z;
    return x*x + y*y + z*z;
}

void VfxDraw(const PresentationState *presentation, const Assets *assets,
             Camera3D camera, bool reducedMotion)
{
    if (!presentation || !assets) return;

    int alpha[MAX_VFX_INSTANCES];
    int alphaCount = 0;
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
    {
        const VfxInstance *instance = &presentation->vfx[i];
        if (!instance->active) continue;
        const VfxRecipeDefinition *recipe = VfxCatalogGet(instance->effect);
        if (!recipe || instance->layerIndex < 0 ||
            instance->layerIndex >= recipe->layerCount) continue;
        const VfxLayerDefinition *layer = &recipe->layers[instance->layerIndex];
        if (layer->blend != VFX_BLEND_ALPHA) continue;
        alpha[alphaCount++] = i;
    }

    // Alpha smoke needs a stable, coarse back-to-front order. The additive pass does
    // not require sorting and is kept separate so state is restored predictably.
    for (int i = 1; i < alphaCount; i++)
    {
        int value = alpha[i];
        float distance = DistanceSquared(
            presentation->vfx[value].position, camera.position);
        int j = i - 1;
        while (j >= 0 &&
               DistanceSquared(presentation->vfx[alpha[j]].position,
                               camera.position) < distance)
        {
            alpha[j + 1] = alpha[j];
            j--;
        }
        alpha[j + 1] = value;
    }

    if (alphaCount > 0)
    {
        BeginBlendMode(BLEND_ALPHA);
        RenderBeginNoDepthWrite();
        for (int i = 0; i < alphaCount; i++)
            DrawInstance(&presentation->vfx[alpha[i]], presentation, assets,
                         camera, reducedMotion);
        RenderEndNoDepthWrite();
        EndBlendMode();
    }

    BeginBlendMode(BLEND_ADDITIVE);
    RenderBeginNoDepthWrite();
    for (int i = 0; i < MAX_VFX_INSTANCES; i++)
    {
        const VfxInstance *instance = &presentation->vfx[i];
        if (!instance->active) continue;
        const VfxRecipeDefinition *recipe = VfxCatalogGet(instance->effect);
        if (!recipe || instance->layerIndex < 0 ||
            instance->layerIndex >= recipe->layerCount) continue;
        if (recipe->layers[instance->layerIndex].blend != VFX_BLEND_ADDITIVE)
            continue;
        DrawInstance(instance, presentation, assets, camera, reducedMotion);
    }
    RenderEndNoDepthWrite();
    EndBlendMode();
}
