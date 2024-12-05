#ifndef MATH_SURFACE_H
#define MATH_SURFACE_H

#include "Vector.h"

namespace FTS 
{
    struct FQuadricSurface
	{
        FLOAT a2 = 0.0f, b2 = 0.0f, c2 = 0.0f, d2 = 0.0f;
        FLOAT ab = 0.0f, ac = 0.0f, ad = 0.0f;
        FLOAT bc = 0.0f, bd = 0.0f; 
        FLOAT cd = 0.0f;

        FQuadricSurface() = default;
        FQuadricSurface(FVector3F p0, FVector3F p1, FVector3F p2);

        FVector3F GetVertexPos();
        FLOAT DistanceToSurface(FVector3F p);

	};

    FQuadricSurface Union(const FQuadricSurface& Surface0, const FQuadricSurface& Surface1);
}















#endif