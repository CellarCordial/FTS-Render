#ifndef SCENE_CAMERA_H
#define SCENE_CAMERA_H

#include <math.h>
#include "../../Core/include/SysCall.h"
#include "../../Math/include/Vector.h"
#include "../../Math/include/Matrix.h"
#include <glfw3.h>

namespace FTS 
{
    class FCamera
    {
    public:
        FCamera(GLFWwindow* pWindow);

    public:
        void HandleInput(FLOAT fDeltaTime);

        void UpdateViewMatrix();
        void HandleKeyboardInput(FLOAT InDeltaTime);

        void SetLens(FLOAT InFOV, FLOAT InAspect, FLOAT InNearZ, FLOAT InFarZ);
        void SetPosition(FLOAT X, FLOAT Y, FLOAT Z);
        void SetPosition(const FVector3F& InPosition);
        void SetDirection(FLOAT fVert, FLOAT fHorz);
        
        void Strafe(FLOAT InSize);      // 前后移动
        void Walk(FLOAT InSize);        // 左右平移
        void Vertical(FLOAT InSize);    // 上下移动  

        void Pitch(FLOAT InAngle);  // 上下俯仰
        void Yall(FLOAT InAngle);   // 左右转向
        void Pitch(INT32 X);  // 上下俯仰
        void Yall(INT32 Y);   // 左右转向

        FLOAT GetFOVX() const;
        FLOAT GetFOVY() const;
        
        FLOAT GetNearWindowWidth() const;
        FLOAT GetFarWindowWidth() const;

        FMatrix4x4 GetViewProj() const { return Mul(ViewMatrix, ProjMatrix); }
        FVector2F GetProjectConstantsAB() const;
        FVector2F CursorCycle(FLOAT x, FLOAT y);


        struct FrustumDirections
        {
            // 左上, 右上, 左下, 右下.
            FVector3F A, B, C, D;
        };  

        FCamera::FrustumDirections GetFrustumDirections();


        FMatrix4x4 ViewMatrix;
        FMatrix4x4 ProjMatrix;
        FVector3F Position = { 4.087f, 3.6999f, 3.957f };
        FVector3F Direction = { 0.0f, 0.0f, 1.0f };
        FVector3F Up = { 0.0f, 1.0f, 0.0f };

    private:
		FLOAT m_fVertRadians = 0.0f;
		FLOAT m_fHorzRadians = 0.0f;

        FLOAT m_fNearZ = 0.0f;
        FLOAT m_fFarZ = 0.0f;
        
        FLOAT m_fAspect = 0.0f;
        FLOAT m_fFOVY = 0.0f;
        
        FLOAT m_fNearWindowHeight = 0.0f;
        FLOAT m_fFarWindowHeight = 0.0f;

        BOOL m_bViewDirty = true;

        FVector2F m_MousePosition;

        GLFWwindow* m_pWindow;
    };


}



#endif