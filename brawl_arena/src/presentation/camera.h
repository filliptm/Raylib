#ifndef CAMERA_H
#define CAMERA_H

#include "app_types.h"

float CameraEffectiveDistance(float authoredDistance, bool mobile);
void CameraInit(App *world);
void CameraUpdate(App *world, float deltaTime);

#endif // CAMERA_H
