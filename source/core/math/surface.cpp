#include "surface.h"
#include "matrix.h"
#include "vector.h"

namespace fantasy 
{
    QuadricSurface::QuadricSurface(const double3& p0, const double3& p1, const double3& p2)
    {
        double3 normal = cross(p1 - p0, p2 - p0);
        double3 N = normal * (1 / sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z));

        // ax + by + cz + d = 0 为平面方程, 则 d = -(ax + by + cz) = -dot(normal, p0).
        double distance = -dot(N, p0);
        auto [a, b, c] = N;

        a2 = a * a; b2 = b * b; c2 = c * c; d2 = distance * distance;
        ab = a * b; ac = a * c; ad = a * distance;
        bc = b * c; bd = b * distance; 
        cd = c * distance;
    }

    float3 QuadricSurface::calculate_normal(float3 p) 
    {
        float x = p.x, y = p.y, z = p.z;      

        float3 ret = float3(
            2.0 * x * a2 + 2.0 * y * ab + 2.0 * z * ac + 2.0 * ad,
            2.0 * x * ab + 2.0 * y * b2 + 2.0 * z * bc + 2.0 * bd,
            2.0 * x * ac + 2.0 * y * bc + 2.0 * z * c2 + 2.0 * cd
        );
        
        return normalize(ret);
    }

    float3 QuadricSurface::calculate_tangent(float3 p) 
    {
        float3 tangent;
        tangent.x = 2 * a2 * p.x + ab * p.y + ac * p.z + ad;
        tangent.y = ab * p.x + 2 * b2 * p.y + bc * p.z + bd;
        tangent.z = ac * p.x + bc * p.y + 2 * c2 * p.z + cd;
        return normalize(tangent);
    }

    bool QuadricSurface::get_vertex(float3& position)
    {
        double4x4 inv;
        double4x4 m ={
            a2,ab,ac,ad,
            ab,b2,bc,bd,
            ac,bc,c2,cd,
            0,0,0,1
        };
        if(!inverse_row_major(reinterpret_cast<double*>(&m),reinterpret_cast<double*>(&inv))) return false;
        position = { 
            static_cast<float>(inv[0][3]),
            static_cast<float>(inv[1][3]),
            static_cast<float>(inv[2][3])
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
        QuadricSurface ret;
        double* t0 = reinterpret_cast<double*>(&ret);
        const double* t1 = reinterpret_cast<const double*>(&surface0);
        const double* t2 = reinterpret_cast<const double*>(&surface1);

        for(uint32_t ix = 0; ix < 10; ix++) t0[ix] = t1[ix] + t2[ix];
        
        return ret;
    }


}