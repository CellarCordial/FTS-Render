#ifndef MATH_MATRIX_H
#define MATH_MATRIX_H

#include "vector.h"

namespace fantasy 
{
    struct Matrix4x4;
    
    struct Matrix3x3
    {
        Matrix3x3();

        explicit Matrix3x3(const float f[3][3]);

        Matrix3x3(const Vector3F& x, const Vector3F& y, const Vector3F& z);

        Matrix3x3(
            float f00, float f01, float f02,
            float f10, float f11, float f12,
            float f20, float f21, float f22
        );

        explicit Matrix3x3(const Matrix4x4& matrix);

        friend Matrix3x3 operator+(const Matrix3x3& matrix1, const Matrix3x3& matrix2);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend Matrix3x3 operator*(T value, const Matrix3x3& matrix);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend Matrix3x3 operator*(const Matrix3x3& matrix, T value);

        bool operator==(const Matrix3x3& matrix) const;

        bool operator!=(const Matrix3x3& matrix) const;

        float* operator[](uint32_t ix) { return _data[ix]; }
        const float* operator[](uint32_t ix) const { return _data[ix]; }

        float _data[3][3];
    };

    
    Matrix3x3 mul(const Matrix3x3& matrix1, const Matrix3x3& matrix2);

    Vector3F mul(const Matrix3x3& matrix, const Vector3F& vec);

    Vector3F mul(const Vector3F& vec, const Matrix3x3& matrix);

    inline Matrix3x3 transpose(const Matrix3x3& matrix)
    {
        const auto& f = matrix._data;
        return Matrix3x3(
            f[0][0], f[1][0], f[2][0],
            f[0][1], f[1][1], f[2][1],
            f[0][2], f[1][2], f[2][2]
        );
    }

    struct Matrix4x4
    {
        Matrix4x4();

        explicit Matrix4x4(const float f[4][4]);

        Matrix4x4(
            float f00, float f01, float f02, float f03,
            float f10, float f11, float f12, float f13,
            float f20, float f21, float f22, float f23,
            float f30, float f31, float f32, float f33
        );

        explicit Matrix4x4(const Matrix3x3& matrix);

        friend Matrix4x4 operator+(const Matrix4x4& matrix1, const Matrix4x4& matrix2);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend Matrix4x4 operator*(T value, const Matrix4x4& matrix);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend Matrix4x4 operator*(const Matrix4x4& matrix, T value);

        bool operator==(const Matrix4x4& matrix) const;

        bool operator!=(const Matrix4x4& matrix) const;

        float* operator[](uint32_t ix) { return _data[ix]; }
        const float* operator[](uint32_t ix) const { return _data[ix]; }

        float _data[4][4];
    };

    Matrix4x4 mul(const Matrix4x4& matrix1, const Matrix4x4& matrix2);
    
    Vector4F mul(const Matrix4x4& matrix, const Vector4F& vec);

    Vector4F mul(const Vector4F& vec, const Matrix4x4& matrix);

    inline Matrix4x4 transpose(const Matrix4x4& matrix)
    {
        const auto& f = matrix._data;
        return Matrix4x4(
            f[0][0], f[1][0], f[2][0], f[3][0],
            f[0][1], f[1][1], f[2][1], f[3][1],
            f[0][2], f[1][2], f[2][2], f[3][2],
            f[0][3], f[1][3], f[2][3], f[3][3]
        );
    }

    Matrix4x4 inverse(const Matrix4x4& matrix);
    bool invertible(const Matrix4x4& matrix, Matrix4x4& rInvMatrix);


    
    struct Matrix3x4
    {
        Matrix3x4();

        explicit Matrix3x4(const float f[3][4]);

        Matrix3x4(
            float f00, float f01, float f02, float f03,
            float f10, float f11, float f12, float f13,
            float f20, float f21, float f22, float f23
        );

        explicit Matrix3x4(const Matrix3x3& matrix);

        friend Matrix3x4 operator+(const Matrix3x4& matrix1, const Matrix3x4& matrix2);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend Matrix3x4 operator*(T value, const Matrix3x4& matrix);

        template <typename T>
        requires std::is_arithmetic_v<T>
        friend Matrix3x4 operator*(const Matrix3x4& matrix, T value);

        bool operator==(const Matrix3x4& matrix) const;

        bool operator!=(const Matrix3x4& matrix) const;

        float* operator[](uint32_t ix) { return _data[ix]; }
        const float* operator[](uint32_t ix) const { return _data[ix]; }

        float _data[3][4];
    };



    // 返回 crDelta 表示的变换
    Matrix4x4 translate(const Vector3F& crDelta);

    // 返回 crScale 表示的变换
    Matrix4x4 scale(const Vector3F& crScale);
    
    // 返回绕 X 轴旋转 theta 角度的变换
    Matrix4x4 rotate_x(float theta);

    // 返回绕 Y 轴旋转 theta 角度的变换
    Matrix4x4 rotate_y(float theta);

    // 返回绕 Z 轴旋转 theta 角度的变换
    Matrix4x4 rotate_z(float theta);

	Matrix4x4 rotate(float theta, const Vector3F& crAxis);
	
    Matrix4x4 rotate(const Vector3F& crRotation);

    Matrix4x4 look_at_left_hand(const Vector3F& crPos, const Vector3F& crLook, const Vector3F& crUp);
    Matrix4x4 orthographic_left_hand(float fWidth, float fHeight, float fNearZ, float fFarZ);
    Matrix4x4 perspective_left_hand(float FovAngleY, float AspectRatio, float NearZ, float FarZ);
    Matrix3x3 create_orthogonal_basis_from_z(const Vector3F& Z);
}




#endif