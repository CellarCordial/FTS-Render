#include "../include/Matrix.h"

#include <cassert>

namespace FTS 
{

    FMatrix3x3::FMatrix3x3()
    {
        // 单位矩阵
        m_pf[0][0] = m_pf[1][1] = m_pf[2][2] = 1.0f;
        m_pf[0][1] = m_pf[0][2] = m_pf[1][0] = m_pf[1][2] = m_pf[2][0] = m_pf[2][1] = 0.0f;
    };

    FMatrix3x3::FMatrix3x3(const FLOAT cpf[3][3])
    {
        memcpy(m_pf, cpf, sizeof(FLOAT) * 9);
    }

    FMatrix3x3::FMatrix3x3(
        FLOAT f00, FLOAT f01, FLOAT f02,
        FLOAT f10, FLOAT f11, FLOAT f12,
        FLOAT f20, FLOAT f21, FLOAT f22
    )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
    {
        m_pf[0][0] = f00; m_pf[0][1] = f01; m_pf[0][2] = f02;
        m_pf[1][0] = f10; m_pf[1][1] = f11; m_pf[1][2] = f12;
        m_pf[2][0] = f20; m_pf[2][1] = f21; m_pf[2][2] = f22;
    }

    FMatrix3x3::FMatrix3x3(const FMatrix4x4& crMatrix)
    {
        m_pf[0][0] = crMatrix[0][0]; m_pf[0][1] = crMatrix[0][1]; m_pf[0][2] = crMatrix[0][2];
        m_pf[1][0] = crMatrix[1][0]; m_pf[1][1] = crMatrix[1][1]; m_pf[1][2] = crMatrix[1][2];
        m_pf[2][0] = crMatrix[2][0]; m_pf[2][1] = crMatrix[2][1]; m_pf[2][2] = crMatrix[2][2];
    }


	FMatrix3x3::FMatrix3x3(const FVector3F& x, const FVector3F& y, const FVector3F& z)
	{
		m_pf[0][0] = x[0]; m_pf[0][1] = x[1]; m_pf[0][2] = x[2];
		m_pf[1][0] = y[0]; m_pf[1][1] = y[1]; m_pf[1][2] = y[2];
		m_pf[2][0] = z[0]; m_pf[2][1] = z[1]; m_pf[2][2] = z[2];
	}

	FMatrix3x3 operator+(const FMatrix3x3& crMatrix1, const FMatrix3x3& crMatrix2)
    {
        FMatrix3x3 Ret;
        for (UINT32 i = 0; i < 3; ++i)
            for (UINT32 j = 0; j < 3; ++j)
                Ret.m_pf[i][j] = crMatrix1.m_pf[i][j] + crMatrix2.m_pf[i][j];
        return Ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    FMatrix3x3 operator*(T Value, const FMatrix3x3& crMatrix)
    {
        FMatrix3x3 Ret = crMatrix;
        for (UINT32 i = 0; i < 3; ++i)
            for (UINT32 j = 0; j < 3; ++j)
                Ret.m_pf[i][j] *= static_cast<FLOAT>(Value);
        return Ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    FMatrix3x3 operator*(const FMatrix3x3& crMatrix, T Value)
    {
        FMatrix3x3 Ret = crMatrix;
        for (UINT32 i = 0; i < 3; ++i)
            for (UINT32 j = 0; j < 3; ++j)
                Ret.m_pf[i][j] *= Value;
        return Ret;
    }

    BOOL FMatrix3x3::operator==(const FMatrix3x3& crMatrix) const
    {
        for (UINT32 i = 0; i < 3; ++i)
            for (UINT32 j = 0; j < 3; ++j)
            {
                // m_pf[i][j] != crMatrix.m_pf[i][j]
                const auto Temp = m_pf[i][j] - crMatrix.m_pf[i][j];
                if (NOT_FLOAT_ZERO(Temp))
                    return false;
            }
        return true;
    }

    BOOL FMatrix3x3::operator!=(const FMatrix3x3& crMatrix) const
    {
        return !((*this) == crMatrix);
    }

    FMatrix3x3 Mul(const FMatrix3x3& crMatrix1, const FMatrix3x3& crMatrix2)
    {
        FMatrix3x3 Ret;
        for (UINT32 i = 0; i < 3; ++i)
            for (UINT32 j = 0; j < 3; ++j)
                Ret.m_pf[i][j] =  crMatrix1.m_pf[i][0] * crMatrix2.m_pf[0][j] +
                                crMatrix1.m_pf[i][1] * crMatrix2.m_pf[1][j] +
                                crMatrix1.m_pf[i][2] * crMatrix2.m_pf[2][j];
        return Ret;
    }

    
    FVector3F Mul(const FMatrix3x3& crMatrix, const FVector3F& crVec)
    {
        FVector3F Ret;
        for (UINT32 i = 0; i < 3; ++i)
                Ret[i] =  crMatrix.m_pf[i][0] * crVec[0] +
                          crMatrix.m_pf[i][1] * crVec[1] +
                          crMatrix.m_pf[i][2] * crVec[2];
        return Ret;
    }

    FVector3F Mul(const FVector3F& crVec, const FMatrix3x3& crMatrix)
    {
        FVector3F Ret;
        for (UINT32 i = 0; i < 3; ++i)
                Ret[i] =  crVec[0] * crMatrix.m_pf[0][i] +
                          crVec[1] * crMatrix.m_pf[1][i] +
                          crVec[2] * crMatrix.m_pf[2][i];
        return Ret;
    }

    
    FMatrix4x4::FMatrix4x4()
    {
        // 单位矩阵
        m_pf[0][0] = m_pf[1][1] = m_pf[2][2] = m_pf[3][3] = 1.0f;
        m_pf[0][1] = m_pf[0][2] = m_pf[0][3] = m_pf[1][0] = m_pf[1][2] = m_pf[1][3] =
        m_pf[2][0] = m_pf[2][1] = m_pf[2][3] = m_pf[3][0] = m_pf[3][1] = m_pf[3][2] = 0.0f;
    };

    FMatrix4x4::FMatrix4x4(const FLOAT cpf[4][4])
    {
        memcpy(m_pf, cpf, sizeof(FLOAT) * 16);
    }

    FMatrix4x4::FMatrix4x4(
        FLOAT f00, FLOAT f01, FLOAT f02, FLOAT f03,
        FLOAT f10, FLOAT f11, FLOAT f12, FLOAT f13,
        FLOAT f20, FLOAT f21, FLOAT f22, FLOAT f23,
        FLOAT f30, FLOAT f31, FLOAT f32, FLOAT f33
    )
    {
        m_pf[0][0] = f00; m_pf[0][1] = f01; m_pf[0][2] = f02; m_pf[0][3] = f03;
        m_pf[1][0] = f10; m_pf[1][1] = f11; m_pf[1][2] = f12; m_pf[1][3] = f13;
        m_pf[2][0] = f20; m_pf[2][1] = f21; m_pf[2][2] = f22; m_pf[2][3] = f23;
        m_pf[3][0] = f30; m_pf[3][1] = f31; m_pf[3][2] = f32; m_pf[3][3] = f33;
    }

    FMatrix4x4::FMatrix4x4(const FMatrix3x3& crMatrix)
    {
        m_pf[0][0] = crMatrix[0][0]; m_pf[0][1] = crMatrix[0][1]; m_pf[0][2] = crMatrix[0][2]; m_pf[0][3] = 0.0f;
        m_pf[1][0] = crMatrix[1][0]; m_pf[1][1] = crMatrix[1][1]; m_pf[1][2] = crMatrix[1][2]; m_pf[1][3] = 0.0f;
        m_pf[2][0] = crMatrix[2][0]; m_pf[2][1] = crMatrix[2][1]; m_pf[2][2] = crMatrix[2][2]; m_pf[2][3] = 0.0f;
        m_pf[3][0] = 0.0f;           m_pf[3][1] = 0.0f;           m_pf[3][2] = 0.0f;           m_pf[3][3] = 1.0f;
    }


    FMatrix4x4 operator+(const FMatrix4x4& crMatrix1, const FMatrix4x4& crMatrix2)
    {
        FMatrix4x4 Ret;
        for (UINT32 i = 0; i < 4; ++i)
            for (UINT32 j = 0; j < 4; ++j)
                Ret.m_pf[i][j] = crMatrix1.m_pf[i][j] + crMatrix2.m_pf[i][j];
        return Ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    FMatrix4x4 operator*(T Value, const FMatrix4x4& crMatrix)
    {
        FMatrix4x4 Ret = crMatrix;
        for (UINT32 i = 0; i < 4; ++i)
            for (UINT32 j = 0; j < 4; ++j)
                Ret.m_pf[i][j] *= static_cast<FLOAT>(Value);
        return Ret;
    }

    template <typename T>
    requires std::is_arithmetic_v<T>
    FMatrix4x4 operator*(const FMatrix4x4& crMatrix, T Value)
    {
        FMatrix4x4 Ret = crMatrix;
        for (UINT32 i = 0; i < 4; ++i)
            for (UINT32 j = 0; j < 4; ++j)
                Ret.m_pf[i][j] *= Value;
        return Ret;
    }

    BOOL FMatrix4x4::operator==(const FMatrix4x4& crMatrix) const
    {
        for (UINT32 i = 0; i < 4; ++i)
            for (UINT32 j = 0; j < 4; ++j)
            {
                // m_pf[i][j] != crMatrix.m_pf[i][j]
                const auto Temp = m_pf[i][j] - crMatrix.m_pf[i][j];
                if (NOT_FLOAT_ZERO(Temp))
                    return false;
            }
        return true;
    }

    BOOL FMatrix4x4::operator!=(const FMatrix4x4& crMatrix) const
    {
        return !((*this) == crMatrix);
    }

    FMatrix4x4 Mul(const FMatrix4x4& crMatrix1, const FMatrix4x4& crMatrix2)
    {
        FMatrix4x4 Ret;
        for (UINT32 i = 0; i < 4; ++i)
            for (UINT32 j = 0; j < 4; ++j)
                Ret.m_pf[i][j] =  crMatrix1.m_pf[i][0] * crMatrix2.m_pf[0][j] +
                                crMatrix1.m_pf[i][1] * crMatrix2.m_pf[1][j] +
                                crMatrix1.m_pf[i][2] * crMatrix2.m_pf[2][j] +
                                crMatrix1.m_pf[i][3] * crMatrix2.m_pf[3][j];
        return Ret;
    }

        
    FVector4F Mul(const FMatrix4x4& crMatrix, const FVector4F& crVec)
    {
        FVector4F Ret;
        for (UINT32 i = 0; i < 4; ++i)
                Ret[i] =  crMatrix.m_pf[i][0] * crVec[0] +
                          crMatrix.m_pf[i][1] * crVec[1] +
                          crMatrix.m_pf[i][2] * crVec[2] +
                          crMatrix.m_pf[i][3] * crVec[3];
        return Ret;
    }

    FVector4F Mul(const FVector4F& crVec, const FMatrix4x4& crMatrix)
    {
        FVector4F Ret;
        for (UINT32 i = 0; i < 4; ++i)
                Ret[i] =  crVec[0] * crMatrix.m_pf[0][i] +
                          crVec[1] * crMatrix.m_pf[1][i] +
                          crVec[2] * crMatrix.m_pf[2][i] + 
                          crVec[3] * crMatrix.m_pf[3][i];
        return Ret;
    }

    FMatrix4x4 Inverse(const FMatrix4x4& crMatrix)
    {
		UINT32 dwIndexCol[4], dwIndexRaw[4];
		UINT32 dwIPIV[4] = { 0, 0, 0, 0 };
		FLOAT fInvMatrix[4][4];
		memcpy(fInvMatrix, crMatrix.m_pf, 16 * sizeof(FLOAT));

		for (UINT32 i = 0; i < 4; ++i)
		{
			UINT32 dwColIndex = 0, dwRawIndex = 0;
			FLOAT fBig = 0.0f;
			for (UINT32 j = 0; j < 4; ++j)
			{
				if (dwIPIV[j] != 1)
				{
					for (UINT32 k = 0; k < 4; ++k)
					{
						if (dwIPIV[k] == 0)
						{
							if (std::abs(fInvMatrix[j][k]) >= fBig)
							{
								fBig = std::abs(fInvMatrix[j][k]);
								dwRawIndex = j;
								dwColIndex = k;
							}
						}
						else if (dwIPIV[k] > 1)
						{
							assert(!"It is a Singular matrix which can't be used in MatrixInvert");
						}
					}
				}
			}
			dwIPIV[dwColIndex]++;

			if (dwColIndex != dwRawIndex)
			{
				for (UINT32 k = 0; k < 4; ++k)
				{
					std::swap(fInvMatrix[dwRawIndex][k], fInvMatrix[dwColIndex][k]);
				}
			}
			dwIndexCol[i] = dwColIndex;
			dwIndexRaw[i] = dwRawIndex;

			if (fInvMatrix[dwColIndex][dwColIndex] == 0.0f)
			{
				assert(!"It is a Singular matrix which can't be used in MatrixInvert");
			}

			const FLOAT fInvPIV = 1.0f / fInvMatrix[dwColIndex][dwColIndex];
			fInvMatrix[dwColIndex][dwColIndex] = 1.0f;

			for (UINT32 j = 0; j < 4; j++)
			{
				fInvMatrix[dwColIndex][j] *= fInvPIV;
			}

			for (UINT32 j = 0; j < 4; j++)
			{
				if (j != dwColIndex)
				{
					const FLOAT fSave = fInvMatrix[j][dwColIndex];
					fInvMatrix[j][dwColIndex] = 0;
					for (int k = 0; k < 4; k++)
					{
						fInvMatrix[j][k] -= fInvMatrix[dwColIndex][k] * fSave;
					}
				}
			}
		}

		for (int j = 3; j >= 0; j--)
		{
			if (dwIndexRaw[j] != dwIndexCol[j])
			{
				for (int k = 0; k < 4; k++)
					std::swap(fInvMatrix[k][dwIndexRaw[j]], fInvMatrix[k][dwIndexCol[j]]);
			}
		}

        return FMatrix4x4{ fInvMatrix };
    }

    
    FMatrix4x4 Translate(const FVector3F& crDelta)
    {
        return FMatrix4x4{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            crDelta.x, crDelta.y, crDelta.z, 1.0f
        };
    }

    FMatrix4x4 Scale(const FVector3F& crScale)
    {
        return FMatrix4x4{
            crScale.x,  0.0f,       0.0f,       0,
            0.0f,       crScale.y,  0.0f,       0,
            0.0f,       0.0f,       crScale.z,  0,
            0.0f,       0.0f,       0.0f,       1.0f
        };
    }

    FMatrix4x4 RotateX(FLOAT fTheta)
    {
        const FLOAT fSinTheta = std::sin(Radians(fTheta));
        const FLOAT fCosTheta = std::cos(Radians(fTheta));
        return FMatrix4x4(
            1, 0,           0,          0,
            0, fCosTheta,   fSinTheta, 0,
            0, -fSinTheta,   fCosTheta,  0,
            0, 0,           0,          1
        );
    }

    FMatrix4x4 RotateY(FLOAT fTheta)
    {
        const FLOAT fSinTheta = std::sin(Radians(fTheta));
        const FLOAT fCosTheta = std::cos(Radians(fTheta));
        return FMatrix4x4(
            fCosTheta,  0,          -fSinTheta,  0,
            0,          1,          0,          0,
            fSinTheta, 0,          fCosTheta,  0,
            0,          0,          0,          1
        );
    }

    FMatrix4x4 RotateZ(FLOAT fTheta)
    {
        const FLOAT fSinTheta = std::sin(Radians(fTheta));
        const FLOAT fCosTheta = std::cos(Radians(fTheta));
        return FMatrix4x4(
            fCosTheta,  fSinTheta, 0, 0,
            -fSinTheta,  fCosTheta,  0, 0,
            0,          0,          1, 0,
            0,          0,          0, 1
        );
    }


    FMatrix4x4 Rotate(FLOAT fTheta, const FVector3F& crAxis)
    {
        FVector3F v = Normalize(crAxis);
        FLOAT fSinTheta = std::sin(Radians(fTheta));
        FLOAT fCosTheta = std::cos(Radians(fTheta));

        FMatrix4x4 Ret;

        Ret[0][0] = v.x * v.x + (1 - v.x * v.x) * fCosTheta;
        Ret[1][0] = v.x * v.y * (1 - fCosTheta) - v.z * fSinTheta;
        Ret[2][0] = v.x * v.z * (1 - fCosTheta) + v.y * fSinTheta;
        Ret[3][0] = 0;

        Ret[0][1] = v.x * v.y * (1 - fCosTheta) + v.z * fSinTheta;
        Ret[1][1] = v.y * v.y + (1 - v.y * v.y) * fCosTheta;
        Ret[2][1] = v.y * v.z * (1 - fCosTheta) - v.x * fSinTheta;
        Ret[3][1] = 0;

        Ret[0][2] = v.x * v.z * (1 - fCosTheta) - v.y * fSinTheta;
        Ret[1][2] = v.y * v.z * (1 - fCosTheta) + v.x * fSinTheta;
        Ret[2][2] = v.z * v.z + (1 - v.z * v.z) * fCosTheta;
        Ret[3][2] = 0;

        return Ret;
    }

    FMatrix4x4 OrthographicLeftHand(FLOAT fWidth, FLOAT fHeight, FLOAT fNearZ, FLOAT fFarZ)
	{
		return FMatrix4x4(
            2.0f / fWidth, 0.0f,           0.0f,                       0.0f,
			0.0f,          2.0f / fHeight, 0.0f,                       0.0f,
			0.0f,          0.0f,           1.0f / (fFarZ - fNearZ),    0.0f,
			0.0f,          0.0f,           fNearZ / (fNearZ - fFarZ),  1
        );
	}

    FMatrix4x4 PerspectiveLeftHand(FLOAT fFovAngleY, FLOAT fAspectRatio, FLOAT fNearZ, FLOAT fFarZ)
    {
        FLOAT fInvTanAng = 1.0f / std::tan(Radians(fFovAngleY) / 2.0f);
        return FMatrix4x4(
            fInvTanAng / fAspectRatio, 0,          0,                         0, 
            0,                         fInvTanAng, 0,                         0, 
            0,                         0,          fFarZ / (fFarZ - fNearZ),  1.0f,
            0,                         0,          -fFarZ * fNearZ / (fFarZ - fNearZ),                      0
        );

        // Mul(Scale(FVector3F(fInvTanAng / fAspectRatio, fInvTanAng, 1.0f)), NormalizeDepthMatrix);
    }

    FMatrix4x4 PerspectiveLeftHandInverseDepth(FLOAT fFovAngleY, FLOAT fAspectRatio, FLOAT fNearZ, FLOAT fFarZ)
    {
        FLOAT fInvTanAng = 1.0f / std::tan(Radians(fFovAngleY) / 2.0f);
        return FMatrix4x4(
            fInvTanAng / fAspectRatio, 0,          0,                         0, 
            0,                         fInvTanAng, 0,                         0, 
            0,                         0,          -fNearZ / (fFarZ - fNearZ),  1.0f,
            0,                         0,          fFarZ * fNearZ / (fFarZ - fNearZ),                      0
        );

        // Mul(Scale(FVector3F(fInvTanAng / fAspectRatio, fInvTanAng, 1.0f)), NormalizeDepthMatrix);
    }

	FTS::FMatrix3x3 CreateOrthogonalBasisFromZ(const FVector3F& Z)
	{
		FVector3F z = Normalize(Z);
		FVector3F y;
		if (std::abs(std::abs(Dot(Z, FVector3F(1.0f, 0.0f, 0.0f)) - 1.0f)) > 0.1f)
		{
			y = Cross(z, FVector3F(1.0f, 0.0f, 0.0f));
		}
		else
		{
			y = Cross(z, FVector3F(0.0f, 1.0f, 0.0f));
		}
        FMatrix3x3 Ret(Normalize(Cross(y, z)), Normalize(y), z);
		return Ret;
	}

	FMatrix4x4 LookAtLeftHand(const FVector3F& crPos, const FVector3F& crLook, const FVector3F& crUp)
    {
		auto L = Normalize(crLook - crPos);
		auto R = Normalize(Cross(crUp, L));
		auto U = Cross(L, R);
		return Transpose(Inverse(FMatrix4x4(
            R.x, U.x, L.x, crPos.x,
			R.y, U.y, L.y, crPos.y,
			R.z, U.z, L.z, crPos.z,
			0, 0, 0, 1
        )));
    }



}
