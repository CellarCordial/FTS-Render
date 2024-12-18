#include "matrix.h"
#include <cassert>

namespace fantasy 
{
    Matrix3x3::Matrix3x3()
    {
        // 单位矩阵
        _data[0][0] = _data[1][1] = _data[2][2] = 1.0f;
        _data[0][1] = _data[0][2] = _data[1][0] = _data[1][2] = _data[2][0] = _data[2][1] = 0.0f;
    };

    Matrix3x3::Matrix3x3(const float f[3][3])
    {
        memcpy(_data, f, sizeof(float) * 9);
    }

    Matrix3x3::Matrix3x3(
        float f00, float f01, float f02,
        float f10, float f11, float f12,
        float f20, float f21, float f22
    )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
    {
        _data[0][0] = f00; _data[0][1] = f01; _data[0][2] = f02;
        _data[1][0] = f10; _data[1][1] = f11; _data[1][2] = f12;
        _data[2][0] = f20; _data[2][1] = f21; _data[2][2] = f22;
    }

    Matrix3x3::Matrix3x3(const Matrix4x4& matrix)
    {
        _data[0][0] = matrix[0][0]; _data[0][1] = matrix[0][1]; _data[0][2] = matrix[0][2];
        _data[1][0] = matrix[1][0]; _data[1][1] = matrix[1][1]; _data[1][2] = matrix[1][2];
        _data[2][0] = matrix[2][0]; _data[2][1] = matrix[2][1]; _data[2][2] = matrix[2][2];
    }


	Matrix3x3::Matrix3x3(const Vector3F& x, const Vector3F& y, const Vector3F& z)
	{
		_data[0][0] = x[0]; _data[0][1] = x[1]; _data[0][2] = x[2];
		_data[1][0] = y[0]; _data[1][1] = y[1]; _data[1][2] = y[2];
		_data[2][0] = z[0]; _data[2][1] = z[1]; _data[2][2] = z[2];
	}

	Matrix3x3 operator+(const Matrix3x3& matrix1, const Matrix3x3& matrix2)
    {
        Matrix3x3 ret;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
                ret._data[i][j] = matrix1._data[i][j] + matrix2._data[i][j];
        return ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    Matrix3x3 operator*(T value, const Matrix3x3& matrix)
    {
        Matrix3x3 ret = matrix;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
                ret._data[i][j] *= static_cast<float>(value);
        return ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    Matrix3x3 operator*(const Matrix3x3& matrix, T value)
    {
        Matrix3x3 ret = matrix;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
                ret._data[i][j] *= value;
        return ret;
    }

    bool Matrix3x3::operator==(const Matrix3x3& matrix) const
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

    bool Matrix3x3::operator!=(const Matrix3x3& matrix) const
    {
        return !((*this) == matrix);
    }

    Matrix3x3 mul(const Matrix3x3& matrix1, const Matrix3x3& matrix2)
    {
        Matrix3x3 ret;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 3; ++j)
                ret._data[i][j] =  matrix1._data[i][0] * matrix2._data[0][j] +
                                matrix1._data[i][1] * matrix2._data[1][j] +
                                matrix1._data[i][2] * matrix2._data[2][j];
        return ret;
    }

    
    Vector3F mul(const Matrix3x3& matrix, const Vector3F& crVec)
    {
        Vector3F ret;
        for (uint32_t i = 0; i < 3; ++i)
                ret[i] =  matrix._data[i][0] * crVec[0] +
                          matrix._data[i][1] * crVec[1] +
                          matrix._data[i][2] * crVec[2];
        return ret;
    }

    Vector3F mul(const Vector3F& crVec, const Matrix3x3& matrix)
    {
        Vector3F ret;
        for (uint32_t i = 0; i < 3; ++i)
                ret[i] =  crVec[0] * matrix._data[0][i] +
                          crVec[1] * matrix._data[1][i] +
                          crVec[2] * matrix._data[2][i];
        return ret;
    }

    
    Matrix4x4::Matrix4x4()
    {
        // 单位矩阵
        _data[0][0] = _data[1][1] = _data[2][2] = _data[3][3] = 1.0f;
        _data[0][1] = _data[0][2] = _data[0][3] = _data[1][0] = _data[1][2] = _data[1][3] =
        _data[2][0] = _data[2][1] = _data[2][3] = _data[3][0] = _data[3][1] = _data[3][2] = 0.0f;
    };

    Matrix4x4::Matrix4x4(const float f[4][4])
    {
        memcpy(_data, f, sizeof(float) * 16);
    }

    Matrix4x4::Matrix4x4(
        float f00, float f01, float f02, float f03,
        float f10, float f11, float f12, float f13,
        float f20, float f21, float f22, float f23,
        float f30, float f31, float f32, float f33
    )
    {
        _data[0][0] = f00; _data[0][1] = f01; _data[0][2] = f02; _data[0][3] = f03;
        _data[1][0] = f10; _data[1][1] = f11; _data[1][2] = f12; _data[1][3] = f13;
        _data[2][0] = f20; _data[2][1] = f21; _data[2][2] = f22; _data[2][3] = f23;
        _data[3][0] = f30; _data[3][1] = f31; _data[3][2] = f32; _data[3][3] = f33;
    }

    Matrix4x4::Matrix4x4(const Matrix3x3& matrix)
    {
        _data[0][0] = matrix[0][0]; _data[0][1] = matrix[0][1]; _data[0][2] = matrix[0][2]; _data[0][3] = 0.0f;
        _data[1][0] = matrix[1][0]; _data[1][1] = matrix[1][1]; _data[1][2] = matrix[1][2]; _data[1][3] = 0.0f;
        _data[2][0] = matrix[2][0]; _data[2][1] = matrix[2][1]; _data[2][2] = matrix[2][2]; _data[2][3] = 0.0f;
        _data[3][0] = 0.0f;           _data[3][1] = 0.0f;           _data[3][2] = 0.0f;           _data[3][3] = 1.0f;
    }


    Matrix4x4 operator+(const Matrix4x4& matrix1, const Matrix4x4& matrix2)
    {
        Matrix4x4 ret;
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] = matrix1._data[i][j] + matrix2._data[i][j];
        return ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    Matrix4x4 operator*(T value, const Matrix4x4& matrix)
    {
        Matrix4x4 ret = matrix;
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] *= static_cast<float>(value);
        return ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    Matrix4x4 operator*(const Matrix4x4& matrix, T value)
    {
        Matrix4x4 ret = matrix;
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] *= value;
        return ret;
    }

    bool Matrix4x4::operator==(const Matrix4x4& matrix) const
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

    bool Matrix4x4::operator!=(const Matrix4x4& matrix) const
    {
        return !((*this) == matrix);
    }

    Matrix4x4 mul(const Matrix4x4& matrix1, const Matrix4x4& matrix2)
    {
        Matrix4x4 ret;
        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] =  matrix1._data[i][0] * matrix2._data[0][j] +
                                matrix1._data[i][1] * matrix2._data[1][j] +
                                matrix1._data[i][2] * matrix2._data[2][j] +
                                matrix1._data[i][3] * matrix2._data[3][j];
        return ret;
    }

        
    Vector4F mul(const Matrix4x4& matrix, const Vector4F& crVec)
    {
        Vector4F ret;
        for (uint32_t i = 0; i < 4; ++i)
                ret[i] =  matrix._data[i][0] * crVec[0] +
                          matrix._data[i][1] * crVec[1] +
                          matrix._data[i][2] * crVec[2] +
                          matrix._data[i][3] * crVec[3];
        return ret;
    }

    Vector4F mul(const Vector4F& crVec, const Matrix4x4& matrix)
    {
        Vector4F ret;
        for (uint32_t i = 0; i < 4; ++i)
                ret[i] =  crVec[0] * matrix._data[0][i] +
                          crVec[1] * matrix._data[1][i] +
                          crVec[2] * matrix._data[2][i] + 
                          crVec[3] * matrix._data[3][i];
        return ret;
    }

    bool invertible(const Matrix4x4& matrix, Matrix4x4& out_inv_matrix)
    {
        uint32_t index_column[4], index_raw[4];
		uint32_t ipiv[4] = { 0, 0, 0, 0 };
		float inv_matrix[4][4];
		memcpy(inv_matrix, matrix._data, 16 * sizeof(float));

		for (uint32_t i = 0; i < 4; ++i)
		{
			uint32_t column_index = 0, raw_index = 0;
			float big = 0.0f;
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
							return false;
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

			if (inv_matrix[column_index][column_index] == 0.0f) return false;
            
			const float inv_piv = 1.0f / inv_matrix[column_index][column_index];
			inv_matrix[column_index][column_index] = 1.0f;

			for (uint32_t j = 0; j < 4; j++)
			{
				inv_matrix[column_index][j] *= inv_piv;
			}

			for (uint32_t j = 0; j < 4; j++)
			{
				if (j != column_index)
				{
					const float fSave = inv_matrix[j][column_index];
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

        out_inv_matrix = Matrix4x4(inv_matrix);
        return true;
    }

    Matrix4x4 inverse(const Matrix4x4& matrix)
    {
		uint32_t index_column[4], index_raw[4];
		uint32_t ipiv[4] = { 0, 0, 0, 0 };
		float inv_matrix[4][4];
		memcpy(inv_matrix, matrix._data, 16 * sizeof(float));

		for (uint32_t i = 0; i < 4; ++i)
		{
			uint32_t column_index = 0, raw_index = 0;
			float big = 0.0f;
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

			const float inv_piv = 1.0f / inv_matrix[column_index][column_index];
			inv_matrix[column_index][column_index] = 1.0f;

			for (uint32_t j = 0; j < 4; j++)
			{
				inv_matrix[column_index][j] *= inv_piv;
			}

			for (uint32_t j = 0; j < 4; j++)
			{
				if (j != column_index)
				{
					const float fSave = inv_matrix[j][column_index];
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

        return Matrix4x4{ inv_matrix };
    }


    
    Matrix3x4::Matrix3x4()
    {
        // 单位矩阵
        _data[0][0] = _data[1][1] = _data[2][2] = 1.0f;
        
        _data[0][1] = _data[0][2] = _data[0][3] = 
        _data[1][0] = _data[1][2] = _data[1][3] =
        _data[2][0] = _data[2][1] = _data[2][3] = 0.0f;
    };

    Matrix3x4::Matrix3x4(const float f[3][4])
    {
        memcpy(_data, f, sizeof(float) * 12);
    }

    Matrix3x4::Matrix3x4(
        float f00, float f01, float f02, float f03,
        float f10, float f11, float f12, float f13,
        float f20, float f21, float f22, float f23
    )
    {
        _data[0][0] = f00; _data[0][1] = f01; _data[0][2] = f02; _data[0][3] = f03;
        _data[1][0] = f10; _data[1][1] = f11; _data[1][2] = f12; _data[1][3] = f13;
        _data[2][0] = f20; _data[2][1] = f21; _data[2][2] = f22; _data[2][3] = f23;
    }

    Matrix3x4::Matrix3x4(const Matrix3x3& matrix)
    {
        _data[0][0] = matrix[0][0]; _data[0][1] = matrix[0][1]; _data[0][2] = matrix[0][2]; _data[0][3] = 0.0f;
        _data[1][0] = matrix[1][0]; _data[1][1] = matrix[1][1]; _data[1][2] = matrix[1][2]; _data[1][3] = 0.0f;
        _data[2][0] = matrix[2][0]; _data[2][1] = matrix[2][1]; _data[2][2] = matrix[2][2]; _data[2][3] = 0.0f;
    }


    Matrix3x4 operator+(const Matrix3x4& matrix1, const Matrix3x4& matrix2)
    {
        Matrix3x4 ret;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] = matrix1._data[i][j] + matrix2._data[i][j];
        return ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    Matrix3x4 operator*(T value, const Matrix3x4& matrix)
    {
        Matrix3x4 ret = matrix;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] *= static_cast<float>(value);
        return ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    Matrix3x4 operator*(const Matrix3x4& matrix, T value)
    {
        Matrix3x4 ret = matrix;
        for (uint32_t i = 0; i < 3; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                ret._data[i][j] *= value;
        return ret;
    }

    bool Matrix3x4::operator==(const Matrix3x4& matrix) const
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

    bool Matrix3x4::operator!=(const Matrix3x4& matrix) const
    {
        return !((*this) == matrix);
    }


    
    Matrix4x4 translate(const Vector3F& delta)
    {
        return Matrix4x4{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            delta.x, delta.y, delta.z, 1.0f
        };
    }

    Matrix4x4 scale(const Vector3F& scale)
    {
        return Matrix4x4{
            scale.x,  0.0f,       0.0f,       0,
            0.0f,       scale.y,  0.0f,       0,
            0.0f,       0.0f,       scale.z,  0,
            0.0f,       0.0f,       0.0f,       1.0f
        };
    }

    Matrix4x4 rotate_x(float theta)
    {
        const float sin_theta = std::sin(radians(theta));
        const float cos_theta = std::cos(radians(theta));
        return Matrix4x4(
            1, 0,           0,          0,
            0, cos_theta,   sin_theta, 0,
            0, -sin_theta,   cos_theta,  0,
            0, 0,           0,          1
        );
    }

    Matrix4x4 rotate_y(float theta)
    {
        const float sin_theta = std::sin(radians(theta));
        const float cos_theta = std::cos(radians(theta));
        return Matrix4x4(
            cos_theta,  0,          -sin_theta,  0,
            0,          1,          0,          0,
            sin_theta, 0,          cos_theta,  0,
            0,          0,          0,          1
        );
    }

    Matrix4x4 rotate_z(float theta)
    {
        const float sin_theta = std::sin(radians(theta));
        const float cos_theta = std::cos(radians(theta));
        return Matrix4x4(
            cos_theta,  sin_theta, 0, 0,
            -sin_theta,  cos_theta,  0, 0,
            0,          0,          1, 0,
            0,          0,          0, 1
        );
    }


    Matrix4x4 rotate(float theta, const Vector3F& crAxis)
    {
        Vector3F v = normalize(crAxis);
        float sin_theta = std::sin(radians(theta));
        float cos_theta = std::cos(radians(theta));

        Matrix4x4 ret;

        ret[0][0] = v.x * v.x + (1 - v.x * v.x) * cos_theta;
        ret[1][0] = v.x * v.y * (1 - cos_theta) - v.z * sin_theta;
        ret[2][0] = v.x * v.z * (1 - cos_theta) + v.y * sin_theta;
        ret[3][0] = 0;

        ret[0][1] = v.x * v.y * (1 - cos_theta) + v.z * sin_theta;
        ret[1][1] = v.y * v.y + (1 - v.y * v.y) * cos_theta;
        ret[2][1] = v.y * v.z * (1 - cos_theta) - v.x * sin_theta;
        ret[3][1] = 0;

        ret[0][2] = v.x * v.z * (1 - cos_theta) - v.y * sin_theta;
        ret[1][2] = v.y * v.z * (1 - cos_theta) + v.x * sin_theta;
        ret[2][2] = v.z * v.z + (1 - v.z * v.z) * cos_theta;
        ret[3][2] = 0;

        return ret;
    }

	fantasy::Matrix4x4 rotate(const Vector3F& crRotation)
	{
        return mul(rotate_x(crRotation.x), mul(rotate_y(crRotation.y), rotate_z(crRotation.z)));
	}

	Matrix4x4 orthographic_left_hand(float fWidth, float fHeight, float fNearZ, float fFarZ)
	{
		return Matrix4x4(
            2.0f / fWidth, 0.0f,           0.0f,                       0.0f,
			0.0f,          2.0f / fHeight, 0.0f,                       0.0f,
			0.0f,          0.0f,           1.0f / (fFarZ - fNearZ),    0.0f,
			0.0f,          0.0f,           fNearZ / (fNearZ - fFarZ),  1
        );
	}

    Matrix4x4 perspective_left_hand(float fFovAngleY, float fAspectRatio, float fNearZ, float fFarZ)
    {
        float fInvTanAng = 1.0f / std::tan(radians(fFovAngleY) / 2.0f);
        return Matrix4x4(
            fInvTanAng / fAspectRatio, 0,          0,                         0, 
            0,                         fInvTanAng, 0,                         0, 
            0,                         0,          fFarZ / (fFarZ - fNearZ),  1.0f,
            0,                         0,          -fFarZ * fNearZ / (fFarZ - fNearZ),                      0
        );

        // mul(scale(Vector3F(fInvTanAng / fAspectRatio, fInvTanAng, 1.0f)), normalizeDepthMatrix);
    }

    Matrix4x4 PerspectiveLeftHandinverseDepth(float fFovAngleY, float fAspectRatio, float fNearZ, float fFarZ)
    {
        float fInvTanAng = 1.0f / std::tan(radians(fFovAngleY) / 2.0f);
        return Matrix4x4(
            fInvTanAng / fAspectRatio, 0,          0,                         0, 
            0,                         fInvTanAng, 0,                         0, 
            0,                         0,          -fNearZ / (fFarZ - fNearZ),  1.0f,
            0,                         0,          fFarZ * fNearZ / (fFarZ - fNearZ),                      0
        );

        // mul(scale(Vector3F(fInvTanAng / fAspectRatio, fInvTanAng, 1.0f)), normalizeDepthMatrix);
    }

	fantasy::Matrix3x3 create_orthogonal_basis_from_z(const Vector3F& Z)
	{
		Vector3F z = normalize(Z);
		Vector3F y;
		if (std::abs(std::abs(dot(Z, Vector3F(1.0f, 0.0f, 0.0f)) - 1.0f)) > 0.1f)
		{
			y = cross(z, Vector3F(1.0f, 0.0f, 0.0f));
		}
		else
		{
			y = cross(z, Vector3F(0.0f, 1.0f, 0.0f));
		}
        Matrix3x3 ret(normalize(cross(y, z)), normalize(y), z);
		return ret;
	}

	Matrix4x4 look_at_left_hand(const Vector3F& crPos, const Vector3F& crLook, const Vector3F& crUp)
    {
		auto L = normalize(crLook - crPos);
		auto R = normalize(cross(crUp, L));
		auto U = cross(L, R);

        return inverse(Matrix4x4(
            R.x,     R.y,     R.z,     0.0f,
            U.x,     U.y,     U.z,     0.0f,
            L.x,     L.y,     L.z,     0.0f,
            crPos.x, crPos.y, crPos.z, 1.0f
        ));
    }



}
