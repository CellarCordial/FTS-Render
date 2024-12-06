#include "../include/Surface.h"
#include "../include/Matrix.h"
#include "../../Core/include/ComRoot.h"

namespace FTS 
{
    FQuadricSurface::FQuadricSurface(FVector3F p0, FVector3F p1, FVector3F p2)
    {
        FVector3F N = Cross(p1 - p0, p2 - p0);

        // ax + by + cz + d = 0 为平面方程, 则 d = -(ax + by + cz) = -Dot(Normal, p0).
        FLOAT fDistance = -Dot(N, p0);
        auto [a, b, c] = N;

        a2 = a * a; b2 = b * b; c2 = c * c; d2 = fDistance * fDistance;
        ab = a * b; ac = a * c; ad = a * fDistance;
        bc = b * c; bd = b * fDistance; 
        cd = c * fDistance;
    }

    BOOL FQuadricSurface::GetVertexPos(FVector3F& rVertex)
    {
        FMatrix4x4 m(
            a2,   ab,   ac,   0.0f,
            ab,   b2,   bc,   0.0f,
            ac,   bc,   c2,   0.0f,
            ad,   bd,   cd,   1.0f
        );

        FMatrix4x4 Inv;
        ReturnIfFalse(Inverse(m, Inv));
        rVertex = { Inv[3][0], Inv[3][1], Inv[3][2] };
        return true;
    }

    FLOAT FQuadricSurface::DistanceToSurface(FVector3F p)
    {
        FLOAT Res = 
            a2 * p.x * p.x + 2 * ab * p.x * p.y + 2 * ac * p.x * p.z + 2 * ad * p.x +
            b2 * p.y * p.y + 2 * bc * p.y * p.z + 2 * bd * p.y +
            c2 * p.z * p.z + 2 * cd * p.z + 
            d2;
        return Res <= 0.0f ? 0.0f : Res;
    }

    FQuadricSurface Union(const FQuadricSurface& Surface0, const FQuadricSurface& Surface1)
    {
        const FLOAT* cpSurface0 = reinterpret_cast<const FLOAT*>(&Surface0);
        const FLOAT* cpSurface1 = reinterpret_cast<const FLOAT*>(&Surface1);

        FQuadricSurface Ret;
        FLOAT* pRet = reinterpret_cast<FLOAT*>(&Ret);
        for (UINT32 ix = 0; ix < 10; ++ix)
        {
            pRet[ix] = cpSurface0[ix] + cpSurface1[ix];
        }
        return Ret;
    }


}