#ifndef MATH_QUATERNION_H
#define MATH_QUATERNION_H

#include "matrix.h"

namespace fantasy 
{
    struct Quaternion
    {
        Quaternion() : m_w(1.0f), m_v(0.0f) {}

        Quaternion(float v0, float v1, float v2, float w) : m_v{ v0, v1, v2 }, m_w(w) {}

        Quaternion(const float4x4& crTrans);
        
        friend Quaternion operator+(const Quaternion& quat1, const Quaternion& quat2)
        {
            Quaternion ret = quat1;
            return ret += quat2;
        }

        friend Quaternion operator-(const Quaternion& quat1, const Quaternion& quat2)
        {
            Quaternion ret = quat1;
            return ret -= quat2;
        }

        Quaternion operator+=(const Quaternion& quat);
        Quaternion operator-=(const Quaternion& quat);
        Quaternion operator-() const;

        Quaternion operator*(float f) const;
        Quaternion operator/(float f) const;

        Quaternion operator*=(float f);
        Quaternion operator/=(float f);

        float4x4 to_matrix() const;

        float m_w;
        float3 m_v;
    };

    // 四元数点乘
    inline float dot(const Quaternion& quat1, const Quaternion& quat2)
    {
        return dot(quat1.m_v, quat2.m_v) + quat1.m_w * quat2.m_w;
    }

    // 四元数规范化
    inline Quaternion normalize(const Quaternion& quat)
    {
        return quat / std::sqrt(dot(quat, quat));
    }
    
    inline Quaternion operator*(float f, const Quaternion& quat)
    {
        return quat * f;
    }

    // 对四元数进行插值
    Quaternion slerp(float ft, const Quaternion& quat1, const Quaternion& quat2);



}


#endif