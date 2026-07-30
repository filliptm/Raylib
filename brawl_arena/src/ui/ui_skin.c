#include "ui_skin.h"

#include "raymath.h"
#include <math.h>
#include <string.h>

static float StrokeFor(Rectangle bounds)
{
    return fmaxf(1.25f, fminf(5.0f, bounds.height*0.055f));
}

static float CutFor(Rectangle bounds, bool feature)
{
    float maximum = fminf(bounds.width, bounds.height)*(feature ? 0.20f : 0.14f);
    return fmaxf(3.0f, fminf(feature ? 18.0f : 12.0f, maximum));
}

static void ComicPoints(Rectangle r, float cut, bool feature, Vector2 points[8])
{
    float notch = feature ? fminf(cut*1.75f, r.width*0.14f) : cut;
    points[0] = (Vector2){ r.x + cut, r.y };
    points[1] = (Vector2){ r.x, r.y + cut };
    points[2] = (Vector2){ r.x, r.y + r.height - cut };
    points[3] = (Vector2){ r.x + notch, r.y + r.height };
    points[4] = (Vector2){ r.x + r.width - cut, r.y + r.height };
    points[5] = (Vector2){ r.x + r.width, r.y + r.height - cut };
    points[6] = (Vector2){ r.x + r.width, r.y + cut };
    points[7] = (Vector2){ r.x + r.width - notch, r.y };
}

static void DrawComicShape(Rectangle bounds, Color fill, Color edge,
                           bool raised, bool feature)
{
    float stroke = StrokeFor(bounds);
    float cut = CutFor(bounds, feature);
    Vector2 points[8];

    if (raised)
    {
        Rectangle shadow = bounds;
        shadow.x += stroke*1.45f;
        shadow.y += stroke*1.65f;
        ComicPoints(shadow, cut, feature, points);
        DrawTriangleFan(points, 8, (Color){ 0, 0, 0, 220 });
    }

    ComicPoints(bounds, cut, feature, points);
    if (fill.a > 0) DrawTriangleFan(points, 8, fill);
    for (int i = 0; i < 8; i++)
        DrawLineEx(points[i], points[(i + 1)%8], stroke, edge);

    if (fill.a > 0 && (raised || feature) &&
        bounds.width > stroke*6.0f && bounds.height > stroke*6.0f)
    {
        Rectangle keyline = {
            bounds.x + stroke*1.55f,
            bounds.y + stroke*1.55f,
            bounds.width - stroke*3.1f,
            bounds.height - stroke*3.1f
        };
        ComicPoints(keyline, fmaxf(2.0f, cut - stroke), feature, points);
        Color line = { 255, 247, 219, feature ? 185 : 135 };
        for (int i = 0; i < 8; i++)
            DrawLineEx(points[i], points[(i + 1)%8], fmaxf(1.0f, stroke*0.42f), line);
    }
}

bool UiSkinLoad(UiSkin *skin)
{
    if (!skin) return false;
    *skin = (UiSkin){ .ready = true };
    return true;
}

void UiSkinUnload(UiSkin *skin)
{
    if (skin) memset(skin, 0, sizeof(*skin));
}

bool UiSkinDrawPanel(const UiSkin *skin, Rectangle bounds, Color fill,
                     Color edge, bool raised, bool feature)
{
    if (!skin || !skin->ready) return false;
    DrawComicShape(bounds, fill, edge, raised, feature);
    return true;
}

bool UiSkinDrawButton(const UiSkin *skin, Rectangle bounds, Color fill,
                      Color edge, bool raised)
{
    if (!skin || !skin->ready) return false;
    DrawComicShape(bounds, fill, edge, raised, true);
    return true;
}

bool UiSkinDrawProgress(const UiSkin *skin, Rectangle bounds, float value,
                        Color track, Color fill, bool segmented, int segments,
                        float gap)
{
    if (!skin || !skin->ready || bounds.width <= 0 || bounds.height <= 0)
        return false;
    value = Clamp(value, 0.0f, 1.0f);
    float stroke = fmaxf(1.0f, fminf(3.0f, bounds.height*0.22f));

    DrawRectangleRec(bounds, (Color){ 0, 0, 0, 230 });
    Rectangle inset = {
        bounds.x + stroke, bounds.y + stroke,
        fmaxf(0.0f, bounds.width - stroke*2.0f),
        fmaxf(0.0f, bounds.height - stroke*2.0f)
    };
    DrawRectangleRec(inset, track);

    if (!segmented || segments <= 1)
    {
        Rectangle amount = inset;
        amount.width *= value;
        DrawRectangleRec(amount, fill);
        return true;
    }

    float part = (inset.width - gap*(segments - 1))/segments;
    if (part <= 0.0f) return true;
    for (int i = 0; i < segments; i++)
    {
        float amount = Clamp(value*segments - i, 0.0f, 1.0f);
        DrawRectangleRec((Rectangle){
            inset.x + i*(part + gap), inset.y, part*amount, inset.height
        }, fill);
    }
    return true;
}

static void DrawBurst(Rectangle bounds, Color tint)
{
    Vector2 center = {
        bounds.x + bounds.width*0.5f, bounds.y + bounds.height*0.5f
    };
    float inner = fminf(bounds.width, bounds.height)*0.18f;
    float outer = fmaxf(bounds.width, bounds.height)*0.56f;
    for (int i = 0; i < 24; i++)
    {
        float angle = (float)i*2.0f*PI/24.0f;
        float width = (i%3 == 0 ? 0.060f : 0.027f);
        float length = outer*(0.64f + 0.30f*(float)((i*7)%11)/10.0f);
        Vector2 a = {
            center.x + cosf(angle - width)*inner,
            center.y + sinf(angle - width)*inner
        };
        Vector2 b = {
            center.x + cosf(angle)*length,
            center.y + sinf(angle)*length
        };
        Vector2 c = {
            center.x + cosf(angle + width)*inner,
            center.y + sinf(angle + width)*inner
        };
        DrawTriangle(a, b, c, tint);
    }
}

static void DrawHalftone(Rectangle bounds, Color tint)
{
    float spacing = fmaxf(10.0f, fminf(bounds.width, bounds.height)/18.0f);
    float radius = fmaxf(1.0f, spacing*0.16f);
    BeginScissorMode((int)bounds.x, (int)bounds.y,
                     (int)ceilf(bounds.width), (int)ceilf(bounds.height));
    int row = 0;
    for (float y = bounds.y - spacing; y < bounds.y + bounds.height + spacing; y += spacing)
    {
        float offset = (row++ & 1) ? spacing*0.5f : 0.0f;
        for (float x = bounds.x - spacing; x < bounds.x + bounds.width + spacing;
             x += spacing)
            DrawCircleV((Vector2){ x + offset, y }, radius, tint);
    }
    EndScissorMode();
}

static void DrawSpeedLines(Rectangle bounds, Color tint)
{
    Vector2 origin = { bounds.x, bounds.y + bounds.height*0.5f };
    for (int i = 0; i < 15; i++)
    {
        float t = (float)i/14.0f;
        float y = bounds.y + t*bounds.height;
        float inset = fabsf(t - 0.5f)*bounds.width*0.32f;
        DrawLineEx(origin, (Vector2){ bounds.x + bounds.width - inset, y },
                   fmaxf(1.0f, bounds.height*0.006f), tint);
    }
}

bool UiSkinDrawDecoration(const UiSkin *skin, UiDecoration decoration,
                          Rectangle bounds, Color tint)
{
    if (!skin || !skin->ready) return false;
    if (decoration == UI_DECORATION_HALFTONE) DrawHalftone(bounds, tint);
    else if (decoration == UI_DECORATION_SPEED_LINES) DrawSpeedLines(bounds, tint);
    else DrawBurst(bounds, tint);
    return true;
}

bool UiSkinDrawBackdrop(const UiSkin *skin, Rectangle viewport, Rectangle canvas,
                        Color blue, Color ink, Color yellow, Color red)
{
    if (!skin || !skin->ready) return false;
    DrawRectangleRec(viewport, ink);
    DrawRectangleRec(canvas, blue);

    Color grid = ink;
    grid.a = 125;
    for (int i = 0; i <= 12; i++)
    {
        float x = canvas.x + canvas.width*(float)i/12.0f;
        DrawLineEx((Vector2){ x, canvas.y }, (Vector2){ x, canvas.y + canvas.height },
                   fmaxf(1.0f, canvas.width/640.0f), grid);
    }
    for (int i = 0; i <= 8; i++)
    {
        float y = canvas.y + canvas.height*(float)i/8.0f;
        DrawLineEx((Vector2){ canvas.x, y }, (Vector2){ canvas.x + canvas.width, y },
                   fmaxf(1.0f, canvas.height/400.0f), grid);
    }

    Vector2 leftRed[5] = {
        { canvas.x, canvas.y + canvas.height*0.18f },
        { canvas.x + canvas.width*0.22f, canvas.y + canvas.height*0.38f },
        { canvas.x + canvas.width*0.08f, canvas.y + canvas.height*0.50f },
        { canvas.x + canvas.width*0.24f, canvas.y + canvas.height*0.66f },
        { canvas.x, canvas.y + canvas.height*0.78f }
    };
    DrawTriangleFan(leftRed, 5, red);
    Vector2 rightRed[5] = {
        { canvas.x + canvas.width, canvas.y + canvas.height*0.06f },
        { canvas.x + canvas.width*0.82f, canvas.y + canvas.height*0.30f },
        { canvas.x + canvas.width*0.91f, canvas.y + canvas.height*0.46f },
        { canvas.x + canvas.width*0.80f, canvas.y + canvas.height*0.64f },
        { canvas.x + canvas.width, canvas.y + canvas.height*0.72f }
    };
    DrawTriangleFan(rightRed, 5, red);

    DrawTriangle((Vector2){ canvas.x, canvas.y + canvas.height*0.62f },
                 (Vector2){ canvas.x + canvas.width*0.22f, canvas.y + canvas.height*0.52f },
                 (Vector2){ canvas.x, canvas.y + canvas.height*0.72f }, yellow);
    DrawTriangle((Vector2){ canvas.x + canvas.width, canvas.y + canvas.height*0.50f },
                 (Vector2){ canvas.x + canvas.width*0.81f, canvas.y + canvas.height*0.61f },
                 (Vector2){ canvas.x + canvas.width, canvas.y + canvas.height*0.66f },
                 yellow);

    Color dots = ink;
    dots.a = 105;
    DrawHalftone((Rectangle){ canvas.x, canvas.y, canvas.width*0.24f, canvas.height }, dots);
    DrawHalftone((Rectangle){ canvas.x + canvas.width*0.78f, canvas.y,
                              canvas.width*0.22f, canvas.height }, dots);

    for (int i = 0; i < 18; i++)
    {
        float px = canvas.x + canvas.width*(float)((i*37)%101)/100.0f;
        float py = canvas.y + canvas.height*(float)((i*61 + 13)%97)/96.0f;
        float radius = fmaxf(1.5f, canvas.width*(0.0015f + 0.0007f*(i%4)));
        DrawCircleV((Vector2){ px, py }, radius, dots);
    }
    return true;
}
