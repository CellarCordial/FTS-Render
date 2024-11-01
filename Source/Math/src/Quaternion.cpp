#include "../include/Quaternion.h"

namespace FTS
{
    
    FQuaternion::FQuaternion(const FMatrix4x4& m)
    {
        const FLOAT fTrace = m.m_pf[0][0] + m.m_pf[1][1] + m.m_pf[2][2];

        if (fTrace > 0.0f)
        {
            FLOAT s = std::sqrt(fTrace + 1.0f);
            m_w = s / 2.0f;
            s = 0.5f / s;
            m_v.x = (m.m_pf[2][1] - m.m_pf[1][2]) * s;
            m_v.y = (m.m_pf[0][2] - m.m_pf[2][0]) * s;
            m_v.z = (m.m_pf[1][0] - m.m_pf[0][1]) * s;
        }
        else
        {
            constexpr INT32 nxt[3] = {1, 2, 0};
            FLOAT q[3];
            int i = 0;
            if (m.m_pf[1][1] > m.m_pf[0][0]) i = 1;
            if (m.m_pf[2][2] > m.m_pf[i][i]) i = 2;

            const int j = nxt[i];
            const int k = nxt[j];
                
            FLOAT s = std::sqrt((m.m_pf[i][i] - (m.m_pf[j][j] + m.m_pf[k][k])) + 1.0f);
            q[i] = s * 0.5f;
            if (s != 0.f) s = 0.5f / s;

            m_w = (m.m_pf[k][j] - m.m_pf[j][k]) * s;

            q[j] = (m.m_pf[j][i] + m.m_pf[i][j]) * s;
            q[k] = (m.m_pf[k][i] + m.m_pf[i][k]) * s;

            m_v.x = q[0];
            m_v.y = q[1];
            m_v.z = q[2];
        }
    }

    FQuaternion FQuaternion::operator+=(const FQuaternion& crQuat)
    {
        m_w += crQuat.m_w;
        m_v += crQuat.m_v;
        return *this;
    }

    FQuaternion FQuaternion::operator-=(const FQuaternion& crQuat)
    {
        m_w -= crQuat.m_w;
        m_v -= crQuat.m_v;
        return *this;
    }

    FQuaternion FQuaternion::operator-() const
    {
        FQuaternion Ret;
        Ret.m_w = -m_w;
        Ret.m_v = -m_v;
        return Ret;
    }

    FQuaternion FQuaternion::operator*(FLOAT f) const
    {
        FQuaternion Ret = *this;
        Ret.m_w *= f;
        Ret.m_v *= f;
        return Ret;
    }

    FQuaternion FQuaternion::operator/(FLOAT f) const
    {
        FQuaternion Ret = *this;
        Ret.m_w /= f;
        Ret.m_v /= f;
        return Ret;
    }

    FQuaternion FQuaternion::operator*=(FLOAT f)
    {
        m_w *= f;
        m_v *= f;
        return *this;
    }

    FQuaternion FQuaternion::operator/=(FLOAT f)
    {
        m_w /= f;
        m_v /= f;
        return *this;
    }

    FMatrix4x4 FQuaternion::ToMatrix() const
    {
        const FLOAT xx = m_v.x * m_v.x, yy = m_v.y * m_v.y, zz = m_v.z * m_v.z;
        const FLOAT xy = m_v.x * m_v.y, xz = m_v.x * m_v.z, yz = m_v.y * m_v.z;
        const FLOAT wx = m_v.x * m_w, wy = m_v.y * m_w, wz = m_v.z * m_w;

        FMatrix4x4 m;
        m.m_pf[0][0] = 1 - 2 * (yy + zz);
        m.m_pf[0][1] = 2 * (xy + wz);
        m.m_pf[0][2] = 2 * (xz - wy);
        m.m_pf[1][0] = 2 * (xy - wz);
        m.m_pf[1][1] = 1 - 2 * (xx + zz);
        m.m_pf[1][2] = 2 * (yz + wx);
        m.m_pf[2][0] = 2 * (xz + wy);
        m.m_pf[2][1] = 2 * (yz - wx);
        m.m_pf[2][2] = 1 - 2 * (xx + yy);

        return m;
    }
    
    FQuaternion Slerp(FLOAT ft, const FQuaternion& crQuat1, const FQuaternion& crQuat2)
    {
        const FLOAT fCosTheta = Dot(crQuat1, crQuat2);
        if (fCosTheta > 0.9995f)
        {
            return Normalize((1 - ft) * crQuat1 + ft * crQuat2);
        }

        const FLOAT fTheta = std::acos(Clamp(fCosTheta, -1, 1)) * ft;
        const FQuaternion QuatPerp = Normalize(crQuat2 - crQuat1 * fCosTheta);
        return crQuat1 * std::cos(fTheta) + QuatPerp * std::sin(fTheta);
    }
}