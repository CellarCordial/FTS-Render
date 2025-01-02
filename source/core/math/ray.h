#ifndef MATH_RAY_H
#define MATH_RAY_H

#include "Vector.h"

namespace fantasy 
{
    struct Ray
	{
		float3	ori;
		float3	dir;
		mutable float max;
		
		Ray() : max(INFINITY) {}
		
		Ray(const float3& _o, const float3& _d, float max = INFINITY, float time = 0.0f) :
            ori(_o), dir(_d), max(max)
        {
        }

		float3 operator()(float _t) const
        {
            return ori + dir * _t;
        }
	};

}










#endif