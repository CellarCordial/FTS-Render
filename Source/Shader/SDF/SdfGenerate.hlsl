#include "Intersect.hlsli"

// #define UPPER_BOUND_ESTIMATE_PRECISON 0
// #define BVH_STACK_SIZE 0

struct FPassConstants
{
    float3 SdfLower;    uint dwTriangleNum;
    float3 SdfUpper;    uint dwSignRayNum;
    float3 SdfExtent;   uint dwXBegin;
    uint dwXEnd;        uint3 Pad;
};

struct FNode
{
    float3 Lower;
    float3 Upper;
    uint dwPrimitiveIndex;
    uint dwPrimitiveNum;
};

struct FVertex
{
    float3 Position;
    float3 Normal;
};

ConstantBuffer<FPassConstants> gPassCB : register(b0);

StructuredBuffer<FNode> gNodes : register(t0);
StructuredBuffer<FVertex> gVertices : register(t1);

RWTexture3D<float> gSdf : register(u0);


float CalcSdf(float3 p, float fUpperBound);
float CalcUpperBound(float3 p, uint dwPrecison);
float CalcUdf(float3 p, float fUpperBound, out uint dwIntersectTriangleIndex);
int EstimateUdfSign(float3 p, float fRandom);
bool ContainsTriangle(float3 p, float fRadius);
int TraceTriangleIndex(float3 p, float3 d, float fMaxLength);

#if defined(UPPER_BOUND_ESTIMATE_PRECISON) && defined(BVH_STACK_SIZE)

[numthreads(1, GROUP_THREAD_NUM_Y, GROUP_THREAD_NUM_Z)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    uint dwWidth, dwHeight, dwDepth;
    gSdf.GetDimensions(dwWidth, dwHeight, dwDepth);

    if (ThreadID.y > dwHeight || ThreadID.z > dwDepth) return;

    // X 轴步长
    float fDeltaX = 1.05f * gPassCB.SdfExtent.x / dwWidth;

    float fy = lerp(gPassCB.SdfLower, gPassCB.SdfUpper, (ThreadID.y + 0.5f) / dwHeight);
    float fz = lerp(gPassCB.SdfLower, gPassCB.SdfUpper, (ThreadID.z + 0.5f) / dwDepth);

    float fLastUdf = -100.0f * fDeltaX;
    for (uint x = gPassCB.dwXBegin; x < gPassCB.dwXEnd; ++x)
    {
        float fx = lerp(gPassCB.SdfLower, gPassCB.SdfUpper, (x + 0.5f) / dwWidth);

        // u(q) <= u(p) + ||p - q|| 
        float fUpperBound = fLastUdf + fDeltaX;
        float fNewSdf = CalcSdf(float3(fx, fy, fz), fUpperBound);
        fLastUdf = abs(fNewSdf);

        gSdf[uint3(x, ThreadID.yz)] = fNewSdf;
    }

}


float CalcSdf(float3 p, float fUpperBound)
{
    if (fUpperBound < 0) fUpperBound = CalcUpperBound(p, UPPER_BOUND_ESTIMATE_PRECISON);

    uint dwIntersectTriangleIndex;
    float fUdf = CalcUdf(p, fUpperBound, dwIntersectTriangleIndex);

    int dwUdfSign = 0;
    
    [unroll]
    for (uint ix = 0; ix < gPassCB.dwSignRayNum; ++ix)
    {
        dwUdfSign += EstimateUdfSign(p, lerp(0.0f, 1.0f, (ix + 0.5f) / gPassCB.dwSignRayNum));
    }

    if (dwUdfSign > 0) return fUdf;
    if (dwUdfSign < 0) return -fUdf;

    FVertex v0 = gVertices[dwIntersectTriangleIndex * 3 + 0];
    FVertex v1 = gVertices[dwIntersectTriangleIndex * 3 + 1];
    FVertex v2 = gVertices[dwIntersectTriangleIndex * 3 + 2];
    
    int s0 = dot(p - v0.Position, v0.Normal) > 0 ? 1 : -1;
    int s1 = dot(p - v1.Position, v1.Normal) > 0 ? 1 : -1;
    int s2 = dot(p - v2.Position, v2.Normal) > 0 ? 1 : -1;

    return s0 + s1 + s2 > 0 ? fUdf : -fUdf;
}


float CalcUpperBound(float3 p, uint dwPrecison)
{
    FNode Node = gNodes[0];
    float fLeft = 0.0f;
    float fRight = distance(0.5f * (Node.Lower + Node.Upper), p) + distance(Node.Lower, Node.Upper);
    
    for (uint ix = 0; ix < dwPrecison; ++ix)
    {
        float fMid = 0.5f * (fLeft + fRight);
        if (ContainsTriangle(p, fMid)) fRight = fMid;
        else fLeft = fMid;
    }
    return fRight;
}

float CalcUdf(float3 p, float fUpperBound, out uint dwIntersectTriangleIndex)
{
    uint fStack[BVH_STACK_SIZE];
    fStack[0] = 0;
    uint dwTop = 1;
    
    while (dwTop > 0)
    {
        FNode Node = gNodes[fStack[--dwTop]];
        if (!IntersectBoxSphere(Node.Lower, Node.Upper, p, fUpperBound)) continue;

        if (Node.dwPrimitiveNum != 0)
        {
            for (uint ix = 0, jx = 3 * Node.dwPrimitiveIndex; ix < Node.dwPrimitiveNum; ++ix, jx += 3)
            {
                float fUdf = CalcTriangleUdf(
                    gVertices[jx].Position, 
                    gVertices[jx + 1].Position, 
                    gVertices[jx + 2].Position, 
                    p
                );

                if (fUdf < fUpperBound)
                {
                    fUpperBound = fUdf;
                    dwIntersectTriangleIndex = ix + Node.dwPrimitiveIndex;
                }
            }
        }

        [unroll]
        for (uint ix = 0; ix < Node.dwPrimitiveNum; ++ix)
        {
            fStack[dwTop++] = Node.dwPrimitiveIndex + ix;
        }
    }

    return fUpperBound;
}

int EstimateUdfSign(float3 p, float fRandom)
{
    uint dwRandomTriangleIndex = uint(fRandom) * (gPassCB.dwTriangleNum - 1);

    FVertex v0 = gVertices[dwRandomTriangleIndex * 3 + 0];
    FVertex v1 = gVertices[dwRandomTriangleIndex * 3 + 1];
    FVertex v2 = gVertices[dwRandomTriangleIndex * 3 + 2];

    float3 fCentroid = (v0.Position + v1.Position + v2.Position) / 3.0f;
    float3 d = fCentroid - p;

    // 用 1.0f / 0.0f 表示无穷远.
    int dwTriangleIndex = TraceTriangleIndex(p, d, 1.0f / 0.0f);
    if (dwTriangleIndex < 0) return 0;

    v0 = gVertices[dwTriangleIndex * 3 + 0];
    v1 = gVertices[dwTriangleIndex * 3 + 1];
    v2 = gVertices[dwTriangleIndex * 3 + 2];

    return dot(d, v0.Normal + v1.Normal + v2.Normal) < 0 ? 1 : -1; 
}

bool ContainsTriangle(float3 p, float fRadius)
{
    uint fStack[BVH_STACK_SIZE];
    fStack[0] = 0;
    uint dwTop = 1;
    
    while (dwTop > 0)
    {
        FNode Node = gNodes[fStack[--dwTop]];
        if (!IntersectBoxSphere(Node.Lower, Node.Upper, p, fRadius)) continue;

        if (Node.dwPrimitiveNum != 0)
        {
            for (uint ix = 0, jx = 3 * Node.dwPrimitiveIndex; ix < Node.dwPrimitiveNum; ++ix, jx += 3)
            {
                if (IntersectTriangleSphere(
                    gVertices[jx].Position, 
                    gVertices[jx + 1].Position, 
                    gVertices[jx + 2].Position, 
                    p, 
                    fRadius
                )) 
                    return true;
            }
            return false;
        }

        [unroll]
        for (uint ix = 0; ix < Node.dwPrimitiveNum; ++ix)
        {
            fStack[dwTop++] = Node.dwPrimitiveIndex + ix;
        }
    }

    return false;
}


int TraceTriangleIndex(float3 p, float3 d, float fMaxLength)
{
    int dwTriangleIndex = -1;
    float fLength = fMaxLength;

    uint fStack[BVH_STACK_SIZE];
    fStack[0] = 0;
    uint dwTop = 1;
    
    while (dwTop > 0)
    {
        FNode Node = gNodes[fStack[--dwTop]];
        if (!IntersectRayBox(p, d, 0, fLength, Node.Lower, Node.Upper)) continue;

        if (Node.dwPrimitiveNum != 0)
        {
            for (uint ix = 0, jx = 3 * Node.dwPrimitiveIndex; ix < Node.dwPrimitiveNum; ++ix, jx += 3)
            {
                float3 p0 = gVertices[jx].Position;
                float3 p1 = gVertices[jx + 1].Position;
                float3 p2 = gVertices[jx + 2].Position;

                float fNewLength = 0.0f;
                if (IntersectRayTriangle(p, d, fLength, p0, p1 - p0, p2 - p0, fNewLength))
                {
                    dwTriangleIndex = ix + Node.dwPrimitiveIndex;
                    fLength = fNewLength;
                }
            }
        }

        [unroll]
        for (uint ix = 0; ix < Node.dwPrimitiveNum; ++ix)
        {
            fStack[dwTop++] = Node.dwPrimitiveIndex + ix;
        }
    }

    return dwTriangleIndex;
}



#endif
