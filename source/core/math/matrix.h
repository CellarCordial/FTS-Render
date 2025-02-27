#ifndef MATH_MATRIX_H
#define MATH_MATRIX_H

#include "vector.h"
#include <cassert>
#include <cstdint>

namespace fantasy 
{
    
    template <typename T>
	requires std::is_arithmetic_v<T>
    struct Matrix4x4;
    
    template <typename T>
	requires std::is_arithmetic_v<T>
    struct Matrix3x3
    {
        Matrix3x3();

        explicit Matrix3x3(const T f[3][3]);

        Matrix3x3(const Vector3<T>& x, const Vector3<T>& y, const Vector3<T>& z);

        Matrix3x3(
            T f00, T f01, T f02,
            T f10, T f11, T f12,
            T f20, T f21, T f22
        );

        explicit Matrix3x3(const Matrix4x4<T>& matrix);

        friend Matrix3x3 operator+(const Matrix3x3& matrix1, const Matrix3x3& matrix2);

        template <typename U>
        requires std::is_arithmetic_v<U>
        friend Matrix3x3 operator*(U value, const Matrix3x3& matrix);

        template <typename U>
        requires std::is_arithmetic_v<U>
        friend Matrix3x3 operator*(const Matrix3x3& matrix, U value);

        bool operator==(const Matrix3x3& matrix) const;

        bool operator!=(const Matrix3x3& matrix) const;

        T* operator[](uint32_t ix) { return _data[ix]; }
        const T* operator[](uint32_t ix) const { return _data[ix]; }

        T _data[3][3];
    };

    using int3x3 = Matrix3x3<int32_t>;
    using uint3x3 = Matrix3x3<uint32_t>;
    using float3x3 = Matrix3x3<float>;
    using double3x3 = Matrix3x3<double>;

    template <typename T>
	requires std::is_arithmetic_v<T>
    struct Matrix4x4
    {
        Matrix4x4();
        
        explicit Matrix4x4(const T f[16]);
        
        explicit Matrix4x4(const T f[4][4]);


        Matrix4x4(
            T f00, T f01, T f02, T f03,
            T f10, T f11, T f12, T f13,
            T f20, T f21, T f22, T f23,
            T f30, T f31, T f32, T f33
        );

        explicit Matrix4x4(const Matrix3x3<T>& matrix);

        friend Matrix4x4 operator+(const Matrix4x4& matrix1, const Matrix4x4& matrix2);

        template <typename U>
        requires std::is_arithmetic_v<U>
        friend Matrix4x4 operator*(U value, const Matrix4x4& matrix);

        template <typename U>
        requires std::is_arithmetic_v<U>
        friend Matrix4x4 operator*(const Matrix4x4& matrix, U value);

        bool operator==(const Matrix4x4& matrix) const;

        bool operator!=(const Matrix4x4& matrix) const;

        T* operator[](uint32_t ix) { return _data[ix]; }
        const T* operator[](uint32_t ix) const { return _data[ix]; }

        T _data[4][4];
    };

    using int4x4 = Matrix4x4<int32_t>;
    using uint4x4 = Matrix4x4<uint32_t>;
    using float4x4 = Matrix4x4<float>;
    using double4x4 = Matrix4x4<double>;


    template <typename T>
	requires std::is_arithmetic_v<T>
    struct Matrix3x4
    {
        Matrix3x4();

        explicit Matrix3x4(const T f[3][4]);

        Matrix3x4(
            T f00, T f01, T f02, T f03,
            T f10, T f11, T f12, T f13,
            T f20, T f21, T f22, T f23
        );

        explicit Matrix3x4(const Matrix3x3<T>& matrix);

        friend Matrix3x4 operator+(const Matrix3x4& matrix1, const Matrix3x4& matrix2);

        template <typename U>
        requires std::is_arithmetic_v<U>
        friend Matrix3x4 operator*(U value, const Matrix3x4& matrix);

        template <typename U>
        requires std::is_arithmetic_v<U>
        friend Matrix3x4 operator*(const Matrix3x4& matrix, U value);

        bool operator==(const Matrix3x4& matrix) const;

        bool operator!=(const Matrix3x4& matrix) const;

        T* operator[](uint32_t ix) { return _data[ix]; }
        const T* operator[](uint32_t ix) const { return _data[ix]; }

        T _data[3][4];
    };

    using int3x4 = Matrix3x4<int32_t>;
    using uint3x4 = Matrix3x4<uint32_t>;
    using float3x4 = Matrix3x4<float>;
    using double3x4 = Matrix3x4<double>;
    

	float4x4 rotate(const float3& rotation);
    float4x4 rotate(float theta, const float3& axis);
    float4x4 rotate(float theta, const float3& axis);
    float4x4 rotate_z(float theta);
    float4x4 rotate_y(float theta);
    float4x4 rotate_x(float theta);
    float4x4 translate(const float3& delta);
    float4x4 scale(const float3& scale);
	float4x4 look_at_left_hand(const float3& pos, const float3& look, const float3& up);
    float4x4 perspective_left_hand(float fov_y, float fAspectRatio, float near_z, float far_z);
	float4x4 orthographic_left_hand(float width, float height, float near_z, float far_z);
    float4x4 perspective_left_hand_reverse_z(float fov_y, float fAspectRatio, float near_z, float far_z);
	float3x3 create_orthogonal_basis_from_z(const float3& Z);
	float4x4 look_at_left_hand(const float3& pos, const float3& look, const float3& up);

    template <typename T>
    requires std::is_same_v<T, float> || std::is_same_v<T, double>
    Matrix4x4<T> inverse(const Matrix4x4<T>& matrix)
    {
		uint32_t index_column[4], index_raw[4];
		uint32_t ipiv[4] = { 0, 0, 0, 0 };
		T inv_matrix[4][4];
		memcpy(inv_matrix, matrix._data, 16 * sizeof(T));

		for (uint32_t i = 0; i < 4; ++i)
		{
			uint32_t column_index = 0, raw_index = 0;
			T big = 0.0f;
			for (uint32_t j = 0; j < 4; ++j)
			{
				if (ipiv[j] != 1)
				{
					for (uint32_t k = 0; k < 4; ++k)
					{
						if (ipiv[k] == 0)
						{
							if (std::abs(inv_matrix[j][k]) >= big)
							{
								big = std::abs(inv_matrix[j][k]);
								raw_index = j;
								column_index = k;
							}
						}
						else if (ipiv[k] > 1)
						{
							assert(!"It is a Singular matrix which can't be used in MatrixInvert");
						}
					}
				}
			}
			ipiv[column_index]++;

			if (column_index != raw_index)
			{
				for (uint32_t k = 0; k < 4; ++k)
				{
					std::swap(inv_matrix[raw_index][k], inv_matrix[column_index][k]);
				}
			}
			index_column[i] = column_index;
			index_raw[i] = raw_index;

			if (inv_matrix[column_index][column_index] == 0.0f)
			{
				assert(!"It is a Singular matrix which can't be used in MatrixInvert");
			}

			const T inv_piv = 1.0f / inv_matrix[column_index][column_index];
			inv_matrix[column_index][column_index] = 1.0f;

			for (uint32_t j = 0; j < 4; j++)
			{
				inv_matrix[column_index][j] *= inv_piv;
			}

			for (uint32_t j = 0; j < 4; j++)
			{
				if (j != column_index)
				{
					const T fSave = inv_matrix[j][column_index];
					inv_matrix[j][column_index] = 0;
					for (int k = 0; k < 4; k++)
					{
						inv_matrix[j][k] -= inv_matrix[column_index][k] * fSave;
					}
				}
			}
		}

		for (int j = 3; j >= 0; j--)
		{
			if (index_raw[j] != index_column[j])
			{
				for (int k = 0; k < 4; k++)
					std::swap(inv_matrix[k][index_raw[j]], inv_matrix[k][index_column[j]]);
			}
		}

        return Matrix4x4<T>{ inv_matrix };
    }


    template <typename T>
    requires std::is_same_v<T, float> || std::is_same_v<T, double>
    bool inverse_row_major(T m[16], T out_inv[16])
    {
        T inv[16], delta;
        
    
        inv[ 0] =  m[5] * m[10] * m[15] - m[5] * m[14] * m[11] - m[6] * m[9] * m[15] + m[6] * m[13] * m[11] + m[7] * m[9] * m[14] - m[7] * m[13] * m[10];
        inv[ 1] = -m[1] * m[10] * m[15] + m[1] * m[14] * m[11] + m[2] * m[9] * m[15] - m[2] * m[13] * m[11] - m[3] * m[9] * m[14] + m[3] * m[13] * m[10];
        inv[ 2] =  m[1] * m[ 6] * m[15] - m[1] * m[14] * m[ 7] - m[2] * m[5] * m[15] + m[2] * m[13] * m[ 7] + m[3] * m[5] * m[14] - m[3] * m[13] * m[ 6];
        inv[ 3] = -m[1] * m[ 6] * m[11] + m[1] * m[10] * m[ 7] + m[2] * m[5] * m[11] - m[2] * m[ 9] * m[ 7] - m[3] * m[5] * m[10] + m[3] * m[ 9] * m[ 6];
        inv[ 4] = -m[4] * m[10] * m[15] + m[4] * m[14] * m[11] + m[6] * m[8] * m[15] - m[6] * m[12] * m[11] - m[7] * m[8] * m[14] + m[7] * m[12] * m[10];
        inv[ 5] =  m[0] * m[10] * m[15] - m[0] * m[14] * m[11] - m[2] * m[8] * m[15] + m[2] * m[12] * m[11] + m[3] * m[8] * m[14] - m[3] * m[12] * m[10];
        inv[ 6] = -m[0] * m[ 6] * m[15] + m[0] * m[14] * m[ 7] + m[2] * m[4] * m[15] - m[2] * m[12] * m[ 7] - m[3] * m[4] * m[14] + m[3] * m[12] * m[ 6];
        inv[ 7] =  m[0] * m[ 6] * m[11] - m[0] * m[10] * m[ 7] - m[2] * m[4] * m[11] + m[2] * m[ 8] * m[ 7] + m[3] * m[4] * m[10] - m[3] * m[ 8] * m[ 6];
        inv[ 8] =  m[4] * m[ 9] * m[15] - m[4] * m[13] * m[11] - m[5] * m[8] * m[15] + m[5] * m[12] * m[11] + m[7] * m[8] * m[13] - m[7] * m[12] * m[ 9];
        inv[ 9] = -m[0] * m[ 9] * m[15] + m[0] * m[13] * m[11] + m[1] * m[8] * m[15] - m[1] * m[12] * m[11] - m[3] * m[8] * m[13] + m[3] * m[12] * m[ 9];
        inv[10] =  m[0] * m[ 5] * m[15] - m[0] * m[13] * m[ 7] - m[1] * m[4] * m[15] + m[1] * m[12] * m[ 7] + m[3] * m[4] * m[13] - m[3] * m[12] * m[ 5];
        inv[11] = -m[0] * m[ 5] * m[11] + m[0] * m[ 9] * m[ 7] + m[1] * m[4] * m[11] - m[1] * m[ 8] * m[ 7] - m[3] * m[4] * m[ 9] + m[3] * m[ 8] * m[ 5];
        inv[12] = -m[4] * m[ 9] * m[14] + m[4] * m[13] * m[10] + m[5] * m[8] * m[14] - m[5] * m[12] * m[10] - m[6] * m[8] * m[13] + m[6] * m[12] * m[ 9];
        inv[13] =  m[0] * m[ 9] * m[14] - m[0] * m[13] * m[10] - m[1] * m[8] * m[14] + m[1] * m[12] * m[10] + m[2] * m[8] * m[13] - m[2] * m[12] * m[ 9];
        inv[14] = -m[0] * m[ 5] * m[14] + m[0] * m[13] * m[ 6] + m[1] * m[4] * m[14] - m[1] * m[12] * m[ 6] - m[2] * m[4] * m[13] + m[2] * m[12] * m[ 5];
        inv[15] =  m[0] * m[ 5] * m[10] - m[0] * m[ 9] * m[ 6] - m[1] * m[4] * m[10] + m[1] * m[ 8] * m[ 6] + m[2] * m[4] * m[ 9] - m[2] * m[ 8] * m[ 5];
    
        delta = m[0] * inv[0] + m[4] * inv[1] + m[8] * inv[2] + m[12] * inv[3];
    
        if(delta == 0) return false;
        delta = 1.f / delta;
    
        for(uint32_t i = 0; i < 16; i++) out_inv[i] = inv[i] * delta;
    
        return true;
    }

    template <typename T>
    requires std::is_same_v<T, float> || std::is_same_v<T, double>
    bool inverse_column_major(T m[16], T inv_m[16])
    {
        double inv[16], det;
        int i;
    
        inv[ 0] =  m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        inv[ 4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        inv[ 8] =  m[4] * m[ 9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[ 9];
        inv[12] = -m[4] * m[ 9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[ 9];
        inv[ 1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv[ 5] =  m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv[ 9] = -m[0] * m[ 9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[ 9];
        inv[13] =  m[0] * m[ 9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[ 9];
        inv[ 2] =  m[1] * m[ 6] * m[15] - m[1] * m[ 7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[ 7] - m[13] * m[3] * m[ 6];
        inv[ 6] = -m[0] * m[ 6] * m[15] + m[0] * m[ 7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[ 7] + m[12] * m[3] * m[ 6];
        inv[10] =  m[0] * m[ 5] * m[15] - m[0] * m[ 7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[ 7] - m[12] * m[3] * m[ 5];
        inv[14] = -m[0] * m[ 5] * m[14] + m[0] * m[ 6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[ 6] + m[12] * m[2] * m[ 5];
        inv[ 3] = -m[1] * m[ 6] * m[11] + m[1] * m[ 7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[ 9] * m[2] * m[ 7] + m[ 9] * m[3] * m[ 6];
        inv[ 7] =  m[0] * m[ 6] * m[11] - m[0] * m[ 7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[ 8] * m[2] * m[ 7] - m[ 8] * m[3] * m[ 6];
        inv[11] = -m[0] * m[ 5] * m[11] + m[0] * m[ 7] * m[ 9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[ 9] - m[ 8] * m[1] * m[ 7] + m[ 8] * m[3] * m[ 5];
        inv[15] =  m[0] * m[ 5] * m[10] - m[0] * m[ 6] * m[ 9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[ 9] + m[ 8] * m[1] * m[ 6] - m[ 8] * m[2] * m[ 5];
    
        det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    
        if(det == 0) return false;
    
        det = 1.f / det;
    
        for(i = 0; i < 16; i++)
            inv_m[i] = inv[i] * det;
     
        return true;
    }
    
    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x3<T>::Matrix3x3()
    {
        // 单位矩阵
        _data[0][0] = _data[1][1] = _data[2][2] = 1.0f;
        _data[0][1] = _data[0][2] = _data[1][0] = _data[1][2] = _data[2][0] = _data[2][1] = 0.0f;
    };

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x3<T>::Matrix3x3(const T f[3][3])
    {
        memcpy(_data, f, sizeof(T) * 9);
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x3<T>::Matrix3x3(
        T f00, T f01, T f02,
        T f10, T f11, T f12,
        T f20, T f21, T f22
    )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
    {
        _data[0][0] = f00; _data[0][1] = f01; _data[0][2] = f02;
        _data[1][0] = f10; _data[1][1] = f11; _data[1][2] = f12;
        _data[2][0] = f20; _data[2][1] = f21; _data[2][2] = f22;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x3<T>::Matrix3x3(const Matrix4x4<T>& matrix)
    {
        _data[0][0] = matrix[0][0]; _data[0][1] = matrix[0][1]; _data[0][2] = matrix[0][2];
        _data[1][0] = matrix[1][0]; _data[1][1] = matrix[1][1]; _data[1][2] = matrix[1][2];
        _data[2][0] = matrix[2][0]; _data[2][1] = matrix[2][1]; _data[2][2] = matrix[2][2];
    }


    template <typename T>
	requires std::is_arithmetic_v<T>
	Matrix3x3<T>::Matrix3x3(const Vector3<T>& x, const Vector3<T>& y, const Vector3<T>& z)
	{
		_data[0][0] = x[0]; _data[0][1] = x[1]; _data[0][2] = x[2];
		_data[1][0] = y[0]; _data[1][1] = y[1]; _data[1][2] = y[2];
		_data[2][0] = z[0]; _data[2][1] = z[1]; _data[2][2] = z[2];
	}

    template <typename T>
	requires std::is_arithmetic_v<T>
	Matrix3x3<T> operator+(const Matrix3x3<T>& matrix1, const Matrix3x3<T>& matrix2)
    {
        Matrix3x3<T> ret;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
                ret._data[i][j] = matrix1._data[i][j] + matrix2._data[i][j];
        return ret;
    }

    template <typename T, typename U>
    requires std::is_arithmetic_v<U>
    Matrix3x3<T> operator*(U value, const Matrix3x3<T>& matrix)
    {
        Matrix3x3<T> ret = matrix;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
                ret._data[i][j] *= static_cast<T>(value);
        return ret;
    }

    template <typename T, typename U>
    requires std::is_arithmetic_v<U>
    Matrix3x3<T> operator*(const Matrix3x3<T>& matrix, U value)
    {
        Matrix3x3<T> ret = matrix;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
                ret._data[i][j] *= value;
        return ret;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    bool Matrix3x3<T>::operator==(const Matrix3x3<T>& matrix) const
    {
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
            {
                // _data[i][j] != matrix._data[i][j]
                const auto temp = _data[i][j] - matrix._data[i][j];
                if (NOT_FLOAT_ZERO(temp))
                    return false;
            }
        return true;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    bool Matrix3x3<T>::operator!=(const Matrix3x3<T>& matrix) const
    {
        return !((*this) == matrix);
    }

    
    
    template <typename T>
    inline Matrix3x3<T> transpose(const Matrix3x3<T>& matrix)
    {
        const auto& f = matrix._data;
        return Matrix3x3(
            f[0][0], f[1][0], f[2][0],
            f[0][1], f[1][1], f[2][1],
            f[0][2], f[1][2], f[2][2]
        );
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x3<T> mul(const Matrix3x3<T>& matrix1, const Matrix3x3<T>& matrix2)
    {
        Matrix3x3<T> ret;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
                ret._data[i][j] =  matrix1._data[i][0] * matrix2._data[0][j] +
                                matrix1._data[i][1] * matrix2._data[1][j] +
                                matrix1._data[i][2] * matrix2._data[2][j];
        return ret;
    }

    
    template <typename T>
    Vector3<T> mul(const Matrix3x3<T>& matrix, const Vector3<T>& crVec)
    {
        Vector3<T> ret;
        for (uint32_t i = 0; i < 3; ++i)
                ret[i] =  matrix._data[i][0] * crVec[0] +
                          matrix._data[i][1] * crVec[1] +
                          matrix._data[i][2] * crVec[2];
        return ret;
    }

    template <typename T>
    Vector3<T> mul(const Vector3<T>& crVec, const Matrix3x3<T>& matrix)
    {
        Vector3<T> ret;
        for (uint32_t i = 0; i < 3; ++i)
                ret[i] =  crVec[0] * matrix._data[0][i] +
                          crVec[1] * matrix._data[1][i] +
                          crVec[2] * matrix._data[2][i];
        return ret;
    }

    
    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix4x4<T>::Matrix4x4()
    {
        // 单位矩阵
        _data[0][0] = _data[1][1] = _data[2][2] = _data[3][3] = 1.0f;
        _data[0][1] = _data[0][2] = _data[0][3] = _data[1][0] = _data[1][2] = _data[1][3] =
        _data[2][0] = _data[2][1] = _data[2][3] = _data[3][0] = _data[3][1] = _data[3][2] = 0.0f;
    };

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix4x4<T>::Matrix4x4(const T f[16])
    {
        memcpy(_data, f, sizeof(T) * 16);
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix4x4<T>::Matrix4x4(const T f[4][4])
    {
        memcpy(_data, f, sizeof(T) * 16);
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix4x4<T>::Matrix4x4(
        T f00, T f01, T f02, T f03,
        T f10, T f11, T f12, T f13,
        T f20, T f21, T f22, T f23,
        T f30, T f31, T f32, T f33
    )
    {
        _data[0][0] = f00; _data[0][1] = f01; _data[0][2] = f02; _data[0][3] = f03;
        _data[1][0] = f10; _data[1][1] = f11; _data[1][2] = f12; _data[1][3] = f13;
        _data[2][0] = f20; _data[2][1] = f21; _data[2][2] = f22; _data[2][3] = f23;
        _data[3][0] = f30; _data[3][1] = f31; _data[3][2] = f32; _data[3][3] = f33;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix4x4<T>::Matrix4x4(const Matrix3x3<T>& matrix)
    {
        _data[0][0] = matrix[0][0]; _data[0][1] = matrix[0][1]; _data[0][2] = matrix[0][2]; _data[0][3] = 0.0f;
        _data[1][0] = matrix[1][0]; _data[1][1] = matrix[1][1]; _data[1][2] = matrix[1][2]; _data[1][3] = 0.0f;
        _data[2][0] = matrix[2][0]; _data[2][1] = matrix[2][1]; _data[2][2] = matrix[2][2]; _data[2][3] = 0.0f;
        _data[3][0] = 0.0f;           _data[3][1] = 0.0f;           _data[3][2] = 0.0f;           _data[3][3] = 1.0f;
    }


    template <typename T>
    Matrix4x4<T> operator+(const Matrix4x4<T>& matrix1, const Matrix4x4<T>& matrix2)
    {
        Matrix4x4<T> ret;
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] = matrix1._data[i][j] + matrix2._data[i][j];
        return ret;
    }

    template <typename T, typename U>
    Matrix4x4<T> operator*(U value, const Matrix4x4<T>& matrix)
    {
        Matrix4x4<T> ret = matrix;
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] *= static_cast<T>(value);
        return ret;
    }

    template <typename T, typename U>
    Matrix4x4<T> operator*(const Matrix4x4<T>& matrix, U value)
    {
        Matrix4x4 ret = matrix;
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] *= static_cast<T>(value);
        return ret;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    bool Matrix4x4<T>::operator==(const Matrix4x4<T>& matrix) const
    {
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
            {
                // _data[i][j] != matrix._data[i][j]
                const auto temp = _data[i][j] - matrix._data[i][j];
                if (NOT_FLOAT_ZERO(temp))
                    return false;
            }
        return true;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    bool Matrix4x4<T>::operator!=(const Matrix4x4<T>& matrix) const
    {
        return !((*this) == matrix);
    }
    
    template <typename T>
    Matrix4x4<T> transpose(const Matrix4x4<T>& matrix)
    {
        const auto& f = matrix._data;
        return Matrix4x4<T>(
            f[0][0], f[1][0], f[2][0], f[3][0],
            f[0][1], f[1][1], f[2][1], f[3][1],
            f[0][2], f[1][2], f[2][2], f[3][2],
            f[0][3], f[1][3], f[2][3], f[3][3]
        );
    }


    template <typename T>
    Matrix4x4<T> mul(const Matrix4x4<T>& matrix1, const Matrix4x4<T>& matrix2)
    {
        Matrix4x4<T> ret;
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] =  matrix1._data[i][0] * matrix2._data[0][j] +
                                matrix1._data[i][1] * matrix2._data[1][j] +
                                matrix1._data[i][2] * matrix2._data[2][j] +
                                matrix1._data[i][3] * matrix2._data[3][j];
        return ret;
    }

        
    template <typename T>
    Vector4<T> mul(const Matrix4x4<T>& matrix, const Vector4<T>& crVec)
    {
        Vector4<T> ret;
        for (uint32_t i = 0; i < 4; ++i)
                ret[i] =  matrix._data[i][0] * crVec[0] +
                          matrix._data[i][1] * crVec[1] +
                          matrix._data[i][2] * crVec[2] +
                          matrix._data[i][3] * crVec[3];
        return ret;
    }

    template <typename T>
    Vector4<T> mul(const Vector4<T>& crVec, const Matrix4x4<T>& matrix)
    {
        Vector4<T> ret;
        for (uint32_t i = 0; i < 4; ++i)
                ret[i] =  crVec[0] * matrix._data[0][i] +
                          crVec[1] * matrix._data[1][i] +
                          crVec[2] * matrix._data[2][i] + 
                          crVec[3] * matrix._data[3][i];
        return ret;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x4<T>::Matrix3x4()
    {
        // 单位矩阵
        _data[0][0] = _data[1][1] = _data[2][2] = 1.0f;
        
        _data[0][1] = _data[0][2] = _data[0][3] = 
        _data[1][0] = _data[1][2] = _data[1][3] =
        _data[2][0] = _data[2][1] = _data[2][3] = 0.0f;
    };

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x4<T>::Matrix3x4(const T f[3][4])
    {
        memcpy(_data, f, sizeof(T) * 12);
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x4<T>::Matrix3x4(
        T f00, T f01, T f02, T f03,
        T f10, T f11, T f12, T f13,
        T f20, T f21, T f22, T f23
    )
    {
        _data[0][0] = f00; _data[0][1] = f01; _data[0][2] = f02; _data[0][3] = f03;
        _data[1][0] = f10; _data[1][1] = f11; _data[1][2] = f12; _data[1][3] = f13;
        _data[2][0] = f20; _data[2][1] = f21; _data[2][2] = f22; _data[2][3] = f23;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    Matrix3x4<T>::Matrix3x4(const Matrix3x3<T>& matrix)
    {
        _data[0][0] = matrix[0][0]; _data[0][1] = matrix[0][1]; _data[0][2] = matrix[0][2]; _data[0][3] = 0.0f;
        _data[1][0] = matrix[1][0]; _data[1][1] = matrix[1][1]; _data[1][2] = matrix[1][2]; _data[1][3] = 0.0f;
        _data[2][0] = matrix[2][0]; _data[2][1] = matrix[2][1]; _data[2][2] = matrix[2][2]; _data[2][3] = 0.0f;
    }


    template <typename T>
    Matrix3x4<T> operator+(const Matrix3x4<T>& matrix1, const Matrix3x4<T>& matrix2)
    {
        Matrix3x4<T> ret;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] = matrix1._data[i][j] + matrix2._data[i][j];
        return ret;
    }

    template <typename T, typename U>
    requires std::is_arithmetic_v<U>
    Matrix3x4<T> operator*(U value, const Matrix3x4<T>& matrix)
    {
        Matrix3x4<T> ret = matrix;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] *= static_cast<U>(value);
        return ret;
    }

    template <typename T, typename U>
    requires std::is_arithmetic_v<U>
    Matrix3x4<T> operator*(const Matrix3x4<T>& matrix, U value)
    {
        Matrix3x4<T> ret = matrix;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] *= value;
        return ret;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    bool Matrix3x4<T>::operator==(const Matrix3x4<T>& matrix) const
    {
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 4; ++j)
            {
                // _data[i][j] != matrix._data[i][j]
                const auto temp = _data[i][j] - matrix._data[i][j];
                if (NOT_FLOAT_ZERO(temp))
                    return false;
            }
        return true;
    }

    template <typename T>
	requires std::is_arithmetic_v<T>
    bool Matrix3x4<T>::operator!=(const Matrix3x4<T>& matrix) const
    {
        return !((*this) == matrix);
    }

}




#endif