#ifndef MATH_QUATERNION_H
#define MATH_QUATERNION_H

#include "Matrix.h"

namespace FTS 
{
    struct FQuaternion
    {
        FQuaternion() : m_w(1.0f), m_v(0.0f) {}

        FQuaternion(FLOAT v0, FLOAT v1, FLOAT v2, FLOAT w) : m_v{ v0, v1, v2 }, m_w(w) {}

        FQuaternion(const FMatrix4x4& crTrans);
        
        friend FQuaternion operator+(const FQuaternion& crQuat1, const FQuaternion& crQuat2)
        {
            FQuaternion Ret = crQuat1;
            return Ret += crQuat2;
        }

        friend FQuaternion operator-(const FQuaternion& crQuat1, const FQuaternion& crQuat2)
        {
            FQuaternion Ret = crQuat1;
            return Ret -= crQuat2;
        }

        FQuaternion operator+=(const FQuaternion& crQuat);
        FQuaternion operator-=(const FQuaternion& crQuat);
        FQuaternion operator-() const;

        FQuaternion operator*(FLOAT f) const;
        FQuaternion operator/(FLOAT f) const;

        FQuaternion operator*=(FLOAT f);
        FQuaternion operator/=(FLOAT f);

        FMatrix4x4 ToMatrix() const;

        FLOAT m_w;
        FVector3F m_v;
    };

    // 四元数点乘
    inline FLOAT Dot(const FQuaternion& crQuat1, const FQuaternion& crQuat2)
    {
        return Dot(crQuat1.m_v, crQuat2.m_v) + crQuat1.m_w * crQuat2.m_w;
    }

    // 四元数规范化
    inline FQuaternion Normalize(const FQuaternion& crQuat)
    {
        return crQuat / std::sqrt(Dot(crQuat, crQuat));
    }
    
    inline FQuaternion operator*(FLOAT f, const FQuaternion& crQuat)
    {
        return crQuat * f;
    }

    // 对四元数进行插值
    FQuaternion Slerp(FLOAT ft, const FQuaternion& crQuat1, const FQuaternion& crQuat2);



}


#endif