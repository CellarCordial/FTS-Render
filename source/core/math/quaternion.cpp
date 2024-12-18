#include "quaternion.h"

namespace fantasy
{
    
    Quaternion::Quaternion(const Matrix4x4& m)
    {
        const float trace = m._data[0][0] + m._data[1][1] + m._data[2][2];

        if (trace > 0.0f)
        {
            float s = std::sqrt(trace + 1.0f);
            m_w = s / 2.0f;
            s = 0.5f / s;
            m_v.x = (m._data[2][1] - m._data[1][2]) * s;
            m_v.y = (m._data[0][2] - m._data[2][0]) * s;
            m_v.z = (m._data[1][0] - m._data[0][1]) * s;
        }
        else
        {
            constexpr int32_t nxt[3] = {1, 2, 0};
            float q[3];
            int i = 0;
            if (m._data[1][1] > m._data[0][0]) i = 1;
            if (m._data[2][2] > m._data[i][i]) i = 2;

            const int j = nxt[i];
            const int k = nxt[j];
                
            float s = std::sqrt((m._data[i][i] - (m._data[j][j] + m._data[k][k])) + 1.0f);
            q[i] = s * 0.5f;
            if (s != 0.f) s = 0.5f / s;

            m_w = (m._data[k][j] - m._data[j][k]) * s;

            q[j] = (m._data[j][i] + m._data[i][j]) * s;
            q[k] = (m._data[k][i] + m._data[i][k]) * s;

            m_v.x = q[0];
            m_v.y = q[1];
            m_v.z = q[2];
        }
    }

    Quaternion Quaternion::operator+=(const Quaternion& quat)
    {
        m_w += quat.m_w;
        m_v += quat.m_v;
        return *this;
    }

    Quaternion Quaternion::operator-=(const Quaternion& quat)
    {
        m_w -= quat.m_w;
        m_v -= quat.m_v;
        return *this;
    }

    Quaternion Quaternion::operator-() const
    {
        Quaternion ret;
        ret.m_w = -m_w;
        ret.m_v = -m_v;
        return ret;
    }

    Quaternion Quaternion::operator*(float f) const
    {
        Quaternion ret = *this;
        ret.m_w *= f;
        ret.m_v *= f;
        return ret;
    }

    Quaternion Quaternion::operator/(float f) const
    {
        Quaternion ret = *this;
        ret.m_w /= f;
        ret.m_v /= f;
        return ret;
    }

    Quaternion Quaternion::operator*=(float f)
    {
        m_w *= f;
        m_v *= f;
        return *this;
    }

    Quaternion Quaternion::operator/=(float f)
    {
        m_w /= f;
        m_v /= f;
        return *this;
    }

    Matrix4x4 Quaternion::to_matrix() const
    {
        const float xx = m_v.x * m_v.x, yy = m_v.y * m_v.y, zz = m_v.z * m_v.z;
        const float xy = m_v.x * m_v.y, xz = m_v.x * m_v.z, yz = m_v.y * m_v.z;
        const float wx = m_v.x * m_w, wy = m_v.y * m_w, wz = m_v.z * m_w;

        Matrix4x4 m;
        m._data[0][0] = 1 - 2 * (yy + zz);
        m._data[0][1] = 2 * (xy + wz);
        m._data[0][2] = 2 * (xz - wy);
        m._data[1][0] = 2 * (xy - wz);
        m._data[1][1] = 1 - 2 * (xx + zz);
        m._data[1][2] = 2 * (yz + wx);
        m._data[2][0] = 2 * (xz + wy);
        m._data[2][1] = 2 * (yz - wx);
        m._data[2][2] = 1 - 2 * (xx + yy);

        return m;
    }
    
    Quaternion slerp(float ft, const Quaternion& quat1, const Quaternion& quat2)
    {
        const float cos_theta = dot(quat1, quat2);
        if (cos_theta > 0.9995f)
        {
            return normalize((1 - ft) * quat1 + ft * quat2);
        }

        const float theta = std::acos(Clamp(cos_theta, -1, 1)) * ft;
        const Quaternion quat = normalize(quat2 - quat1 * cos_theta);
        return quat1 * std::cos(theta) + quat * std::sin(theta);
    }
}