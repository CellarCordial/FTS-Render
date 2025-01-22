// #define UPPER_BOUND_ESTIMATE_PRECISON 0
// #define BVH_STACK_SIZE 1
// #define GROUP_THREAD_NUM_Y 1
// #define GROUP_THREAD_NUM_Z 1

#include "../common/intersect.slang"

cbuffer pass_constant : register(b0)
{
    float3 sdf_lower;    uint32_t triangle_count;
    float3 sdf_upper;    uint32_t sign_ray_count;
    float3 sdf_extent;   uint32_t x_begin;
    uint32_t x_end;        uint3 pad;
};

struct Node
{
    float3 lower;
    float3 upper;
    uint32_t child_index;  // 叶子节点时, 代表 triangle index; 非叶子节点, 代表 node index.
    uint32_t child_count;
};

struct Vertex
{
    float3 position;
    float3 normal;
};

StructuredBuffer<Node> node_buffer : register(t0);
StructuredBuffer<Vertex> vertex_buffer : register(t1);

RWTexture3D<float> sdf_texture : register(u0);


float calculate_sdf(float3 p, float upper_bound);
float calculate_upper_bound(float3 p, uint32_t precsion);
float calculate_udf(float3 p, float upper_bound, out uint32_t intersect_triangle_index);
int32_t estimate_udf_sign(float3 p, float random);
bool contains_triangle(float3 p, float radius);
int32_t trace_triangle_index(float3 p, float3 d, float max_length);

#if defined(UPPER_BOUND_ESTIMATE_PRECISON) && defined(BVH_STACK_SIZE) && defined(GROUP_THREAD_NUM_Y) && defined(GROUP_THREAD_NUM_Z)

[shader("compute")]
[numthreads(1, GROUP_THREAD_NUM_Y, GROUP_THREAD_NUM_Z)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    uint32_t width, height, depth;
    sdf_texture.GetDimensions(width, height, depth);

    if (thread_id.y >= height || thread_id.z >= depth) return;

    // X 轴步长
    float dx = 1.05f * sdf_extent.x / width;

    // fx, fy, fz 相当于 voxel 的 uvw.

    float fy = lerp(sdf_lower.y, sdf_upper.y, (thread_id.y + 0.5f) / height);
    float fz = lerp(sdf_lower.z, sdf_upper.z, (thread_id.z + 0.5f) / depth);

    float last_udf = -100.0f * dx;
    for (uint32_t x = x_begin; x < x_end; ++x)
    {
        float fx = lerp(sdf_lower.x, sdf_upper.x, (x + 0.5f) / width);

        // u(q) <= u(p) + ||p - q|| 
        float upper_bound = last_udf + dx;
        float new_sdf = calculate_sdf(float3(fx, fy, fz), upper_bound);
        last_udf = abs(new_sdf);

        sdf_texture[uint3(x, thread_id.yz)] = new_sdf;
    }
}


float calculate_sdf(float3 p, float upper_bound)
{
    if (upper_bound < 0) upper_bound = calculate_upper_bound(p, UPPER_BOUND_ESTIMATE_PRECISON);

    uint32_t intersect_triangle_index;
    float udf = calculate_udf(p, upper_bound, intersect_triangle_index);

    int32_t udf_sign = 0;
    
    for (uint32_t ix = 0; ix < sign_ray_count; ++ix)
    {
        udf_sign += estimate_udf_sign(p, lerp(0.0f, 1.0f, (ix + 0.5f) / sign_ray_count));
    }

    if (udf_sign > 0) return udf;
    if (udf_sign < 0) return -udf;

    Vertex v0 = vertex_buffer[intersect_triangle_index * 3 + 0];
    Vertex v1 = vertex_buffer[intersect_triangle_index * 3 + 1];
    Vertex v2 = vertex_buffer[intersect_triangle_index * 3 + 2];
    
    int32_t s0 = dot(p - v0.position, v0.normal) > 0 ? 1 : -1;
    int32_t s1 = dot(p - v1.position, v1.normal) > 0 ? 1 : -1;
    int32_t s2 = dot(p - v2.position, v2.normal) > 0 ? 1 : -1;

    return s0 + s1 + s2 > 0 ? udf : -udf;
}


float calculate_upper_bound(float3 p, uint32_t precsion)
{
    Node node = node_buffer[0];
    float fNear = 0.0f;
    float fFar = distance(0.5f * (node.lower + node.upper), p) + distance(node.lower, node.upper);
    
    for (uint32_t ix = 0; ix < precsion; ++ix)
    {
        float fMid = 0.5f * (fNear + fFar);
        if (contains_triangle(p, fMid)) fFar = fMid;
        else fNear = fMid;
    }
    return fFar;
}

float calculate_udf(float3 p, float upper_bound, out uint32_t intersect_triangle_index)
{
    uint32_t stack[BVH_STACK_SIZE];
    stack[0] = 0;
    uint32_t top = 1;
    
    while (top > 0)
    {
        Node node = node_buffer[stack[--top]];
        if (!IntersectBoxSphere(node.lower, node.upper, p, upper_bound)) continue;

        if (node.child_count != 0)
        {
            for (uint32_t ix = 0, jx = 3 * node.child_index; ix < node.child_count; ++ix, jx += 3)
            {
                float udf = CalcTriangleUdf(
                    vertex_buffer[jx].position, 
                    vertex_buffer[jx + 1].position, 
                    vertex_buffer[jx + 2].position, 
                    p
                );

                if (udf < upper_bound)
                {
                    upper_bound = udf;
                    intersect_triangle_index = ix + node.child_index;
                }
            }
        }
        else
        {
            stack[top++] = node.child_index;
            stack[top++] = node.child_index + 1;
        }
    }

    return upper_bound;
}

int32_t estimate_udf_sign(float3 p, float random)
{
    uint32_t random_triangle_index = uint32_t(random) * (triangle_count - 1);

    Vertex v0 = vertex_buffer[random_triangle_index * 3 + 0];
    Vertex v1 = vertex_buffer[random_triangle_index * 3 + 1];
    Vertex v2 = vertex_buffer[random_triangle_index * 3 + 2];

    float3 fCentroid = (v0.position + v1.position + v2.position) / 3.0f;
    float3 d = fCentroid - p;

    // 用 1.0f / 0.0f 表示无穷远.
    int32_t triangle_index = trace_triangle_index(p, d, 1.0f / 0.0f);
    if (triangle_index < 0) return 0;

    v0 = vertex_buffer[triangle_index * 3 + 0];
    v1 = vertex_buffer[triangle_index * 3 + 1];
    v2 = vertex_buffer[triangle_index * 3 + 2];

    return dot(d, v0.normal + v1.normal + v2.normal) < 0 ? 1 : -1; 
}

// 遍历所有三角形, 发现相交就返回.
bool contains_triangle(float3 p, float radius)
{
    uint32_t stack[BVH_STACK_SIZE];
    stack[0] = 0;
    uint32_t top = 1;
    
    while (top > 0)
    {
        Node node = node_buffer[stack[--top]];
        if (!IntersectBoxSphere(node.lower, node.upper, p, radius)) continue;

        if (node.child_count != 0) // 是否叶子节点.
        {
            for (uint32_t ix = 0, jx = 3 * node.child_index; ix < node.child_count; ++ix, jx += 3)
            {
                if (IntersectTriangleSphere(
                    vertex_buffer[jx].position, 
                    vertex_buffer[jx + 1].position, 
                    vertex_buffer[jx + 2].position, 
                    p, 
                    radius
                )) 
                    return true;
            }
            return false;
        }
        else
        {
            stack[top++] = node.child_index;
            stack[top++] = node.child_index + 1;
        }
    }

    return false;
}


int32_t trace_triangle_index(float3 p, float3 d, float max_length)
{
    int32_t triangle_index = -1;
    float length = max_length;

    uint32_t stack[BVH_STACK_SIZE];
    stack[0] = 0;
    uint32_t top = 1;
    
    while (top > 0)
    {
        Node node = node_buffer[stack[--top]];
        if (!IntersectRayBox(p, d, 0, length, node.lower, node.upper)) continue;

        if (node.child_count != 0)
        {
            for (uint32_t ix = 0, jx = 3 * node.child_index; ix < node.child_count; ++ix, jx += 3)
            {
                float3 p0 = vertex_buffer[jx].position;
                float3 p1 = vertex_buffer[jx + 1].position;
                float3 p2 = vertex_buffer[jx + 2].position;

                float fNewLength = 0.0f;
                if (IntersectRayTriangle(p, d, length, p0, p1 - p0, p2 - p0, fNewLength))
                {
                    triangle_index = ix + node.child_index;
                    length = fNewLength;
                }
            }
        }
        else
        {
            stack[top++] = node.child_index;
            stack[top++] = node.child_index + 1;
        }
    }

    return triangle_index;
}



#endif
