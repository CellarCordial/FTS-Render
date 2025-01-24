#ifndef SHADER_INTERSECT_SLANG
#define SHADER_INTERSECT_SLANG

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

    return sqrt(
        (sign(dot(cross(ba, nor), pa)) +
         sign(dot(cross(cb, nor), pb)) +
         sign(dot(cross(ac, nor), pc)) < 2.0f)
        ?
        min(
            min(dot2(ba * clamp(dot(ba, pa) / dot2(ba), 0.0f, 1.0f) - pa),
                dot2(cb * clamp(dot(cb, pb) / dot2(cb), 0.0f, 1.0f) - pb)),
            dot2(ac * clamp(dot(ac, pc) / dot2(ac), 0.0f, 1.0f) - pc)
        )
        :
        dot(nor, pa) * dot(nor, pa) / dot2(nor) );
}

bool IntersectBoxSphere(float3 lower, float3 upper, float3 p, float fRadius)
{
    float3 q = clamp(p, lower, upper);
    return dot(p - q, p - q) <= fRadius * fRadius;
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


bool IntersectRayBox(float3 o, float3 d, float t0, float t1, float3 lower, float3 upper)
{
    float3 inv_d = 1.0f / d;

    float3 near = (lower - o) * inv_d;
    float3 far = (upper - o) * inv_d;

    float3 max_nf = max(near, far);
    float3 min_nf = min(near, far);

    t0 = max4(t0, min_nf.x, min_nf.y, min_nf.z);
    t1 = min4(t1, max_nf.x, max_nf.y, max_nf.z);

    return t0 <= t1;
}

bool IntersectRayBox(float3 o, float3 d, float3 lower, float3 upper, out float step)
{
    float3 inv_d = 1.0f / d;

    float3 near = (lower - o) * inv_d;
    float3 far = (upper - o) * inv_d;

    float3 max_nf = max(near, far);
    float3 min_nf = min(near, far);

    float t0 = max3(min_nf.x, min_nf.y, min_nf.z);
    float t1 = min3(max_nf.x, max_nf.y, max_nf.z);

    step = max(t0, 0.0f) + 0.01f;

    return t0 <= t1;
}

bool intersect_ray_box_inside(float3 o, float3 d, float3 lower, float3 upper, out float step)
{
    float3 inv_d = 1.0f / d;

    float3 near = (lower - o) * inv_d;
    float3 far = (upper - o) * inv_d;

    float3 max_nf = max(near, far);
    float3 min_nf = min(near, far);

    float t0 = max3(min_nf.x, min_nf.y, min_nf.z);
    float t1 = min3(max_nf.x, max_nf.y, max_nf.z);

    step = max(t1, 0.0f) + 0.01f;

    return t0 <= t1;
}

float InterectRayPlane(in float3 ro, in float3 rd, in float4 p)
{
    return -(dot(ro, p.xyz) + p.w) / dot(rd, p.xyz);
}

float IntersectRayBoxPlane(float3 o, float3 d, float3 lower, float3 upper, out float3 normal)
{
    normal = float3(0.0f, 0.0f, 0.0f);
    float step = 1000.0f;
    float t;
    
    if (o.x < lower.x || o.x > upper.x)
    {
        t = (upper.x - o.x) / d.x;
        if (t > 0.0f)
        {
            float3 p = o + t * d;
            float3 ClampPos = clamp(p, lower, upper);
            if (length(ClampPos - p) < 0.001f && t - step < 0.001f)
            {
                normal = float3(1.0f, 0.0f, 0.0f);
                step = t;
            }
        }

        t = (lower.x - o.x) / d.x;
        if (t > 0.0f)
        {
            float3 p = o + t * d;
            float3 ClampPos = clamp(p, lower, upper);
            if (length(ClampPos - p) < 0.001f && t - step < 0.001f)
            {
                normal = float3(-1.0f, 0.0f, 0.0f);
                step = t;
            }
        }
    }


    if (o.y < lower.y || o.y > upper.y)
    {
        t = (upper.y - o.y) / d.y;
        if (t > 0.0f)
        {
            float3 p = o + t * d;
            float3 ClampPos = clamp(p, lower, upper);
            if (length(ClampPos - p) < 0.001f && t - step < 0.001f)
            {
                normal = float3(0.0f, 1.0f, 0.0f);
                step = t;
            }
        }

        t = (lower.y - o.y) / d.y;
        if (t > 0.0f)
        {
            float3 p = o + t * d;
            float3 ClampPos = clamp(p, lower, upper);
            if (length(ClampPos - p) < 0.001f && t - step < 0.001f)
            {
                normal = float3(0.0f, -1.0f, 0.0f);
                step = t;
            }
        }
    }

    if (o.z < lower.z || o.z > upper.z)
    {
        t = (upper.z - o.z) / d.z;
        if (t > 0.0f)
        {
            float3 p = o + t * d;
            float3 ClampPos = clamp(p, lower, upper);
            if (length(ClampPos - p) < 0.001f && t - step < 0.001f)
            {
                normal = float3(0.0f, 0.0f, 1.0f);
                step = t;
            }
        }

        t = (lower.z - o.z) / d.z;
        if (t > 0.0f)
        {
            float3 p = o + t * d;
            float3 ClampPos = clamp(p, lower, upper);
            if (length(ClampPos - p) < 0.001f && t - step < 0.001f)
            {
                normal = float3(0.0f, 0.0f, -1.0f);
                step = t;
            }
        }
    }

    return step;
}



bool IntersectRayTriangle(float3 o, float3 d, float fMaxLength, float3 a, float3 ab, float3 ac, out float fLength)
{
    fLength = 0.0f;

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

// 默认圆心在坐标系原点.
bool IntersectRayCircle(float2 o, float2 d, float r, out float fClosestIntersectDistance)
{
    // 圆: o^2 + d^2 = r^2 和 射线: o + td 相交得 At^2 + Bt + C - r_2 = 0.
    // A = d_x^2 + d_y^2, B = 2(o_x * d_x + o_y * d_y), C = o_x^2 + o_y^2.
    fClosestIntersectDistance = 0.0f;

    float A = dot(d, d);
    float B = 2.0f * dot(o, d);
    float C = dot(o, o) - r * r;
    float delta = B * B - 4.0f * A * C;
    if (delta < 0.0f) return false;

    fClosestIntersectDistance = (-B + (C <= 0 ? sqrt(delta) : -sqrt(delta))) / (2.0f * A);

    // 现确认光线所在直线与圆相交.
    // C <= 0: 圆心到光线起点 o 的距离小于 r, 即光线从圆内部射出, 若 true, 则一定与圆相交.
    // B <= 0: 光线方向朝向圆心, 若 true, 则一定与圆相交.
    return C <= 0 || B <= 0;
}

bool intersect_ray_sphere(float3 o, float3 d, float r, out float ClosestIntersectDistance)
{
    ClosestIntersectDistance = 0.0f;
    
    float A = dot(d, d);
    float B = 2.0f * dot(o, d);
    float C = dot(o, o) - r * r;
    float delta = B * B - 4.0f * A * C;
    if (delta < 0.0f) return false;

    ClosestIntersectDistance = (-B + (C <= 0 ? sqrt(delta) : -sqrt(delta))) / (2.0f * A);

    return C <= 0 || B <= 0;
}

bool intersect_ray_sphere(float3 o, float3 d, float r)
{
    float A = dot(d, d);
    float B = 2.0f * dot(o, d);
    float C = dot(o, o) - r * r;
    float delta = B * B - 4.0f * A * C;
    return delta >= 0.0f && (C <= 0 || B <= 0);
}

#endif