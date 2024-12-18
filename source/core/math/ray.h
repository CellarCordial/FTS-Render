#ifndef MATH_RAY_H
#define MATH_RAY_H

#include "Vector.h"

namespace fantasy 
{
    struct Ray
	{
		Vector3F	ori;
		Vector3F	dir;
		mutable float max;
		
		Ray() : max(INFINITY) {}
		
		Ray(const Vector3F& _o, const Vector3F& _d, float max = INFINITY, float time = 0.0f) :
            ori(_o), dir(_d), max(max)
        {
        }

		Vector3F operator()(float _t) const
        {
            return ori + dir * _t;
        }
	};

}










#endif