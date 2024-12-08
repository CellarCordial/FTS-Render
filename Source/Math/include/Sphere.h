#ifndef MATH_SHPERE_H
#define MATH_SHPERE_H

#include "Vector.h"

namespace FTS 
{
    template <typename T>
	requires std::is_arithmetic_v<T>
    struct TSphere2
    {
        TVector2<T> Center;
        T Radius;

    };

	using FSphere2F = TSphere2<FLOAT>;
	using FSphere2I = TSphere2<INT32>;


    template <typename T>
	requires std::is_arithmetic_v<T>
    struct TSphere3
    {
        TVector3<T> Center;
        T Radius;



    };

	using FSphere3F = TSphere3<FLOAT>;
	using FSphere3I = TSphere3<INT32>;
}

#endif