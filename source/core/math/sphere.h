#ifndef MATH_SHPERE_H
#define MATH_SHPERE_H

#include "vector.h"

namespace fantasy 
{
    template <typename T>
	requires std::is_arithmetic_v<T>
    struct Sphere2
    {
        Vector2<T> Center;
        T Radius;

    };

	using Sphere2F = Sphere2<float>;
	using Sphere2I = Sphere2<int32_t>;


    template <typename T>
	requires std::is_arithmetic_v<T>
    struct Sphere3
    {
        Vector3<T> Center;
        T Radius;



    };

	using Sphere3F = Sphere3<float>;
	using Sphere3I = Sphere3<int32_t>;
}

#endif