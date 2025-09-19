#include "test_framework.h"
#include "../animation.h"
#include <math.h>

TEST(test_easing_functions) {
    // Test linear easing
    ASSERT_EQ(0.0f, EaseValue(0.0f, EASE_LINEAR));
    ASSERT_EQ(0.5f, EaseValue(0.5f, EASE_LINEAR));
    ASSERT_EQ(1.0f, EaseValue(1.0f, EASE_LINEAR));
    
    // Test quad easing
    float result = EaseValue(0.5f, EASE_IN_QUAD);
    ASSERT_TRUE(result > 0.2f && result < 0.3f);  // 0.5^2 = 0.25
}

TEST(test_lerp_functions) {
    Vector3 a = {0.0f, 0.0f, 0.0f};
    Vector3 b = {10.0f, 20.0f, 30.0f};
    
    Vector3 result = LerpVector3(a, b, 0.5f);
    ASSERT_EQ(5.0f, result.x);
    ASSERT_EQ(10.0f, result.y);
    ASSERT_EQ(15.0f, result.z);
    
    Color colorA = {0, 0, 0, 255};
    Color colorB = {255, 255, 255, 255};
    
    Color colorResult = LerpColor(colorA, colorB, 0.5f);
    ASSERT_EQ(127, colorResult.r);
    ASSERT_EQ(127, colorResult.g);
    ASSERT_EQ(127, colorResult.b);
}

TEST(test_animation_queue_init) {
    AnimationQueue queue;
    GameError result = InitAnimationSystem(&queue);
    
    ASSERT_EQ(GAME_OK, result);
    ASSERT_EQ(0, queue.count);
    ASSERT_TRUE(queue.capacity > 0);
    ASSERT_TRUE(queue.animations != NULL);
    
    CleanupAnimationSystem(&queue);
    ASSERT_TRUE(queue.animations == NULL);
    ASSERT_EQ(0, queue.count);
}

TEST(test_create_move_animation) {
    Vector3 from = {0.0f, 0.0f, 0.0f};
    Vector3 to = {10.0f, 10.0f, 10.0f};
    
    Animation* anim = CreateMoveAnimation(NULL, from, to, 1.0f);
    
    ASSERT_TRUE(anim != NULL);
    ASSERT_EQ(ANIM_TYPE_MOVE, anim->type);
    ASSERT_EQ(1.0f, anim->duration);
    ASSERT_EQ(0.0f, anim->currentTime);
    ASSERT_FALSE(anim->isPlaying);
    
    // Check positions
    ASSERT_EQ(from.x, anim->startPos.x);
    ASSERT_EQ(to.x, anim->endPos.x);
    
    free(anim);
}

int main() {
    RUN_TEST(test_easing_functions);
    RUN_TEST(test_lerp_functions);
    RUN_TEST(test_animation_queue_init);
    RUN_TEST(test_create_move_animation);
    
    TEST_SUMMARY();
}