#include "ui_skin.h"

#include "raymath.h"
#include <math.h>
#include <string.h>

static UiSkinTexture LoadSkinTexture(const char *path)
{
    UiSkinTexture result = { 0 };
    result.texture = LoadTexture(path);
    result.ready = result.texture.id > 0 &&
                   result.texture.width > 0 && result.texture.height > 0;
    if (result.ready)
        SetTextureFilter(result.texture, TEXTURE_FILTER_BILINEAR);
    else
        TraceLog(LOG_WARNING, "UI skin: could not load %s; using geometry fallback", path);
    return result;
}

bool UiSkinLoad(UiSkin *skin)
{
    if (!skin) return false;
    memset(skin, 0, sizeof(*skin));
    skin->panel = LoadSkinTexture(
        "resources/ui/kenney_scifi/runtime/panel_rectangle_screws.png");
    skin->button = LoadSkinTexture(
        "resources/ui/kenney_scifi/runtime/button_rectangle_depth.png");
    skin->featurePanel = LoadSkinTexture(
        "resources/ui/kenney_scifi/runtime/feature_panel_notch.png");
    skin->progress = LoadSkinTexture(
        "resources/ui/kenney_scifi/runtime/bar_square_large.png");
    skin->progressShadow = LoadSkinTexture(
        "resources/ui/kenney_scifi/runtime/bar_shadow_square_large.png");
    skin->orbitalRing = LoadSkinTexture(
        "resources/ui/scifi_interface/runtime/orbital_ring.png");
    skin->radarDisc = LoadSkinTexture(
        "resources/ui/scifi_interface/runtime/radar_disc.png");
    skin->complete = skin->panel.ready && skin->button.ready &&
                     skin->featurePanel.ready && skin->progress.ready &&
                     skin->progressShadow.ready && skin->orbitalRing.ready &&
                     skin->radarDisc.ready;
    return skin->complete;
}

static void UnloadSkinTexture(UiSkinTexture *resource)
{
    if (resource->ready) UnloadTexture(resource->texture);
    *resource = (UiSkinTexture){ 0 };
}

void UiSkinUnload(UiSkin *skin)
{
    if (!skin) return;
    UnloadSkinTexture(&skin->panel);
    UnloadSkinTexture(&skin->button);
    UnloadSkinTexture(&skin->featurePanel);
    UnloadSkinTexture(&skin->progress);
    UnloadSkinTexture(&skin->progressShadow);
    UnloadSkinTexture(&skin->orbitalRing);
    UnloadSkinTexture(&skin->radarDisc);
    memset(skin, 0, sizeof(*skin));
}

NPatchInfo UiSkinNinePatchInfo(int width, int height,
                               int left, int top, int right, int bottom)
{
    NPatchInfo patch = {
        .source = { 0, 0, (float)width, (float)height },
        .left = left,
        .top = top,
        .right = right,
        .bottom = bottom,
        .layout = NPATCH_NINE_PATCH
    };
    return patch;
}

static void DrawPatch(UiSkinTexture resource, Rectangle bounds, Color tint,
                      int horizontalInset, int verticalInset)
{
    NPatchInfo patch = UiSkinNinePatchInfo(
        resource.texture.width, resource.texture.height,
        horizontalInset, verticalInset, horizontalInset, verticalInset);
    DrawTextureNPatch(resource.texture, patch, bounds, (Vector2){ 0, 0 }, 0, tint);
}

static void DrawHardwareEdge(Rectangle bounds, Color edge)
{
    float thickness = fmaxf(1.0f, fminf(bounds.width, bounds.height)*0.016f);
    DrawRectangleLinesEx(bounds, thickness, edge);
}

bool UiSkinDrawPanel(const UiSkin *skin, Rectangle bounds, Color fill,
                     Color edge, bool raised, bool feature)
{
    if (!skin || fill.a == 0) return false;
    UiSkinTexture resource =
        feature && skin->featurePanel.ready ? skin->featurePanel : skin->panel;
    if (!resource.ready) return false;

    if (raised && skin->panel.ready)
    {
        Rectangle shadow = bounds;
        shadow.x += fmaxf(2.0f, bounds.height*0.035f);
        shadow.y += fmaxf(3.0f, bounds.height*0.055f);
        DrawPatch(skin->panel, shadow, (Color){ 0, 3, 10, 190 }, 24, 24);
    }
    DrawPatch(resource, bounds, fill, feature ? 28 : 24, feature ? 34 : 24);
    DrawHardwareEdge(bounds, edge);
    return true;
}

bool UiSkinDrawButton(const UiSkin *skin, Rectangle bounds, Color fill,
                      Color edge, bool raised)
{
    if (!skin || !skin->button.ready || fill.a == 0) return false;
    if (raised && skin->panel.ready)
    {
        Rectangle shadow = bounds;
        shadow.x += fmaxf(2.0f, bounds.height*0.035f);
        shadow.y += fmaxf(3.0f, bounds.height*0.055f);
        DrawPatch(skin->panel, shadow, (Color){ 0, 3, 10, 190 }, 24, 24);
    }
    DrawPatch(skin->button, bounds, fill, 24, 24);
    DrawHardwareEdge(bounds, edge);
    return true;
}

static void DrawProgressSlice(const UiSkin *skin, Rectangle bounds,
                              float value, Color track, Color fill)
{
    DrawPatch(skin->progress, bounds, track, 20, 16);
    if (value <= 0.0f) return;

    int clipX = (int)floorf(bounds.x);
    int clipY = (int)floorf(bounds.y);
    int clipWidth = (int)ceilf(bounds.width*Clamp(value, 0.0f, 1.0f));
    int clipHeight = (int)ceilf(bounds.height);
    if (clipWidth <= 0 || clipHeight <= 0) return;
    BeginScissorMode(clipX, clipY, clipWidth, clipHeight);
    DrawPatch(skin->progress, bounds, fill, 20, 16);
    EndScissorMode();
}

bool UiSkinDrawProgress(const UiSkin *skin, Rectangle bounds, float value,
                        Color track, Color fill, bool segmented, int segments,
                        float gap)
{
    if (!skin || !skin->progress.ready || bounds.width <= 0 || bounds.height <= 0)
        return false;
    value = Clamp(value, 0.0f, 1.0f);

    if (skin->progressShadow.ready)
    {
        Rectangle shadow = bounds;
        shadow.y += fmaxf(1.0f, bounds.height*0.12f);
        DrawPatch(skin->progressShadow, shadow, WHITE, 20, 16);
    }

    if (!segmented || segments <= 1)
    {
        DrawProgressSlice(skin, bounds, value, track, fill);
        return true;
    }

    float part = (bounds.width - gap*(segments - 1))/segments;
    if (part <= 0.0f) return false;
    for (int i = 0; i < segments; i++)
    {
        float amount = Clamp(value*segments - i, 0.0f, 1.0f);
        Rectangle segment = {
            bounds.x + i*(part + gap), bounds.y, part, bounds.height
        };
        DrawProgressSlice(skin, segment, amount, track, fill);
    }
    return true;
}

bool UiSkinDrawDecoration(const UiSkin *skin, UiDecoration decoration,
                          Rectangle bounds, Color tint)
{
    if (!skin) return false;
    UiSkinTexture resource = decoration == UI_DECORATION_RADAR_DISC
        ? skin->radarDisc : skin->orbitalRing;
    if (!resource.ready) return false;
    Rectangle source = {
        0, 0, (float)resource.texture.width, (float)resource.texture.height
    };
    DrawTexturePro(resource.texture, source, bounds, (Vector2){ 0, 0 }, 0, tint);
    return true;
}
