#include "../include/Camera.h"
#include "glfw3.h"
#include <wtypesbase.h>

namespace FTS 
{
    
    FCamera::FCamera(GLFWwindow* pWindow) : 
        m_pWindow(pWindow)
    {
        SetLens(60.0f, 1.0f * CLIENT_WIDTH / CLIENT_HEIGHT, 0.1f, 100.0f);
        UpdateViewMatrix();
    }

    void FCamera::HandleInput(FLOAT fDeltaTime)
    {
        DOUBLE dNewX, dNewY;
        glfwGetCursorPos(m_pWindow, &dNewX, &dNewY);
        FVector2F NewPos(dNewX, dNewY);
        auto CusorState = glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_LEFT);
        if (CusorState == GLFW_PRESS)
        {
             //glfwSetInputMode(m_pWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

            Pitch(static_cast<INT32>(dNewY));
            Yall(static_cast<INT32>(dNewX));
            UpdateViewMatrix();

            NewPos = CursorCycle(NewPos.x, NewPos.y);
        }
        else if (CusorState == GLFW_RELEASE)
        {
             //glfwSetInputMode(m_pWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        m_MousePosition = NewPos;

        HandleKeyboardInput(fDeltaTime);
    }

    void FCamera::UpdateViewMatrix()
    {
        if(m_bViewDirty)
        {
            ViewMatrix = LookAtLeftHand(Position, Position + Direction, Up);
            m_bViewDirty = false;
        }
    }

    FCamera::FrustumDirections FCamera::GetFrustumDirections()
    {
        FMatrix4x4 InvViewProj = Inverse(Mul(ViewMatrix, ProjMatrix));
        const FVector4F A0 = Mul(FVector4F(-1, 1, 0.2f, 1.0f), InvViewProj);
        const FVector4F A1 = Mul(FVector4F(-1, 1, 0.5f, 1.0f), InvViewProj);

        const FVector4F B0 = Mul(FVector4F(1, 1, 0.2f, 1.0f), InvViewProj);
        const FVector4F B1 = Mul(FVector4F(1, 1, 0.5f, 1.0f), InvViewProj);

        const FVector4F C0 = Mul(FVector4F(-1, -1, 0.2f, 1.0f), InvViewProj);
        const FVector4F C1 = Mul(FVector4F(-1, -1, 0.5f, 1.0f), InvViewProj);
        
        const FVector4F D0 = Mul(FVector4F(1, -1, 0.2f, 1.0f), InvViewProj);
        const FVector4F D1 = Mul(FVector4F(1, -1, 0.5f, 1.0f), InvViewProj);

        FrustumDirections Directions;
        Directions.A = Normalize(FVector3F(A1) / A1.w - FVector3F(A0) / A0.w);
        Directions.B = Normalize(FVector3F(B1) / B1.w - FVector3F(B0) / B0.w);
        Directions.C = Normalize(FVector3F(C1) / C1.w - FVector3F(C0) / C0.w);
        Directions.D = Normalize(FVector3F(D1) / D1.w - FVector3F(D0) / D0.w);

        return Directions;
    }


    void FCamera::HandleKeyboardInput(FLOAT InDeltaTime)
    {
        auto Action = glfwGetKey(m_pWindow, GLFW_KEY_W);
        if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS) Walk(3.0f * InDeltaTime);
        if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS) Walk(-3.0f * InDeltaTime);
        if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS) Strafe(3.0f * InDeltaTime);
        if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS) Strafe(-3.0f * InDeltaTime);
        if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS) Vertical(3.0f * InDeltaTime);
        if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS) Vertical(-3.0f * InDeltaTime);

        UpdateViewMatrix();
    }

    void FCamera::SetLens(FLOAT fFOV, FLOAT fAspect, FLOAT fNearZ, FLOAT fFarZ)
    {
        m_fFOVY = fFOV; m_fAspect = fAspect; m_fNearZ = fNearZ; m_fFarZ = fFarZ;
        m_fNearWindowHeight = m_fNearZ * static_cast<FLOAT>(std::tan(m_fFOVY)) * 2.0f;
        m_fFarWindowHeight = m_fFarZ * static_cast<FLOAT>(std::tan(m_fFOVY)) * 2.0f;

        ProjMatrix = PerspectiveLeftHand(m_fFOVY, m_fAspect, m_fNearZ, m_fFarZ);    // LH is left hand
    }

    void FCamera::SetPosition(FLOAT X, FLOAT Y, FLOAT Z)
    {
        Position = FVector3F{ X, Y, Z };
        m_bViewDirty = true;
    }

    void FCamera::SetPosition(const FVector3F& InPosition)
    {
        Position = InPosition;
        m_bViewDirty = true;
    }

	void FCamera::SetDirection(FLOAT fVert, FLOAT fHorz)
	{
        // TODO
		m_fVertRadians = Radians(fVert);
		m_fHorzRadians = Radians(fHorz);
        Direction = FVector3F(
            std::cos(m_fVertRadians) * std::cos(m_fHorzRadians),
            std::sin(m_fVertRadians),
            std::cos(m_fVertRadians) * std::sin(m_fHorzRadians)
        );

	}

	void FCamera::Walk(FLOAT InSize)
    {
        const FVector3F Size(InSize);
        Position = Size * Direction + Position;

        m_bViewDirty = true;
    }

    void FCamera::Strafe(FLOAT InSize)
    {
        const FVector3F Size(InSize);

        FVector3F Right = Cross(Up, Direction);
        Position = Size * Right + Position;

        m_bViewDirty = true;
    }

    void FCamera::Vertical(FLOAT InSize)
    {
        const FVector3F Size(InSize);
        Position = Size * Up + Position;

        m_bViewDirty = true;
    }

    void FCamera::Pitch(FLOAT InAngle)
    {
        m_fVertRadians -= Radians(InAngle);
		Direction = FVector3F(
			std::cos(m_fVertRadians) * std::cos(m_fHorzRadians),
			std::sin(m_fVertRadians),
			std::cos(m_fVertRadians) * std::sin(m_fHorzRadians)
		);

        m_bViewDirty = true;
    }

    void FCamera::Pitch(INT32 Y)
    {
        Pitch(0.2f * (static_cast<FLOAT>(Y) - m_MousePosition.y));
    }

    void FCamera::Yall(FLOAT InAngle)
    {
        m_fHorzRadians -= Radians(InAngle);
		Direction = FVector3F(
			std::cos(m_fVertRadians) * std::cos(m_fHorzRadians),
			std::sin(m_fVertRadians),
			std::cos(m_fVertRadians) * std::sin(m_fHorzRadians)
		);

        m_bViewDirty = true;
    }

    void FCamera::Yall(INT32 X)
    {
        Yall(0.2f * (static_cast<FLOAT>(X) - m_MousePosition.x));
    }

    FLOAT FCamera::GetFOVX() const
    {
        return 2.0f * static_cast<FLOAT>(std::atan((0.5f * GetNearWindowWidth()) / m_fNearZ));
    }

    FLOAT FCamera::GetNearWindowWidth() const
    {
        return m_fAspect * m_fNearWindowHeight;
    }

    FLOAT FCamera::GetFarWindowWidth() const
    {
        return m_fAspect * m_fFarWindowHeight;
    }

    FVector2F FCamera::CursorCycle(FLOAT x, FLOAT y)
    {
        if (x > CLIENT_WIDTH)     x = 0.0f;
        if (x < 0.0f)               x = static_cast<FLOAT>(CLIENT_WIDTH);
        if (y > CLIENT_HEIGHT)    y = 0.0f;
        if (y < 0.0f)               y = static_cast<FLOAT>(CLIENT_HEIGHT);
        glfwSetCursorPos(m_pWindow, static_cast<DOUBLE>(x), static_cast<DOUBLE>(y));

        return FVector2F(x, y);
    }

    FVector2F FCamera::GetProjectConstantsAB() const
    {
        FVector2F Constants;
        Constants.x = m_fFarZ / (m_fFarZ - m_fNearZ);
        Constants.y = -(m_fNearZ * m_fFarZ) / (m_fFarZ - m_fNearZ);
        return Constants;
    }


}