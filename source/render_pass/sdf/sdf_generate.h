#ifndef RENDER_PASS_SDF_GENERATE_H
#define RENDER_PASS_SDF_GENERATE_H
#include "../../render_graph/render_pass.h"
#include "../../scene/distance_field.h"
#include "../../core/tools/file.h"
#include "../../core/math/vector.h"
#include <memory>

namespace fantasy 
{
    namespace constant
    {
		struct SdfGeneratePassConstants
		{
			float3 sdf_lower;
			uint32_t triangle_num = 0;

			float3 sdf_upper;
			uint32_t dwSignRayNum = 3;

			float3 sdf_extent;
			uint32_t x_begin = 0;

			uint32_t x_end = 0;
			uint3 pad;
		};
    }

    class SdfGeneratePass : public RenderPassInterface
    {
    public:
		SdfGeneratePass() { type = RenderPassType::Precompute | RenderPassType::Exclude; }

        bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
        bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

        bool finish_pass() override;

    private:
        bool BuildBvh();

    private:
        uint32_t _begin_x = 0;
        bool _resource_writed = false;
        uint32_t _current_mesh_sdf_index = 0;
        DistanceField* _distance_field = nullptr;
        std::unique_ptr<serialization::BinaryOutput> _binary_output;
		constant::SdfGeneratePassConstants _pass_constants;

        std::shared_ptr<BufferInterface> _bvh_node_buffer;
        std::shared_ptr<BufferInterface> _bvh_vertex_buffer;
        std::shared_ptr<TextureInterface> _sdf_output_texture;
        std::unique_ptr<StagingTextureInterface> _read_back_texture;

        std::unique_ptr<BindingLayoutInterface> _binding_layout;
        
        std::unique_ptr<Shader> _cs;
        std::unique_ptr<ComputePipelineInterface> _pipeline;

        std::unique_ptr<BindingSetInterface> _binding_set;
        ComputeState _compute_state;
    };
}





#endif