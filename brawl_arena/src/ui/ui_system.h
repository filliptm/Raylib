#ifndef BRAWL_UI_SYSTEM_H
#define BRAWL_UI_SYSTEM_H

#include "ui_types.h"
#include "ui_theme.h"
#include "ui_icons.h"
#include "ui_skin.h"
#include "app_types.h"

typedef struct UiResources {
    Font display;
    Font body;
    Font emphasis;
    Font data;
    bool displayOwned;
    bool bodyOwned;
    bool emphasisOwned;
    bool dataOwned;
} UiResources;

typedef struct UiSystem {
    const UiTheme *theme;
    UiResources resources;
    UiSkin skin;
    UiFrameLayout layout;
    UiViewportInsets insets;
    UiInputModality modality;
    int glyphMode;
    Vector2 previousMouse;
    UiId focused;
    UiId restoreFocus;
    UiFocusNode previousNodes[UI_MAX_FOCUS_NODES];
    int previousNodeCount;
    UiFocusNode nodes[UI_MAX_FOCUS_NODES];
    int nodeCount;
    bool activatePressed;
    bool backPressed;
    bool previousPressed;
    bool nextPressed;
    int navigationX;
    int navigationY;
    bool focusVisible;
    bool fontFallback;
    bool interactionsEnabled;
    bool focusOverflow;
    bool reducedMotion;
    float frameDt;
    float elapsed;
} UiSystem;

bool UiSystemLoad(UiSystem *ui);
void UiSystemUnload(UiSystem *ui);
void UiSystemBeginFrame(UiSystem *ui, const UiPreferences *preferences,
                        int width, int height, float dt);
void UiSystemSetViewportInsets(UiSystem *ui, UiViewportInsets insets);
void UiSystemEndFrame(UiSystem *ui);
void UiSystemSetActive(UiSystem *ui);
UiSystem *UiSystemActive(void);
void UiSetInteractionsEnabled(bool enabled);

Rectangle UiRefRect(float x, float y, float width, float height);
Vector2 UiRefPoint(float x, float y);
float UiScale(float value);
float UiReferenceScaleForViewport(int width, int height, float preferenceScale);
Rectangle UiReferenceSafeRect(int width, int height, float preferenceScale);
Rectangle UiReferenceSafeRectWithInsets(int width, int height,
                                        float preferenceScale,
                                        UiViewportInsets insets);
Rectangle UiTouchTargetBounds(Rectangle bounds, float minimumSize);
UiId UiFocusNeighbor(const UiFocusNode *nodes, int count, UiId current, int dx, int dy);
float UiMotionDuration(float normalDuration, bool reducedMotion);
float UiEaseOutCubic(float value);
float UiEaseOutBack(float value);
float UiMotionProgress(float age, float delay, float duration, bool reducedMotion);

UiId UiHash(const char *text);
void UiFocus(UiId id);
bool UiBackPressed(void);
UiInputModality UiCurrentModality(void);
const char *UiBindingLabel(const char *keyboardMouse, const char *gamepad,
                           const char *touch);

float UiTextSize(UiTextRole role);
Vector2 UiMeasureText(UiTextRole role, const char *text);
void UiDrawText(UiTextRole role, const char *text, Vector2 position, Color color);
void UiDrawTextAligned(UiTextRole role, const char *text, Rectangle bounds,
                       UiAlign align, Color color);
void UiDrawTextFit(UiTextRole role, const char *text, Rectangle bounds,
                   UiAlign align, Color color);
void UiDrawTextShadow(UiTextRole role, const char *text, Vector2 position, Color color);
void UiDrawTextOutline(UiTextRole role, const char *text, Vector2 position,
                       Color fill, Color outline, float thickness);

void UiDrawPanel(Rectangle bounds, Color fill, Color edge, bool raised);
void UiDrawFeaturePanel(Rectangle bounds, Color fill, Color edge, bool raised);
void UiDrawControlSurface(Rectangle bounds, Color fill, Color edge, bool raised);
void UiDrawSignalRail(Rectangle bounds, Color color, bool rightSide);
void UiDrawKeycap(Rectangle bounds, const char *label, bool active);
void UiDrawProgress(Rectangle bounds, float value, Color fill, bool segmented, int segments);
void UiDrawDecoration(UiDecoration decoration, Rectangle bounds, Color tint, float opacity);
void UiDrawCharacterMotif(CharacterUiMotif motif, Rectangle bounds,
                          Color primary, Color secondary, float opacity);
void UiDrawComicBackdrop(void);
void UiDrawArenaLogo(Rectangle bounds);
UiResponse UiInteract(UiId id, Rectangle bounds, bool enabled);
UiResponse UiButton(UiId id, Rectangle bounds, const char *label,
                    UiButtonStyle style, UiIcon icon);
UiResponse UiIconButton(UiId id, Rectangle bounds, UiIcon icon, const char *accessibleLabel);
UiResponse UiToggle(UiId id, Rectangle bounds, const char *label, bool value);

#endif
