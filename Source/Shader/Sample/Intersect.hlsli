#ifndef SHADER_INTERSECT_HLSLI
#define SHADER_INTERSECT_HLSLI

float dot2(float3 v)
{
    return dot(v, v);
}


float CalcTriangleUdf(float3 a, float3 b, float3 c, float3 p)
{
    float3 ba = b - a; float3 pa = p - a;
    float3 cb = c - b; float3 pb = p - b;
    float3 ac = a - c; float3 pc = p - c;
    float3 nor = cross(ba, ac);

    if(sign(dot(cross(ba, nor), pa)) +
       sign(dot(cross(cb, nor), pb)) +
       sign(dot(cross(ac, nor), pc)) < 2)
    {
        return min(min(
            dot2(ba * clamp(dot(ba, pa) / dot2(ba), 0.0f, 1.0f) - pa),
            dot2(cb * clamp(dot(cb, pb) / dot2(cb), 0.0f, 1.0f) - pb)),
            dot2(ac * clamp(dot(ac, pc) / dot2(ac), 0.0f, 1.0f) - pc));
    }

    return dot(nor, pa) * dot(nor, pa) / dot2(nor);
}

bool IntersectBoxSphere(float3 Lower, float3 Upper, float3 p, float fRadius)
{
    // 使用 dot 来避免开方运算.
    float3 q = clamp(p, Lower, Upper);
    return dot(p - q, q - p) <= fRadius * fRadius;
}

bool IntersectTriangleSphere(float3 v0, float3 v1, float3 v2, float3 p, float fRadius)
{
    return CalcTriangleUdf(v0, v1, v2, p) <= fRadius *fRadius;
}

float max4(float x, float y, float z, float w)
{
    return max(max(x, y), max(z, w));
}

float min4(float x, float y, float z, float w)
{
    return min(min(x, y), min(z, w));
}

float max3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float min3(float x, float y, float z)
{
    return min(x, min(y, z));
}


bool IntersectRayBox(float3 o, float3 d, float fT0, float fT1, float3 Lower, float3 Upper)
{
    float3 InvD = 1.0f / d;

    float3 fNear = (Lower - o) * InvD;
    float3 fFar = (Upper - o) * InvD;

    float3 fMaxNF = max(fNear, fFar);
    float3 fMinNF = min(fNear, fFar);

    fT0 = max4(fT0, fMinNF.x, fMinNF.y, fMinNF.z);
    fT1 = min4(fT1, fMaxNF.x, fMaxNF.y, fMaxNF.z);

    return fT0 <= fT1;
}

float2 IntersectRayBox(float3 o, float3 d, float3 Lower, float3 Upper, out float fStep)
{
    float3 InvD = 1.0f / d;

    float3 fNear = (Lower - o) * InvD;
    float3 fFar = (Upper - o) * InvD;

    float3 fMaxNF = max(fNear, fFar);
    float3 fMinNF = min(fNear, fFar);

    float fT0 = max3(fMinNF.x, fMinNF.y, fMinNF.z);
    float fT1 = min3(fMaxNF.x, fMaxNF.y, fMaxNF.z);

    fStep = max(fT0, 0.0f) + 0.01f;

    return fT0 <= fT1;
}

float2 IntersectRayBoxEnd(float3 o, float3 d, float3 Lower, float3 Upper, out float fStep)
{
    float3 InvD = 1.0f / d;

    float3 fNear = (Lower - o) * InvD;
    float3 fFar = (Upper - o) * InvD;

    float3 fMaxNF = max(fNear, fFar);
    float3 fMinNF = min(fNear, fFar);

    float fT0 = max3(fMinNF.x, fMinNF.y, fMinNF.z);
    float fT1 = min3(fMaxNF.x, fMaxNF.y, fMaxNF.z);

    fStep = max(fT1, 0.0f) + 0.01f;

    return fT0 <= fT1;
}

bool IntersectRayTriangle(float3 o, float3 d, float fMaxLength, float3 a, float3 ab, float3 ac, out float fLength)
{
    float3 S = o - a;
    float3 S1 = cross(d, ac);
    float3 S2 = cross(S, ab);
    float fInvDenom = 1.0f / dot(S1, ab);
    
    float t = dot(S2, ac) * fInvDenom;
    float b1 = dot(S1, S) * fInvDenom;
    float b2 = dot(S2, d) * fInvDenom;

    if (t < 0 || t > fMaxLength || b1 < 0 || b2 < 0 || b1 + b2 > 1.0f) return false;
    
    fLength = t;
    return true;
}


#endif