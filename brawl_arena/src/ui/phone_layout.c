#include "phone_layout.h"

#include <math.h>

static float NonNegative(float value)
{
    return fmaxf(0.0f, value);
}

UiPhoneFrame UiPhoneFrameForViewport(int width, int height,
                                     UiViewportInsets insets)
{
    const float horizontalMargin = 12.0f;
    const float verticalMargin = 8.0f;
    float left = NonNegative(insets.left) + horizontalMargin;
    float top = NonNegative(insets.top) + verticalMargin;
    float right = width - NonNegative(insets.right) - horizontalMargin;
    float bottom = height - NonNegative(insets.bottom) - verticalMargin;
    Rectangle safe = {
        left,
        top,
        fmaxf(1.0f, right - left),
        fmaxf(1.0f, bottom - top)
    };
    float scale = safe.height/UI_PHONE_REFERENCE_HEIGHT;
    if (scale <= 0.0f) scale = 1.0f;
    return (UiPhoneFrame){
        .safe = safe,
        .origin = { safe.x, safe.y },
        .scale = scale,
        .referenceWidth = safe.width/scale
    };
}

Rectangle UiPhoneRect(UiPhoneFrame frame, float x, float y,
                      float width, float height)
{
    return (Rectangle){
        frame.origin.x + x*frame.scale,
        frame.origin.y + y*frame.scale,
        width*frame.scale,
        height*frame.scale
    };
}

UiFrameLayout UiPhoneApplyFrame(UiSystem *ui, UiPhoneFrame frame)
{
    UiFrameLayout previous = ui ? ui->layout : (UiFrameLayout){ 0 };
    if (!ui) return previous;
    ui->layout.viewportScale = frame.scale;
    ui->layout.scale = frame.scale;
    ui->layout.origin = frame.origin;
    ui->layout.safe = frame.safe;
    ui->layout.content = frame.safe;
    return previous;
}

void UiPhoneRestoreFrame(UiSystem *ui, UiFrameLayout previous)
{
    if (ui) ui->layout = previous;
}

UiPhoneHomeLayout UiPhoneHomeLayoutForFrame(UiPhoneFrame frame)
{
    float width = frame.referenceWidth;
    const float railY = 420.0f;
    const float railPadding = 12.0f;
    const float gap = 12.0f;
    float innerWidth = width - railPadding*2.0f;
    float rosterWidth = innerWidth*0.22f;
    float modeWidth = innerWidth*0.31f;
    float practiceWidth = innerWidth*0.17f;
    float deployWidth =
        innerWidth - rosterWidth - modeWidth - practiceWidth - gap*3.0f;
    float x = railPadding;

    UiPhoneHomeLayout layout = {
        .logo = UiPhoneRect(frame, 0.0f, -8.0f, 280.0f, 158.0f),
        .controls = UiPhoneRect(frame, width - 316.0f, 8.0f, 150.0f, 64.0f),
        .settings = UiPhoneRect(frame, width - 154.0f, 8.0f, 150.0f, 64.0f),
        .stage = UiPhoneRect(frame, width*0.25f, 72.0f, width*0.50f, 350.0f),
        .rail = UiPhoneRect(frame, 0.0f, railY, width, 80.0f)
    };
    layout.roster = UiPhoneRect(frame, x, railY + 9.0f,
                                rosterWidth, 62.0f);
    x += rosterWidth + gap;
    layout.mode = UiPhoneRect(frame, x, railY + 5.0f,
                              modeWidth, 70.0f);
    layout.modePrevious = UiPhoneRect(frame, x + 8.0f, railY + 9.0f,
                                      64.0f, 62.0f);
    layout.modeNext = UiPhoneRect(frame, x + modeWidth - 72.0f,
                                  railY + 9.0f, 64.0f, 62.0f);
    layout.modeSlab = UiPhoneRect(frame, x + 80.0f, railY + 9.0f,
                                  fmaxf(1.0f, modeWidth - 160.0f), 62.0f);
    x += modeWidth + gap;
    layout.practice = UiPhoneRect(frame, x, railY + 9.0f,
                                  practiceWidth, 62.0f);
    x += practiceWidth + gap;
    layout.deploy = UiPhoneRect(frame, x, railY + 5.0f,
                                deployWidth, 70.0f);
    return layout;
}

UiPhoneRosterLayout UiPhoneRosterLayoutForFrame(UiPhoneFrame frame)
{
    float width = frame.referenceWidth;
    const float sideWidth = 280.0f;
    const float gap = 12.0f;
    float stageWidth = fmaxf(220.0f, width - sideWidth*2.0f - gap*2.0f);
    float candidateGap = 8.0f;
    float candidatePadding = 10.0f;
    float candidateWidth =
        (width - candidatePadding*2.0f - candidateGap*(CLASS_COUNT - 1))/
        CLASS_COUNT;

    UiPhoneRosterLayout layout = {
        .header = UiPhoneRect(frame, 0.0f, 0.0f, width, 70.0f),
        .back = UiPhoneRect(frame, 10.0f, 4.0f, 150.0f, 62.0f),
        .title = UiPhoneRect(frame, 176.0f, 4.0f,
                             fmaxf(1.0f, width - 432.0f), 62.0f),
        .select = UiPhoneRect(frame, width - 244.0f, 4.0f, 234.0f, 62.0f),
        .identity = UiPhoneRect(frame, 0.0f, 80.0f, sideWidth, 328.0f),
        .stage = UiPhoneRect(frame, sideWidth + gap, 76.0f,
                             stageWidth, 336.0f),
        .telemetry = UiPhoneRect(frame, width - sideWidth, 80.0f,
                                 sideWidth, 328.0f),
        .candidateRail = UiPhoneRect(frame, 0.0f, 420.0f, width, 80.0f)
    };
    for (int i = 0; i < CLASS_COUNT; i++)
    {
        layout.candidates[i] = UiPhoneRect(
            frame,
            candidatePadding + i*(candidateWidth + candidateGap),
            429.0f, candidateWidth, 62.0f);
    }
    return layout;
}

UiPhoneResultLayout UiPhoneResultLayoutForFrame(UiPhoneFrame frame)
{
    float width = frame.referenceWidth;
    const float gap = 12.0f;
    const float side = 20.0f;
    float actionWidth = (width - side*2.0f - gap*2.0f)/3.0f;
    UiPhoneResultLayout layout = {
        .canvas = UiPhoneRect(frame, 0.0f, 0.0f, width, 500.0f),
        .panel = UiPhoneRect(frame, 10.0f, 8.0f, width - 20.0f, 484.0f),
        .title = UiPhoneRect(frame, 50.0f, 18.0f, width - 100.0f, 82.0f),
        .motif = UiPhoneRect(frame, width*0.30f, 92.0f, width*0.40f, 236.0f),
        .character = UiPhoneRect(frame, width*0.30f, 104.0f,
                                 width*0.40f, 44.0f),
        .impact = UiPhoneRect(frame, width*0.30f, 146.0f,
                              width*0.40f, 28.0f),
        .score = UiPhoneRect(frame, width*0.25f, 184.0f,
                             width*0.50f, 88.0f),
        .summary = UiPhoneRect(frame, 80.0f, 278.0f,
                               width - 160.0f, 42.0f),
        .fallback = UiPhoneRect(frame, width*0.34f, 466.0f,
                                width*0.32f, 24.0f)
    };
    for (int i = 0; i < 3; i++)
    {
        layout.actions[i] = UiPhoneRect(
            frame, side + i*(actionWidth + gap), 378.0f,
            actionWidth, 72.0f);
    }
    return layout;
}
