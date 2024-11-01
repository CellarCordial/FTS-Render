#ifndef MATH_RAY_H
#define MATH_RAY_H

#include "Vector.h"

namespace FTS 
{
    struct FRay
	{
		FVector3F	m_Ori;
		FVector3F	m_Dir;
		mutable FLOAT m_fMax;
		
		FRay() : m_fMax(_INFINITY_) {}
		
		FRay(const FVector3F& _o, const FVector3F& _d, FLOAT fMax = _INFINITY_, FLOAT fTime = 0.0f) :
            m_Ori(_o), m_Dir(_d), m_fMax(fMax)
        {
        }

		FVector3F operator()(FLOAT _t) const
        {
            return m_Ori + m_Dir * _t;
        }
	};

}










#endif