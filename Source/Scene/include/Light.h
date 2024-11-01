#ifndef SCENE_LGIHT_H
#define SCENE_LGIHT_H
#include "../../Math/include/Vector.h"
#include "../../Math/include/Matrix.h"

namespace FTS 
{
    struct FPointLight
    {
        FVector4F Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        FVector3F Position = { 0.0f, 2.0f, 0.0f };
        FLOAT fIntensity = 0.5f;
        FLOAT fFallOffStart = 1.0f;
        FLOAT fFallOffEnd = 10.0f;
    };
        
    struct FDirectionalLight
    {
        FVector4F Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        FLOAT fIntensity = 5.0f;
        FVector3F Direction;
        FVector2F Angle = { 0.0f, 11.6f };

        FMatrix4x4 ViewProj;
    };
}




















#endif