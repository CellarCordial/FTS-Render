#include "surface.h"
#include "matrix.h"
#include "vector.h"

namespace fantasy 
{
    QuadricSurface::QuadricSurface(const Vector3<double>& p0, const Vector3<double>& p1, const Vector3<double>& p2)
    {
        Vector3<double> normal = cross(p1 - p0, p2 - p0);
        Vector3<double> N = normal * (1 / sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z));

        // ax + by + cz + d = 0 为平面方程, 则 d = -(ax + by + cz) = -dot(normal, p0).
        double distance = -dot(N, p0);
        auto [a, b, c] = N;

        a2 = a * a; b2 = b * b; c2 = c * c; d2 = distance * distance;
        ab = a * b; ac = a * c; ad = a * distance;
        bc = b * c; bd = b * distance; 
        cd = c * distance;
    }

    bool QuadricSurface::get_vertex_position(float3& vertex)
    {
        double4x4 m(
            a2,   ab,   ac,   0.0f,
            ab,   b2,   bc,   0.0f,
            ac,   bc,   c2,   0.0f,
            ad,   bd,   cd,   1.0f
        );

        double4x4 inv;
        if (!invertible(m, inv)) return false;
        vertex = { 
            static_cast<float>(inv[3][0]), 
            static_cast<float>(inv[3][1]), 
            static_cast<float>(inv[3][2]) 
        };
        return true;
    }

    float QuadricSurface::distance_to_surface(const float3& p)
    {
        float ret = 
            a2 * p.x * p.x + 2 * ab * p.x * p.y + 2 * ac * p.x * p.z + 2 * ad * p.x +
            b2 * p.y * p.y + 2 * bc * p.y * p.z + 2 * bd * p.y +
            c2 * p.z * p.z + 2 * cd * p.z + 
            d2;
        return ret <= 0.0f ? 0.0f : ret;
    }

    QuadricSurface merge(const QuadricSurface& surface0, const QuadricSurface& surface1)
    {
        static_assert(sizeof(QuadricSurface) == sizeof(double) * 10);
        
        const double* sur0 = reinterpret_cast<const double*>(&surface0);
        const double* sur1 = reinterpret_cast<const double*>(&surface1);
        
        QuadricSurface ret;
        double* ret_ptr = reinterpret_cast<double*>(&ret);
        for (uint32_t ix = 0; ix < 10; ++ix)
        {
            ret_ptr[ix] = sur0[ix] + sur1[ix];
        }
        return ret;
    }


}