#define THREAD_GROUP_NUM_X 1
#define THREAD_GROUP_NUM_Y 1
#define TEMPORAL

cbuffer pass_constants : register(b0)
{
    float4x4 proj_matrix;

    uint2 ssao_texture_res;
    float sample_radius;
    float sample_bias;

    uint tangent_noise_texture_res;
    uint sample_num;
    uint max_sample_num;
    uint pad;
};

Texture2D<float3> view_space_position_texture : register(t0);
Texture2D<float3> view_space_normal_texture : register(t1);
Texture2D<float3> tangent_noise_texture : register(t2);
SamplerState sampler : register(s0);

StructuredBuffer<float3> tangent_space_sample_dirs : register(t3);

#ifdef TEMPORAL
Texture2D<float3> view_space_view_space_velocity_texture : register(t4);
Texture2D<float2> reprojection_texture : register(t5);
Texture2D<float2> history_ssao_texture : register(t6);
#endif

RWTexture2D<float2> ssao_texture : register(u0);

#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void compute_shader(uint3 thread_id: SV_Dispatchthread_id)
{
    float3 view_space_position = view_space_position_texture[thread_id.xy];
    float3 view_space_normal = view_space_normal_texture[thread_id.xy];
    float3 random_tangent = tangent_noise_texture[thread_id.xy * (ssao_texture_res / tangent_noise_texture_res)];

#ifdef TEMPORAL
    float3 view_space_velocity = view_space_view_space_velocity_texture[thread_id.xy];
    float3 prev_view_space_position = view_space_position + view_space_velocity;

    bool discard_history_sample_accumulation = false;
#endif

    // Gramm-Schmidt Process.
    float3 tangent = normalize(random_tangent - view_space_normal * dot(view_space_normal, random_tangent));
    float3 bitangent = cross(view_space_normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, view_space_normal);

    float occlusion_sum = 0.0f;
    for (uint ix = 0; ix < sample_num; ++ix)
    {
        float3 view_space_sample_dir = mul(tangent_space_sample_dirs[ix], TBN);
        float3 view_space_sample_position = view_space_position + view_space_sample_dir * sample_radius;

        float4 proj_space_sample_position = mul(float4(view_space_sample_position, 1.0f), proj_matrix);
        float2 sample_position_uv = ((proj_space_sample_position.xy / proj_space_sample_position.w) * float2(0.5f, -0.5f) + 0.5f);

        float sample_depth = view_space_position_texture.Sample(sampler, sample_position_uv).z;
        float range_check = smoothstep(0.0f, 1.0f, sample_radius / abs(view_space_position.z - sample_depth));

        occlusion_sum += (sample_depth <= view_space_sample_position.z + sample_bias ? 1.0f : 0.0f) * range_check;

#ifdef TEMPORAL
        //
        // ||s_i - p| - |s_iold - p_old|| < epsilon         (Func0)
        //
        float3 prev_view_space_sample_position = view_space_sample_position + view_space_velocity;
        if (abs(length(view_space_sample_position - view_space_position) - length(prev_view_space_sample_position - prev_view_space_position)) < 1e-5)
        {
            discard_history_sample_accumulation = true;
        }
#endif
    }

#ifdef TEMPORAL
    float2 uv_offset = reprojection_texture[thread_id.xy];
    float2 history_uv = uv_offset + float2(
        (thread_id.x + 0.5f) / ssao_texture_res.x,
        (thread_id.y + 0.5f) / ssao_texture_res.y
    );

    //        d_new
    // |1 - --------- | < epsilon               (Func1)
    //        d_old
    if (abs(1.0f - view_space_position.z / prev_view_space_position.z) < 1e-5)
    {
        discard_history_sample_accumulation = true;
    }

    // 若 (Func0) 或 (Func1) 达成条件, 则舍弃之前积累的 Occlusion. (Occlusion.y = n_t(p) / n_max)
    float2 history_occlusition = history_ssao_texture.Sample(sampler, history_uv);
    history_occlusition.y *= discard_history_sample_accumulation ? 0 : max_sample_num;

    // n_{t+1}(p) = min(n_t(p) + k * n_max)
    float new_sample_num_accumulation = min(max_sample_num, history_occlusition.y + sample_num);

    //                 n_t(p) * AO_t(p_old) + kC_{t+1}(p)
    // AO_{t+1}(p) = --------------------------------------
    //                          n_t(p) + k
    float occlusion = (history_occlusition.x * history_occlusition.y + occlusion_sum * sample_num) / new_sample_num_accumulation;

    ssao_texture[thread_id.xy] = float2(occlusion, new_sample_num_accumulation / max_sample_num);

#else
    ssao_texture[thread_id.xy] = float2(occlusion_sum, 0.0f);

#endif
}
 





#endif