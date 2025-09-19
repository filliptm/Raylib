#ifndef ANIMATION_H
#define ANIMATION_H

#include "common.h"
#include "errors.h"
#include "raylib.h"

typedef enum {
    ANIM_TYPE_MOVE,
    ANIM_TYPE_SCALE,
    ANIM_TYPE_ROTATE,
    ANIM_TYPE_COLOR,
    ANIM_TYPE_SHAKE,
    ANIM_TYPE_BOUNCE,
    ANIM_TYPE_ATTACK,
    ANIM_TYPE_DAMAGE,
    ANIM_TYPE_HEAL,
    ANIM_TYPE_DEATH
} AnimationType;

typedef enum {
    EASE_LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_IN_ELASTIC,
    EASE_OUT_ELASTIC,
    EASE_IN_OUT_ELASTIC
} EasingType;

typedef struct Animation {
    AnimationType type;
    EasingType easing;
    
    void* target;  // Pointer to the object being animated (Card, Player, etc.)
    
    // Animation properties
    Vector3 startPos;
    Vector3 endPos;
    Vector3 startScale;
    Vector3 endScale;
    float startRotation;
    float endRotation;
    Color startColor;
    Color endColor;
    
    // Timing
    float duration;
    float currentTime;
    bool isPlaying;
    bool loop;
    
    // Callback
    void (*onComplete)(void* target);
    
    // Chain to next animation
    struct Animation* next;
} Animation;

struct AnimationQueue {
    Animation* animations;
    int count;
    int capacity;
};

// Animation system functions
GameError InitAnimationSystem(AnimationQueue* queue);
void CleanupAnimationSystem(AnimationQueue* queue);

// Create animations
Animation* CreateMoveAnimation(void* target, Vector3 from, Vector3 to, float duration);
Animation* CreateScaleAnimation(void* target, Vector3 from, Vector3 to, float duration);
Animation* CreateColorAnimation(void* target, Color from, Color to, float duration);
Animation* CreateAttackAnimation(Card* attacker, void* defender, float duration);
Animation* CreateDamageAnimation(void* target, int damage, float duration);
Animation* CreateHealAnimation(void* target, int healing, float duration);
Animation* CreateDeathAnimation(Card* card, float duration);

// Queue management
GameError AddAnimation(AnimationQueue* queue, Animation* anim);
GameError PlayAnimation(Animation* anim);
void UpdateAnimations(AnimationQueue* queue, float deltaTime);
void ClearAnimations(AnimationQueue* queue);

// Utility functions
float EaseValue(float t, EasingType easing);
Vector3 LerpVector3(Vector3 a, Vector3 b, float t);
Color LerpColor(Color a, Color b, float t);

// Card-specific animations
void AnimateCardDraw(Card* card);
void AnimateCardPlay(Card* card);
void AnimateCardAttack(Card* attacker, void* target);
void AnimateCardDeath(Card* card);

#endif