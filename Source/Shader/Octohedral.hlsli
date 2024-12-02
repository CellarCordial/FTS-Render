#ifndef SHADER_OCOTOHEDRAL_HLSLI
#define SHADER_OCOTOHEDRAL_HLSLI



float2 UnitVectorToOctahedron(float3 v)
{
    // dot(1.0f, abs(v)) = abs(v.x) + abs(v.y) + abs(v.z)
    v.xz = v.xz / dot(float3(1.0f, 1.0f, 1.0f), abs(v));

    if (v.y <= 0.0f)
    {
        v.xz = (1.0f - abs(v.zx)) * (v.xz >= 0 ? float2(1.0f, 1.0f) : float2(-1.0f, -1.0f));
    }

    return v.xz;
}

float3 OctahedronToUnitVector(float2 Oct)
{
    float3 v = float3(Oct, 1.0f - dot(float2(1.0f, 1.0f), abs(Oct)));

    if (v.y <= 0.0f)
    {
        v.xz = (1.0f - abs(v.zx)) * (v.xz >= 0 ? float2(1.0f, 1.0f) : float2(-1.0f, -1.0f));
    }

    return normalize(v);
}











#endif