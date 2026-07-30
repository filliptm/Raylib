#ifndef BRAWL_PHONE_LAYOUT_H
#define BRAWL_PHONE_LAYOUT_H

#include "ui_system.h"

#define UI_PHONE_REFERENCE_HEIGHT 500.0f

typedef struct UiPhoneFrame {
    Rectangle safe;
    Vector2 origin;
    float scale;
    float referenceWidth;
} UiPhoneFrame;

typedef struct UiPhoneHomeLayout {
    Rectangle logo;
    Rectangle controls;
    Rectangle settings;
    Rectangle stage;
    Rectangle rail;
    Rectangle roster;
    Rectangle mode;
    Rectangle modePrevious;
    Rectangle modeNext;
    Rectangle modeSlab;
    Rectangle practice;
    Rectangle deploy;
} UiPhoneHomeLayout;

typedef struct UiPhoneRosterLayout {
    Rectangle header;
    Rectangle back;
    Rectangle title;
    Rectangle select;
    Rectangle identity;
    Rectangle stage;
    Rectangle telemetry;
    Rectangle candidateRail;
    Rectangle candidates[CLASS_COUNT];
} UiPhoneRosterLayout;

typedef struct UiPhoneResultLayout {
    Rectangle canvas;
    Rectangle panel;
    Rectangle title;
    Rectangle motif;
    Rectangle character;
    Rectangle impact;
    Rectangle score;
    Rectangle summary;
    Rectangle actions[3];
    Rectangle fallback;
} UiPhoneResultLayout;

UiPhoneFrame UiPhoneFrameForViewport(int width, int height,
                                     UiViewportInsets insets);
Rectangle UiPhoneRect(UiPhoneFrame frame, float x, float y,
                      float width, float height);
UiFrameLayout UiPhoneApplyFrame(UiSystem *ui, UiPhoneFrame frame);
void UiPhoneRestoreFrame(UiSystem *ui, UiFrameLayout previous);

UiPhoneHomeLayout UiPhoneHomeLayoutForFrame(UiPhoneFrame frame);
UiPhoneRosterLayout UiPhoneRosterLayoutForFrame(UiPhoneFrame frame);
UiPhoneResultLayout UiPhoneResultLayoutForFrame(UiPhoneFrame frame);

#endif
