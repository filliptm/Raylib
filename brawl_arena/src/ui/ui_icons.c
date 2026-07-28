#include "ui_icons.h"
#include <math.h>

static void Line(Vector2 a, Vector2 b, float stroke, Color color)
{
    DrawLineEx(a, b, stroke, color);
}

void UiIconDraw(UiIcon icon, Vector2 c, float s, Color color)
{
    float h = s*0.5f;
    float stroke = s >= 24.0f ? 2.5f : 2.0f;
    switch (icon)
    {
        case UI_ICON_BACK:
        case UI_ICON_PREVIOUS:
            Line((Vector2){ c.x + h*0.55f, c.y - h*0.72f },
                 (Vector2){ c.x - h*0.45f, c.y }, stroke, color);
            Line((Vector2){ c.x - h*0.45f, c.y },
                 (Vector2){ c.x + h*0.55f, c.y + h*0.72f }, stroke, color);
            break;
        case UI_ICON_NEXT:
            Line((Vector2){ c.x - h*0.55f, c.y - h*0.72f },
                 (Vector2){ c.x + h*0.45f, c.y }, stroke, color);
            Line((Vector2){ c.x + h*0.45f, c.y },
                 (Vector2){ c.x - h*0.55f, c.y + h*0.72f }, stroke, color);
            break;
        case UI_ICON_CLOSE:
        case UI_ICON_QUIT:
            Line((Vector2){ c.x - h*0.62f, c.y - h*0.62f },
                 (Vector2){ c.x + h*0.62f, c.y + h*0.62f }, stroke, color);
            Line((Vector2){ c.x + h*0.62f, c.y - h*0.62f },
                 (Vector2){ c.x - h*0.62f, c.y + h*0.62f }, stroke, color);
            break;
        case UI_ICON_SETTINGS:
            DrawCircleLines((int)c.x, (int)c.y, h*0.44f, color);
            for (int i = 0; i < 8; i++)
            {
                float a = (float)i*PI/4.0f;
                Line((Vector2){ c.x + cosf(a)*h*0.62f, c.y + sinf(a)*h*0.62f },
                     (Vector2){ c.x + cosf(a)*h*0.92f, c.y + sinf(a)*h*0.92f },
                     stroke, color);
            }
            break;
        case UI_ICON_CONTROLS:
            DrawRectangleLinesEx((Rectangle){ c.x - h*0.82f, c.y - h*0.48f,
                                              h*1.64f, h*0.96f }, stroke, color);
            Line((Vector2){ c.x - h*0.52f, c.y }, (Vector2){ c.x - h*0.12f, c.y },
                 stroke, color);
            Line((Vector2){ c.x - h*0.32f, c.y - h*0.2f },
                 (Vector2){ c.x - h*0.32f, c.y + h*0.2f }, stroke, color);
            DrawCircle((int)(c.x + h*0.36f), (int)(c.y - h*0.14f), stroke, color);
            DrawCircle((int)(c.x + h*0.58f), (int)(c.y + h*0.14f), stroke, color);
            break;
        case UI_ICON_PRACTICE:
        case UI_ICON_RANGE:
            DrawCircleLines((int)c.x, (int)c.y, h*0.78f, color);
            DrawCircleLines((int)c.x, (int)c.y, h*0.34f, color);
            Line((Vector2){ c.x - h, c.y }, (Vector2){ c.x + h, c.y }, stroke, color);
            Line((Vector2){ c.x, c.y - h }, (Vector2){ c.x, c.y + h }, stroke, color);
            break;
        case UI_ICON_HEALTH:
        case UI_ICON_ALLY:
            DrawRectangle((int)(c.x - h*0.2f), (int)(c.y - h*0.78f),
                          (int)(h*0.4f), (int)(h*1.56f), color);
            DrawRectangle((int)(c.x - h*0.78f), (int)(c.y - h*0.2f),
                          (int)(h*1.56f), (int)(h*0.4f), color);
            break;
        case UI_ICON_DAMAGE:
        case UI_ICON_ENEMY:
        {
            Vector2 points[4] = {
                { c.x, c.y - h }, { c.x + h*0.72f, c.y },
                { c.x, c.y + h }, { c.x - h*0.72f, c.y }
            };
            DrawTriangle(points[0], points[1], points[2], color);
            DrawTriangle(points[0], points[2], points[3], color);
            break;
        }
        case UI_ICON_RELOAD:
            DrawCircleLines((int)c.x, (int)c.y, h*0.72f, color);
            DrawTriangle((Vector2){ c.x + h*0.52f, c.y - h*0.66f },
                         (Vector2){ c.x + h*0.94f, c.y - h*0.62f },
                         (Vector2){ c.x + h*0.72f, c.y - h*0.25f }, color);
            break;
        case UI_ICON_SUPER:
            DrawPoly(c, 6, h*0.88f, 30.0f, color);
            DrawPoly(c, 6, h*0.46f, 30.0f, (Color){ 5, 11, 20, color.a });
            break;
        case UI_ICON_GEM:
            DrawPoly(c, 4, h*0.82f, 45.0f, color);
            DrawPolyLines(c, 4, h*0.82f, 45.0f, WHITE);
            break;
    }
}
