#include "animation.h"
#include "card.h"
#include "effects.h"
#include <stdlib.h>
#include <math.h>

GameError InitAnimationSystem(AnimationQueue* queue) {
    if (!queue) return GAME_ERROR_INVALID_PARAMETER;
    
    queue->capacity = 100;
    queue->animations = calloc(queue->capacity, sizeof(Animation));
    if (!queue->animations) return GAME_ERROR_OUT_OF_MEMORY;
    
    queue->count = 0;
    return GAME_OK;
}

void CleanupAnimationSystem(AnimationQueue* queue) {
    if (!queue || !queue->animations) return;
    
    free(queue->animations);
    queue->animations = NULL;
    queue->count = 0;
    queue->capacity = 0;
}

// Create move animation
Animation* CreateMoveAnimation(void* target, Vector3 from, Vector3 to, float duration) {
    Animation* anim = calloc(1, sizeof(Animation));
    if (!anim) return NULL;
    
    anim->type = ANIM_TYPE_MOVE;
    anim->easing = EASE_IN_OUT_CUBIC;
    anim->target = target;
    anim->startPos = from;
    anim->endPos = to;
    anim->duration = duration;
    anim->currentTime = 0;
    anim->isPlaying = false;
    anim->loop = false;
    anim->onComplete = NULL;
    anim->next = NULL;
    
    return anim;
}

// Create scale animation
Animation* CreateScaleAnimation(void* target, Vector3 from, Vector3 to, float duration) {
    Animation* anim = calloc(1, sizeof(Animation));
    if (!anim) return NULL;
    
    anim->type = ANIM_TYPE_SCALE;
    anim->easing = EASE_OUT_ELASTIC;
    anim->target = target;
    anim->startScale = from;
    anim->endScale = to;
    anim->duration = duration;
    anim->currentTime = 0;
    anim->isPlaying = false;
    anim->loop = false;
    anim->onComplete = NULL;
    anim->next = NULL;
    
    return anim;
}

// Create color animation
Animation* CreateColorAnimation(void* target, Color from, Color to, float duration) {
    Animation* anim = calloc(1, sizeof(Animation));
    if (!anim) return NULL;
    
    anim->type = ANIM_TYPE_COLOR;
    anim->easing = EASE_LINEAR;
    anim->target = target;
    anim->startColor = from;
    anim->endColor = to;
    anim->duration = duration;
    anim->currentTime = 0;
    anim->isPlaying = false;
    anim->loop = false;
    anim->onComplete = NULL;
    anim->next = NULL;
    
    return anim;
}

// Add animation to queue
GameError AddAnimation(AnimationQueue* queue, Animation* anim) {
    if (!queue || !anim) return GAME_ERROR_INVALID_PARAMETER;
    
    if (queue->count >= queue->capacity) {
        return GAME_ERROR_OUT_OF_MEMORY;
    }
    
    queue->animations[queue->count] = *anim;
    queue->count++;
    
    return GAME_OK;
}

// Play animation
GameError PlayAnimation(Animation* anim) {
    if (!anim) return GAME_ERROR_INVALID_PARAMETER;
    
    anim->isPlaying = true;
    anim->currentTime = 0;
    
    return GAME_OK;
}

// Update all animations
void UpdateAnimations(AnimationQueue* queue, float deltaTime) {
    if (!queue || !queue->animations) return;
    
    for (int i = 0; i < queue->count; i++) {
        Animation* anim = &queue->animations[i];
        
        if (!anim->isPlaying) continue;
        
        anim->currentTime += deltaTime;
        
        float t = anim->currentTime / anim->duration;
        if (t > 1.0f) t = 1.0f;
        
        float easedT = EaseValue(t, anim->easing);
        
        // Apply animation based on type
        switch (anim->type) {
            case ANIM_TYPE_MOVE:
                if (anim->target) {
                    Card* card = (Card*)anim->target;
                    card->position = LerpVector3(anim->startPos, anim->endPos, easedT);
                }
                break;
                
            case ANIM_TYPE_SCALE:
                if (anim->target) {
                    Card* card = (Card*)anim->target;
                    card->size = LerpVector3(anim->startScale, anim->endScale, easedT);
                }
                break;
                
            case ANIM_TYPE_COLOR:
                if (anim->target) {
                    Card* card = (Card*)anim->target;
                    card->color = LerpColor(anim->startColor, anim->endColor, easedT);
                }
                break;
                
            default:
                break;
        }
        
        // Check if animation is complete
        if (anim->currentTime >= anim->duration) {
            anim->isPlaying = false;
            
            if (anim->onComplete) {
                anim->onComplete(anim->target);
            }
            
            if (anim->loop) {
                anim->currentTime = 0;
                anim->isPlaying = true;
            }
        }
    }
    
    // Remove completed animations
    int writeIndex = 0;
    for (int readIndex = 0; readIndex < queue->count; readIndex++) {
        if (queue->animations[readIndex].isPlaying || queue->animations[readIndex].loop) {
            if (writeIndex != readIndex) {
                queue->animations[writeIndex] = queue->animations[readIndex];
            }
            writeIndex++;
        }
    }
    queue->count = writeIndex;
}

// Clear all animations
void ClearAnimations(AnimationQueue* queue) {
    if (!queue) return;
    queue->count = 0;
}

// Easing functions
float EaseValue(float t, EasingType easing) {
    switch (easing) {
        case EASE_LINEAR:
            return t;
            
        case EASE_IN_QUAD:
            return t * t;
            
        case EASE_OUT_QUAD:
            return t * (2 - t);
            
        case EASE_IN_OUT_QUAD:
            return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
            
        case EASE_IN_CUBIC:
            return t * t * t;
            
        case EASE_OUT_CUBIC:
            t = t - 1;
            return t * t * t + 1;
            
        case EASE_IN_OUT_CUBIC:
            return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
            
        case EASE_IN_ELASTIC:
            if (t == 0) return 0;
            if (t == 1) return 1;
            return -powf(2, 10 * (t - 1)) * sinf((t - 1.1f) * 5 * PI);
            
        case EASE_OUT_ELASTIC:
            if (t == 0) return 0;
            if (t == 1) return 1;
            return powf(2, -10 * t) * sinf((t - 0.1f) * 5 * PI) + 1;
            
        case EASE_IN_OUT_ELASTIC:
            if (t == 0) return 0;
            if (t == 1) return 1;
            t *= 2;
            if (t < 1) {
                return -0.5f * powf(2, 10 * (t - 1)) * sinf((t - 1.1f) * 5 * PI);
            }
            return powf(2, -10 * (t - 1)) * sinf((t - 1.1f) * 5 * PI) * 0.5f + 1;
            
        default:
            return t;
    }
}

// Lerp functions
Vector3 LerpVector3(Vector3 a, Vector3 b, float t) {
    return (Vector3){
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

Color LerpColor(Color a, Color b, float t) {
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

// Card-specific animations
void AnimateCardDraw(Card* card) {
    // Animate card from deck to hand
    Animation* moveAnim = CreateMoveAnimation(card, 
        (Vector3){10.0f, 0.0f, card->ownerPlayer == 0 ? 10.0f : -10.0f},
        card->position, 0.5f);
    
    Animation* scaleAnim = CreateScaleAnimation(card,
        (Vector3){0.1f, 0.1f, 0.1f},
        card->size, 0.5f);
    
    if (moveAnim) {
        moveAnim->easing = EASE_OUT_CUBIC;
        // Would add to animation queue here
        free(moveAnim);
    }
    
    if (scaleAnim) {
        scaleAnim->easing = EASE_OUT_ELASTIC;
        // Would add to animation queue here
        free(scaleAnim);
    }
}

void AnimateCardPlay(Card* card) {
    // Animate card from hand to board
    Animation* moveAnim = CreateMoveAnimation(card,
        card->position,
        card->targetPosition, 0.6f);
    
    if (moveAnim) {
        moveAnim->easing = EASE_IN_OUT_CUBIC;
        // Would add to animation queue here
        free(moveAnim);
    }
}

void AnimateCardAttack(Card* attacker, void* target) {
    // Create attack animation
    Vector3 originalPos = attacker->position;
    Vector3 targetPos = originalPos;
    
    if (target) {
        Card* targetCard = (Card*)target;
        targetPos = targetCard->position;
    }
    
    // Move halfway to target and back
    Vector3 attackPos = LerpVector3(originalPos, targetPos, 0.7f);
    
    Animation* moveToAnim = CreateMoveAnimation(attacker, originalPos, attackPos, 0.2f);
    Animation* moveBackAnim = CreateMoveAnimation(attacker, attackPos, originalPos, 0.2f);
    
    if (moveToAnim && moveBackAnim) {
        moveToAnim->easing = EASE_OUT_QUAD;
        moveBackAnim->easing = EASE_IN_QUAD;
        // Would chain animations and add to queue here
        free(moveToAnim);
        free(moveBackAnim);
    }
}

void AnimateCardDeath(Card* card) {
    // Animate card death
    Animation* scaleAnim = CreateScaleAnimation(card,
        card->size,
        (Vector3){0.0f, 0.0f, 0.0f}, 0.5f);
    
    Color fadedColor = card->color;
    fadedColor.a = 0;
    Animation* colorAnim = CreateColorAnimation(card,
        card->color,
        fadedColor, 0.5f);
    
    if (scaleAnim) {
        scaleAnim->easing = EASE_IN_CUBIC;
        // Would add to animation queue here
        free(scaleAnim);
    }
    
    if (colorAnim) {
        // Would add to animation queue here
        free(colorAnim);
    }
}