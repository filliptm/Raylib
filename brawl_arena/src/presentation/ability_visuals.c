#include "ability_visuals.h"

#include "attack_content.h"

#include "arena.h"
#include "brawler.h"
#include "content_catalog.h"
#include "environment.h"
#include "render_state.h"
#include "weapons.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stddef.h>

static Matrix MakeTransform(Vector3 scale, Vector3 position)
{
    Matrix matrix = MatrixScale(scale.x, scale.y, scale.z);
    return MatrixMultiply(matrix,
                          MatrixTranslate(position.x, position.y, position.z));
}

static float Scatter(int a, int b, int c)
{
    int hash = a*374761393 + b*668265263 + c*1442695040;
    hash = (hash ^ (hash >> 13))*1274126177;
    return (float)((hash ^ (hash >> 16)) & 0xFFFFFF)/(float)0xFFFFFF;
}

static void DrawGroundEdge(Vector3 start, Vector3 end, float thickness, Color color)
{
    DrawCylinderEx((Vector3){ start.x, ARENA_PREVIEW_Y, start.z },
                   (Vector3){ end.x, ARENA_PREVIEW_Y, end.z },
                   thickness, thickness, 6, color);
}

static void DrawGroundGlow(Assets *assets, Vector3 position, float radius, Color tint)
{
    Matrix transform = MakeTransform(
        (Vector3){ radius*2.0f, 1.0f, radius*2.0f },
        (Vector3){ position.x, ARENA_DECAL_Y, position.z }
    );
    DrawLit(assets, assets->plane, transform, assets->texGlow, tint,
            (Vector2){ 1.0f, 1.0f }, 1.0f);
}

static float RayGroundDistance(const App *world, Vector3 origin,
                               float angle, float maximum)
{
    for (float distance = 0.6f; distance < maximum; distance += 0.25f)
    {
        float x = origin.x + sinf(angle)*distance;
        float z = origin.z + cosf(angle)*distance;
        if (ArenaSolidAt(&world->session.arena, x, z)) return distance;
    }
    return maximum;
}

static void DrawAimDisc(Vector3 center, float radius, Color fill, Color edge);
static void DrawGroundRing(Vector3 center, float radius, float thickness,
                           Color color);

static void DrawAimCone(const App *world, Vector3 origin, float centerAngle,
                        float halfSpread, float range, Color fill, Color edge)
{
    enum { SEGMENTS = 26 };
    Vector3 points[SEGMENTS + 1];

    for (int i = 0; i <= SEGMENTS; i++)
    {
        float angle = centerAngle - halfSpread +
                      (i/(float)SEGMENTS)*halfSpread*2.0f;
        float distance = RayGroundDistance(world, origin, angle, range);
        points[i] = (Vector3){
            origin.x + sinf(angle)*distance,
            ARENA_PREVIEW_Y,
            origin.z + cosf(angle)*distance
        };
    }

    Vector3 apex = { origin.x, ARENA_PREVIEW_Y, origin.z };
    rlDisableBackfaceCulling();
    for (int i = 0; i < SEGMENTS; i++)
        DrawTriangle3D(apex, points[i], points[i + 1], fill);
    rlEnableBackfaceCulling();

    for (int i = 0; i < SEGMENTS; i++)
        DrawGroundEdge(points[i], points[i + 1], 0.055f, edge);
    DrawGroundEdge(apex, points[0], 0.05f, edge);
    DrawGroundEdge(apex, points[SEGMENTS], 0.05f, edge);
}

static void DrawAimBeam(const App *world, Vector3 origin, float angle,
                        float range, float halfWidth, Color fill, Color edge)
{
    float distance = RayGroundDistance(world, origin, angle, range);
    Vector3 endpoint = {
        origin.x + sinf(angle)*distance,
        ARENA_PREVIEW_Y,
        origin.z + cosf(angle)*distance
    };
    origin.y = ARENA_PREVIEW_Y;

    Vector3 forward = Vector3Subtract(endpoint, origin);
    if (Vector3LengthSqr(forward) < 0.000001f) return;
    forward = Vector3Normalize(forward);
    float perpendicularX = forward.z;
    float perpendicularZ = -forward.x;

    Vector3 nearLeft = {
        origin.x + perpendicularX*halfWidth,
        ARENA_PREVIEW_Y,
        origin.z + perpendicularZ*halfWidth
    };
    Vector3 nearRight = {
        origin.x - perpendicularX*halfWidth,
        ARENA_PREVIEW_Y,
        origin.z - perpendicularZ*halfWidth
    };
    Vector3 farLeft = {
        endpoint.x + perpendicularX*halfWidth,
        ARENA_PREVIEW_Y,
        endpoint.z + perpendicularZ*halfWidth
    };
    Vector3 farRight = {
        endpoint.x - perpendicularX*halfWidth,
        ARENA_PREVIEW_Y,
        endpoint.z - perpendicularZ*halfWidth
    };

    rlDisableBackfaceCulling();
    DrawTriangle3D(nearLeft, nearRight, farRight, fill);
    DrawTriangle3D(nearLeft, farRight, farLeft, fill);
    rlEnableBackfaceCulling();

    DrawGroundEdge(nearLeft, farLeft, 0.055f, edge);
    DrawGroundEdge(nearRight, farRight, 0.055f, edge);
    DrawGroundEdge(farLeft, farRight, 0.055f, edge);
}

static void DrawGrapplePreview(App *world, Brawler *brawler,
                                const AbilityDefinition *ability)
{
    Vector3 direction = {
        sinf(brawler->aimAngle), 0.0f, cosf(brawler->aimAngle)
    };
    Vector3 endpoint = BrawlerGrappleEndpoint(
        &world->session.arena, brawler->position, direction, ability->range);
    float distance = Vector3Distance(brawler->position, endpoint);
    bool valid = distance >= 1.5f;
    bool coverLimited = distance < ability->range - 0.08f;
    float pulse = 0.5f + 0.5f*sinf(world->session.time*8.0f);

    Color rangeColor = { 94, 222, 255, 72 };
    Color fill = valid ? (Color){ 54, 210, 255, 64 }
                       : (Color){ 255, 74, 92, 68 };
    Color edge = !valid ? (Color){ 255, 112, 126, 235 }
               : coverLimited ? (Color){ 255, 202, 92, 230 }
                              : (Color){ 154, 244, 255, 235 };

    // The outer ring communicates the authored maximum even when cover shortens
    // the actual body-safe endpoint.
    DrawGroundRing(brawler->position, ability->range, 0.032f, rangeColor);

    if (distance > 0.001f)
    {
        float angle = atan2f(direction.x, direction.z);
        DrawAimBeam(world, brawler->position, angle, distance,
                    0.12f, fill, edge);

        int pulses = (int)fminf(9.0f, floorf(distance/1.15f));
        for (int marker = 1; marker <= pulses; marker++)
        {
            float travel = fmodf(
                marker/(float)(pulses + 1) +
                world->session.time*0.65f, 1.0f);
            Vector3 point = Vector3Lerp(brawler->position, endpoint, travel);
            point.y = ARENA_PREVIEW_Y + 0.025f;
            DrawSphere(point, 0.045f,
                       (Color){ edge.r, edge.g, edge.b, 190 });
        }
    }

    DrawAimDisc(endpoint, 0.24f, fill, edge);
    DrawGroundRing(endpoint, 0.38f + pulse*0.08f, 0.050f, edge);
}

static void DrawAimDisc(Vector3 center, float radius, Color fill, Color edge)
{
    enum { SEGMENTS = 32 };
    Vector3 middle = { center.x, ARENA_PREVIEW_Y, center.z };
    Vector3 previous = { center.x, ARENA_PREVIEW_Y, center.z + radius };

    rlDisableBackfaceCulling();
    for (int i = 1; i <= SEGMENTS; i++)
    {
        float angle = (i/(float)SEGMENTS)*PI*2.0f;
        Vector3 point = {
            center.x + sinf(angle)*radius,
            ARENA_PREVIEW_Y,
            center.z + cosf(angle)*radius
        };
        DrawTriangle3D(middle, previous, point, fill);
        DrawGroundEdge(previous, point, 0.06f, edge);
        previous = point;
    }
    rlEnableBackfaceCulling();
}

static void DrawGroundRing(Vector3 center, float radius, float thickness,
                           Color color)
{
    enum { SEGMENTS = 40 };
    Vector3 previous = {
        center.x, ARENA_PREVIEW_Y, center.z + radius
    };
    for (int segment = 1; segment <= SEGMENTS; segment++)
    {
        float angle = (segment/(float)SEGMENTS)*PI*2.0f;
        Vector3 point = {
            center.x + sinf(angle)*radius,
            ARENA_PREVIEW_Y,
            center.z + cosf(angle)*radius
        };
        DrawGroundEdge(previous, point, thickness, color);
        previous = point;
    }
}

static void DrawActiveFields(App *world, Assets *assets)
{
    BeginBlendMode(BLEND_ADDITIVE);
    RenderBeginNoDepthWrite();

    for (int i = 0; i < MAX_ABILITY_FIELDS; i++)
    {
        AbilityField *field = &world->session.abilityFields[i];
        if (!field->active) continue;
        // Authored documents own this field's look entirely.
        if (AttackAuthored(&world->content, field->abilityIndex)) continue;

        if (field->type == ABILITY_FIELD_MINE)
        {
            float armProgress = field->growTime > 0.0f
                              ? Clamp(1.0f - field->armTime/field->growTime,
                                      0.0f, 1.0f)
                              : 1.0f;
            float pulse = 0.5f + 0.5f*sinf(
                world->session.time*(field->armed ? 9.0f : 18.0f) + i);
            Color trigger = field->armed
                          ? (Color){ 255, 108, 54,
                                     (unsigned char)(145 + 80*pulse) }
                          : (Color){ 255, 194, 76,
                                     (unsigned char)(70 + 100*armProgress) };
            DrawGroundRing(field->position, field->triggerRadius,
                           field->armed ? 0.075f : 0.045f, trigger);
            DrawGroundRing(field->position, field->radius, 0.028f,
                           (Color){ 255, 154, 76,
                                    (unsigned char)(34 + 32*pulse) });
            DrawGroundGlow(assets, field->position,
                           0.50f + 0.10f*pulse,
                           (Color){ trigger.r, trigger.g, trigger.b,
                                    (unsigned char)(50 + 36*pulse) });
            continue;
        }

        if (field->maxLife <= 0.0f) continue;

        float age = field->maxLife - field->life;
        float progress = Clamp(age/field->maxLife, 0.0f, 1.0f);
        float fade = Clamp(field->life/fminf(field->maxLife, 0.32f), 0.0f, 1.0f);

        if (field->type == ABILITY_FIELD_RAIN)
        {
            float growth = field->growTime > 0.0f
                         ? field->growTime
                         : field->maxLife;
            float radius = field->radius*Clamp(age/growth, 0.15f, 1.0f);
            Color fill = { 65, 218, 190, (unsigned char)(44*fade) };
            Color edge = { 116, 255, 218, (unsigned char)(185*fade) };
            DrawAimDisc(field->position, radius, fill, edge);
            DrawGroundGlow(assets, field->position, radius,
                           (Color){ 62, 226, 190, (unsigned char)(54*fade) });

            for (int drop = 0; drop < 22; drop++)
            {
                float theta = Scatter(i + 41, drop, 3)*PI*2.0f;
                float radial = sqrtf(Scatter(i + 41, drop, 5))*radius;
                float fall = fmodf(world->session.time*2.8f +
                                   Scatter(i + 41, drop, 7), 1.0f);
                float top = 3.25f - fall*2.75f;
                Vector3 start = {
                    field->position.x + sinf(theta)*radial,
                    top,
                    field->position.z + cosf(theta)*radial
                };
                Vector3 end = {
                    start.x - 0.04f,
                    fmaxf(0.12f, top - 0.52f),
                    start.z + 0.03f
                };
                DrawCylinderEx(end, start, 0.018f, 0.027f, 5,
                               (Color){ 134, 244, 255,
                                        (unsigned char)(150*fade) });
            }
        }
        else if (field->type == ABILITY_FIELD_SOUND_WAVE)
        {
            Color fill = {
                90, 222, 255,
                (unsigned char)(28*(1.0f - progress))
            };
            Color edge = {
                142, 244, 255,
                (unsigned char)(220*(1.0f - progress))
            };
            float travel = field->range*
                           (1.0f - powf(1.0f - progress, 2.0f));
            DrawAimCone(world, field->position, field->angle,
                        field->spread*0.5f, travel, fill, edge);

            for (int band = 0; band < 3; band++)
            {
                float radius = travel - band*0.72f;
                if (radius <= 0.1f) continue;

                enum { SEGMENTS = 24 };
                Vector3 previous = { 0 };
                for (int segment = 0; segment <= SEGMENTS; segment++)
                {
                    float angle = field->angle - field->spread*0.5f +
                                  (segment/(float)SEGMENTS)*field->spread;
                    Vector3 point = {
                        field->position.x + sinf(angle)*radius,
                        ARENA_PREVIEW_Y + 0.02f + band*0.018f,
                        field->position.z + cosf(angle)*radius
                    };
                    if (segment > 0)
                        DrawGroundEdge(previous, point,
                                       0.10f - band*0.018f, edge);
                    previous = point;
                }
            }
        }
    }

    for (int i = 0; i < world->session.brawlerCount; i++)
    {
        Brawler *brawler = &world->session.brawlers[i];
        const StatusEffect *status = NULL;
        for (int effect = 0; effect < MAX_STATUS_EFFECTS; effect++)
        {
            if (brawler->statuses[effect].active)
            {
                status = &brawler->statuses[effect];
                break;
            }
        }
        if (!brawler->alive || !brawler->visible || !status) continue;
        if (AttackAuthored(&world->content, status->abilityIndex)) continue;

        bool healing = brawler->team == status->sourceTeam;
        Color aura = healing ? (Color){ 74, 255, 176, 105 }
                             : (Color){ 255, 82, 156, 105 };
        float pulse = 0.92f + 0.10f*sinf(world->session.time*12.0f);
        DrawGroundGlow(assets, brawler->position, 1.15f*pulse, aura);
    }

    RenderEndNoDepthWrite();
    EndBlendMode();
}

static void DrawActiveGrapples(App *world)
{
    BeginBlendMode(BLEND_ADDITIVE);
    RenderBeginNoDepthWrite();

    for (int i = 0; i < world->session.brawlerCount; i++)
    {
        Brawler *brawler = &world->session.brawlers[i];
        if (!brawler->alive || !brawler->visible ||
            !BrawlerIsGrappling(brawler))
            continue;

        Vector3 hand = {
            brawler->position.x, 1.10f, brawler->position.z
        };
        if (world->presentation.sockets[i].valid[VFX_SOCKET_RIGHT_HAND])
            hand = world->presentation.sockets[i]
                .positions[VFX_SOCKET_RIGHT_HAND];
        Vector3 anchor = {
            brawler->grappleAnchor.x, 0.24f, brawler->grappleAnchor.z
        };
        Vector3 hook = anchor;
        const AbilityDefinition *ability =
            ContentAbility(&world->content, brawler->grappleAbility);
        if (brawler->grappleDelayTimer > 0.0f && ability &&
            ability->behavior == ABILITY_BEHAVIOR_GRAPPLE &&
            ability->data.grapple.launchDelay > 0.0001f)
        {
            float progress = Clamp(
                1.0f - brawler->grappleDelayTimer/
                       ability->data.grapple.launchDelay,
                0.0f, 1.0f);
            progress = 1.0f - (1.0f - progress)*(1.0f - progress);
            hook = Vector3Lerp(hand, anchor, progress);
        }
        Color cable = { 88, 224, 255, 225 };
        if (Vector3Distance(hand, hook) > 0.03f)
            DrawCylinderEx(hand, hook, 0.045f, 0.070f, 7, cable);

        float length = Vector3Distance(hand, hook);
        int nodes = (int)fminf(12.0f, ceilf(length/0.75f));
        for (int node = 1; node < nodes; node++)
        {
            float travel = fmodf(
                node/(float)nodes + world->session.time*2.2f, 1.0f);
            Vector3 point = Vector3Lerp(hand, hook, travel);
            DrawSphere(point, 0.055f,
                       (Color){ 198, 250, 255, 210 });
        }
        DrawSphere(hook, 0.14f,
                   (Color){ 126, 238, 255, 230 });
    }

    RenderEndNoDepthWrite();
    EndBlendMode();
}

static void DrawActiveShields(App *world)
{
    BeginBlendMode(BLEND_ADDITIVE);
    RenderBeginNoDepthWrite();

    for (int i = 0; i < world->session.brawlerCount; i++)
    {
        Brawler *brawler = &world->session.brawlers[i];
        if (!brawler->alive || !brawler->visible || !brawler->shieldActive)
            continue;
        const AbilityDefinition *ability =
            ContentAbility(&world->content, brawler->shieldAbility);
        if (!ability || ability->behavior != ABILITY_BEHAVIOR_SHIELD) continue;

        float charge = ability->data.shield.capacity > 0
                     ? Clamp(brawler->shieldCharge/
                             ability->data.shield.capacity, 0.0f, 1.0f)
                     : 0.0f;
        float lowCharge = Clamp((0.28f - charge)/0.28f, 0.0f, 1.0f);
        float instability =
            lowCharge*(0.035f*sinf(world->session.time*41.0f + i*1.7f));
        float pulse =
            1.0f + 0.018f*sinf(world->session.time*6.5f + i) + instability;
        float radius = 1.65f*pulse;
        Vector3 center = {
            brawler->position.x, 1.45f, brawler->position.z
        };
        unsigned char flicker = (unsigned char)(
            168.0f + 66.0f*charge -
            lowCharge*48.0f*(0.5f + 0.5f*sinf(world->session.time*53.0f)));
        Color shell = { 62, 205, 255,
                        (unsigned char)(22 + 30*charge) };
        Color cage = { 156, 242, 255, flicker };

        rlDisableBackfaceCulling();
        DrawSphere(center, radius, shell);
        DrawSphereWires(center, radius, 10, 20, cage);
        DrawCircle3D(center, radius*1.012f,
                     (Vector3){ 1.0f, 0.0f, 0.0f },
                     world->session.time*34.0f, cage);
        DrawCircle3D(center, radius*1.018f,
                     (Vector3){ 0.0f, 0.0f, 1.0f },
                     -world->session.time*27.0f,
                     (Color){ 92, 220, 255,
                              (unsigned char)(flicker*0.72f) });
        rlEnableBackfaceCulling();
    }

    RenderEndNoDepthWrite();
    EndBlendMode();
}

static void FinishAimPass(void)
{
    RenderEndNoDepthWrite();
    EndBlendMode();
}

static void DrawAimPreview(App *world, Assets *assets)
{
    Brawler *brawler =
        &world->session.brawlers[world->session.playerIdx];
    if (!brawler->alive || brawler->dashTimer > 0.0f ||
        brawler->shieldActive)
        return;
    if (!world->controller.charging && !world->controller.aimingSuper &&
        !world->controller.aimingSecondary)
        return;

    bool super = world->controller.aimingSuper;
    bool secondary = world->controller.aimingSecondary;
    const AbilityDefinition *ability =
        secondary ? ContentSecondaryAbility(&world->content, brawler->cls)
        : super ? ContentSuperAbility(&world->content, brawler->cls)
                : ContentMainAbility(&world->content, brawler->cls);
    if (!ability) return;

    BeginBlendMode(BLEND_ADDITIVE);
    RenderBeginNoDepthWrite();

    if (secondary && ability->behavior == ABILITY_BEHAVIOR_GRAPPLE)
    {
        DrawGrapplePreview(world, brawler, ability);
        FinishAimPass();
        return;
    }

    float range = ability->range;
    float spreadDegrees = 0.0f;
    int pellets = 0;
    float radius = ability->radius;
    if (ability->behavior == ABILITY_BEHAVIOR_PROJECTILE ||
        ability->behavior == ABILITY_BEHAVIOR_LOB)
    {
        spreadDegrees = ability->data.projectile.spreadDegrees;
        pellets = ability->data.projectile.pellets;
    }
    else if (ability->behavior == ABILITY_BEHAVIOR_RETURNING)
    {
        pellets = 1;
    }
    else
    {
        spreadDegrees = ability->data.area.spreadDegrees;
    }

    Color fill = super ? (Color){ 255, 206, 92, 62 }
                       : (Color){ 96, 178, 255, 58 };
    Color edge = super ? (Color){ 255, 238, 170, 210 }
                       : (Color){ 176, 224, 255, 205 };
    if (ability->healing > 0)
    {
        fill = (Color){ 70, 244, 166, 58 };
        edge = (Color){ 170, 255, 214, 215 };
    }
    if (super && ability->behavior == ABILITY_BEHAVIOR_SOUND_WAVE)
    {
        fill = (Color){ 75, 215, 255, 56 };
        edge = (Color){ 152, 246, 255, 220 };
    }

    if (!super && ability->behavior == ABILITY_BEHAVIOR_RAIN)
    {
        Vector3 landing =
            WeaponsArcLanding(AppGameContext(world), brawler,
                              world->controller.aimDist);
        DrawAimDisc(landing, ability->radius, fill, edge);
        FinishAimPass();
        return;
    }

    if (super && ability->behavior == ABILITY_BEHAVIOR_SOUND_WAVE)
    {
        DrawAimCone(world, brawler->position, brawler->aimAngle,
                    ability->data.area.spreadDegrees*DEG2RAD*0.5f,
                    ability->range, fill, edge);
        FinishAimPass();
        return;
    }

    if (super && ability->behavior == ABILITY_BEHAVIOR_HEALING_BURST)
    {
        DrawAimDisc(brawler->position, ability->range, fill, edge);
        FinishAimPass();
        return;
    }

    if (super && ability->behavior == ABILITY_BEHAVIOR_DASH)
    {
        float speed = ability->data.dash.speed > 0.0f
                    ? ability->data.dash.speed : world->tune.dashSpeed;
        DrawAimBeam(world, brawler->position, brawler->aimAngle,
                    speed*ability->data.dash.duration,
                    BRAWLER_RADIUS*1.1f, fill, edge);
        FinishAimPass();
        return;
    }

    if (ability->behavior == ABILITY_BEHAVIOR_LOB)
    {
        float aimDistance = Clamp(world->controller.aimDist, 1.5f, range);
        float halfSpread = spreadDegrees*DEG2RAD*0.5f;

        for (int i = 0; i < pellets; i++)
        {
            float amount = pellets == 1 ? 0.5f : i/(float)(pellets - 1);
            float angle = brawler->aimAngle +
                          (amount - 0.5f)*halfSpread*2.0f;
            Vector3 landing = {
                brawler->position.x + sinf(angle)*aimDistance,
                0.0f,
                brawler->position.z + cosf(angle)*aimDistance
            };

            DrawAimDisc(landing, radius, fill, edge);
            if (i == pellets/2)
            {
                Vector3 start = {
                    brawler->position.x,
                    0.8f,
                    brawler->position.z
                };
                float height = Vector3Distance(start, landing)*0.42f + 1.0f;
                for (int point = 1; point <= 16; point++)
                {
                    float progress = point/16.0f;
                    Vector3 position = Vector3Lerp(start, landing, progress);
                    position.y = sinf(progress*PI)*height;
                    if (point%2 == 0)
                        DrawBillboard(world->presentation.camera, assets->texGlow,
                                      position, 0.32f, edge);
                }
            }
        }
        FinishAimPass();
        return;
    }

    if (pellets > 1 && spreadDegrees > 0.5f)
    {
        DrawAimCone(world, brawler->position, brawler->aimAngle,
                    spreadDegrees*DEG2RAD*0.5f, range, fill, edge);
    }
    else
    {
        DrawAimBeam(world, brawler->position, brawler->aimAngle, range,
                    fmaxf(radius*2.2f, 0.30f), fill, edge);
    }
    FinishAimPass();
}

void AbilityVisualsDraw(App *world, Assets *assets)
{
    DrawActiveFields(world, assets);
    DrawActiveGrapples(world);
    DrawActiveShields(world);
    DrawAimPreview(world, assets);
}
