#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../render_graph/render_pass.h"
#include "../../scene/geometry.h"
#include <array>
#include <memory>

namespace fantasy
{
	namespace constant 
	{
		struct MipmapGenerationPassConstant
		{
			uint2 output_resolution;
			uint32_t input_mip_level;
		};
	}

	class MipmapGenerationPass : public RenderPassInterface
	{
	public:
		MipmapGenerationPass() { type = RenderPassType::Precompute | RenderPassType::Exclude; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;
        bool finish_pass(RenderResourceCache* cache) override;

	private:
		Entity* _current_model = nullptr;
		uint32_t _current_mip_levels = 0;
		uint2 _current_texture_resolution;
		uint32_t _current_calculate_mip = 1;
		uint32_t _current_submaterial_index = 0;
		std::array<std::shared_ptr<TextureInterface>, Material::TextureType_Num> _current_textures;
		std::array<constant::MipmapGenerationPassConstant, Material::TextureType_Num> _pass_constants;

		uint64_t _geometry_texture_heap_capacity = 1024 * 1024 * 64;
		std::shared_ptr<HeapInterface> _geometry_texture_heap;
		uint64_t _current_heap_offset = 0;

        std::shared_ptr<SamplerInterface> _linear_clamp_sampler;

		std::shared_ptr<BindingLayoutInterface> _binding_layout;

		std::shared_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;

		std::array<std::unique_ptr<BindingSetInterface>, Material::TextureType_Num> _binding_sets;
		ComputeState _compute_state;
	};
}
#endif
