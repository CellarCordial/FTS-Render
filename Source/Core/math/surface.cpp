#include "surface.h"
#include "matrix.h"
#include "../tools/log.h"

namespace fantasy 
{
    QuadricSurface::QuadricSurface(Vector3F p0, Vector3F p1, Vector3F p2)
    {
        Vector3F N = cross(p1 - p0, p2 - p0);

        // ax + by + cz + d = 0 为平面方程, 则 d = -(ax + by + cz) = -dot(normal, p0).
        float distance = -dot(N, p0);
        auto [a, b, c] = N;

        a2 = a * a; b2 = b * b; c2 = c * c; d2 = distance * distance;
        ab = a * b; ac = a * c; ad = a * distance;
        bc = b * c; bd = b * distance; 
        cd = c * distance;
    }

    bool QuadricSurface::get_vertex_position(Vector3F& vertex)
    {
        Matrix4x4 m(
            a2,   ab,   ac,   0.0f,
            ab,   b2,   bc,   0.0f,
            ac,   bc,   c2,   0.0f,
            ad,   bd,   cd,   1.0f
        );

        Matrix4x4 inv;
        ReturnIfFalse(invertible(m, inv));
        vertex = { inv[3][0], inv[3][1], inv[3][2] };
        return true;
    }

    float QuadricSurface::distance_to_surface(Vector3F p)
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
        const float* sur0 = reinterpret_cast<const float*>(&surface0);
        const float* sur1 = reinterpret_cast<const float*>(&surface1);

        QuadricSurface ret;
        float* ret_ptr = reinterpret_cast<float*>(&ret);
        for (uint32_t ix = 0; ix < 10; ++ix)
        {
            ret_ptr[ix] = sur0[ix] + sur1[ix];
        }
        return ret;
    }


}