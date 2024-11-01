#ifndef MATH_MATRIX_H
#define MATH_MATRIX_H

#include "Vector.h"

namespace FTS 
{
    struct FMatrix4x4;
    
    struct FMatrix3x3
    {
        FMatrix3x3();

        explicit FMatrix3x3(const FLOAT cpf[3][3]);

        FMatrix3x3(const FVector3F& x, const FVector3F& y, const FVector3F& z);

        FMatrix3x3(
            FLOAT f00, FLOAT f01, FLOAT f02,
            FLOAT f10, FLOAT f11, FLOAT f12,
            FLOAT f20, FLOAT f21, FLOAT f22
        );

        explicit FMatrix3x3(const FMatrix4x4& crMatrix);

        friend FMatrix3x3 operator+(const FMatrix3x3& crMatrix1, const FMatrix3x3& crMatrix2);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend FMatrix3x3 operator*(T Value, const FMatrix3x3& crMatrix);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend FMatrix3x3 operator*(const FMatrix3x3& crMatrix, T Value);

        BOOL operator==(const FMatrix3x3& crMatrix) const;

        BOOL operator!=(const FMatrix3x3& crMatrix) const;

        FLOAT* operator[](UINT32 ix) { return m_pf[ix]; }
        const FLOAT* operator[](UINT32 ix) const { return m_pf[ix]; }

        FLOAT m_pf[3][3];
    };

    
    FMatrix3x3 Mul(const FMatrix3x3& crMatrix1, const FMatrix3x3& crMatrix2);

    FVector3F Mul(const FMatrix3x3& crMatrix, const FVector3F& crVec);

    FVector3F Mul(const FVector3F& crVec, const FMatrix3x3& crMatrix);

    inline FMatrix3x3 Transpose(const FMatrix3x3& crMatrix)
    {
        const auto& f = crMatrix.m_pf;
        return FMatrix3x3(
            f[0][0], f[1][0], f[2][0],
            f[0][1], f[1][1], f[2][1],
            f[0][2], f[1][2], f[2][2]
        );
    }

    // FMatrix3x3 Inverse(const FMatrix3x3& crMatrix);

    // FMatrix3x3 Lerp(FLOAT f, const FMatrix3x3& crMatrix1, const FMatrix3x3& crMatrix2);

    
    struct FMatrix4x4
    {
        FMatrix4x4();

        explicit FMatrix4x4(const FLOAT cpf[4][4]);

        FMatrix4x4(
            FLOAT f00, FLOAT f01, FLOAT f02, FLOAT f03,
            FLOAT f10, FLOAT f11, FLOAT f12, FLOAT f13,
            FLOAT f20, FLOAT f21, FLOAT f22, FLOAT f23,
            FLOAT f30, FLOAT f31, FLOAT f32, FLOAT f33
        );

        explicit FMatrix4x4(const FMatrix3x3& crMatrix);

        friend FMatrix4x4 operator+(const FMatrix4x4& crMatrix1, const FMatrix4x4& crMatrix2);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend FMatrix4x4 operator*(T Value, const FMatrix4x4& crMatrix);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend FMatrix4x4 operator*(const FMatrix4x4& crMatrix, T Value);

        BOOL operator==(const FMatrix4x4& crMatrix) const;

        BOOL operator!=(const FMatrix4x4& crMatrix) const;

        FLOAT* operator[](UINT32 ix) { return m_pf[ix]; }
        const FLOAT* operator[](UINT32 ix) const { return m_pf[ix]; }

        FLOAT m_pf[4][4];
    };

    FMatrix4x4 Mul(const FMatrix4x4& crMatrix1, const FMatrix4x4& crMatrix2);
    
    FVector4F Mul(const FMatrix4x4& crMatrix, const FVector4F& crVec);

    FVector4F Mul(const FVector4F& crVec, const FMatrix4x4& crMatrix);

    inline FMatrix4x4 Transpose(const FMatrix4x4& crMatrix)
    {
        const auto& f = crMatrix.m_pf;
        return FMatrix4x4(
            f[0][0], f[1][0], f[2][0], f[3][0],
            f[0][1], f[1][1], f[2][1], f[3][1],
            f[0][2], f[1][2], f[2][2], f[3][2],
            f[0][3], f[1][3], f[2][3], f[3][3]
        );
    }

    FMatrix4x4 Inverse(const FMatrix4x4& crMatrix);



    // 返回 crDelta 表示的变换
    FMatrix4x4 Translate(const FVector3F& crDelta);

    // 返回 crScale 表示的变换
    FMatrix4x4 Scale(const FVector3F& crScale);
    
    // 返回绕 X 轴旋转 fTheta 角度的变换
    FMatrix4x4 RotateX(FLOAT fTheta/*degrees*/);

    // 返回绕 Y 轴旋转 fTheta 角度的变换
    FMatrix4x4 RotateY(FLOAT fTheta/*degrees*/);

    // 返回绕 Z 轴旋转 fTheta 角度的变换
    FMatrix4x4 RotateZ(FLOAT fTheta/*degrees*/);

    /**
     * @brief       返回绕任意给定向量旋转 fTheta 角度的变换

     * @param fTheta 角度制
     * 
     */
    FMatrix4x4 Rotate(FLOAT fTheta/*degrees*/, const FVector3F& crAxis);

    FMatrix4x4 LookAtLeftHand(const FVector3F& crPos, const FVector3F& crLook, const FVector3F& crUp);
    FMatrix4x4 OrthographicLeftHand(FLOAT fWidth, FLOAT fHeight, FLOAT fNearZ, FLOAT fFarZ);
    FMatrix4x4 PerspectiveLeftHand(FLOAT FovAngleY, FLOAT AspectRatio, FLOAT NearZ, FLOAT FarZ);
    FMatrix4x4 PerspectiveLeftHandInverseDepth(FLOAT fFovAngleY, FLOAT fAspectRatio, FLOAT fNearZ, FLOAT fFarZ);
    FMatrix3x3 CreateOrthogonalBasisFromZ(const FVector3F& Z);
}




#endif