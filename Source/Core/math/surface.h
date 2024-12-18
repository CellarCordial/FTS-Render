#ifndef MATH_SURFACE_H
#define MATH_SURFACE_H

#include "vector.h"

namespace fantasy 
{
    struct QuadricSurface
	{
        float a2 = 0.0f, b2 = 0.0f, c2 = 0.0f, d2 = 0.0f;
        float ab = 0.0f, ac = 0.0f, ad = 0.0f;
        float bc = 0.0f, bd = 0.0f; 
        float cd = 0.0f;

        QuadricSurface() = default;
        QuadricSurface(Vector3F p0, Vector3F p1, Vector3F p2);

        bool get_vertex_position(Vector3F& vertex);
        float distance_to_surface(Vector3F p);

	};

    QuadricSurface merge(const QuadricSurface& surface0, const QuadricSurface& surface1);
}















#endif