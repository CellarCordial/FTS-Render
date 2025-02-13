#include "multi_scattering_lut.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/light.h"
#include <cySampleElim.h>
#include <cyPoint.h>
#include <memory>
#include <string>
#include <random>
#include <vector>
    
namespace fantasy 
{
#define DIRECTION_SAMPLE_COUNT 64
#define THREAD_GROUP_SIZE_X 16 
#define THREAD_GROUP_SIZE_Y 16 
#define MULTI_SCATTERING_LUT_RES 256

    void MultiScatteringLUTPass::create_poisson_disk_samples()
    {
		std::default_random_engine random_engine{ std::random_device()() };
		std::uniform_real_distribution<float> distribution(0, 1);

		std::vector<cy::Point2f> raw_points;
		for (uint32_t ix = 0; ix < DIRECTION_SAMPLE_COUNT * 10; ++ix)
		{
			raw_points.push_back({ distribution(random_engine), distribution(random_engine) });
		}

		std::vector<cy::Point2f> output_points(DIRECTION_SAMPLE_COUNT);

		cy::WeightedSampleElimination<cy::Point2f, float, 2> elimination;
		elimination.SetTiling(true);
		elimination.Eliminate(
			raw_points.data(), raw_points.size(),
			output_points.data(), output_points.size()
		);

		for (auto& p : output_points) _dir_samples.push_back({ p.x, p.y });
    }


    bool MultiScatteringLUTPass::compile(DeviceInterface* device, RenderResourceCache* cache)
    {
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(6);
			binding_layout_items[0] = BindingLayoutItem::create_constant_buffer(0);
			binding_layout_items[1] = BindingLayoutItem::create_constant_buffer(1);
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[3] = BindingLayoutItem::create_structured_buffer_srv(1);
			binding_layout_items[4] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[5] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

        // Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "atmosphere/multi_scattering_lut_cs.slang";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_compile_desc.defines.push_back("DIRECTION_SAMPLE_COUNT=" + std::to_string(DIRECTION_SAMPLE_COUNT));
			ShaderData cs_data = compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}

        // Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.compute_shader = _cs;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

        // Buffer.
		{
			ReturnIfFalse(_pass_constant_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                BufferDesc::create_constant_buffer(
					sizeof(constant::MultiScatteringPassConstant),
					"multiscattering_pass_constant_buffer" 
				)
			)));
			ReturnIfFalse(_dir_sample_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
                BufferDesc::create_structured_buffer(
					sizeof(uint2) * DIRECTION_SAMPLE_COUNT, 
					sizeof(uint2),
					"direction_sample_buffer" 
				)
			)));

		}

		// Texture.
		{
			ReturnIfFalse(_multi_scattering_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write_texture(
					MULTI_SCATTERING_LUT_RES,
					MULTI_SCATTERING_LUT_RES,
					Format::RGBA32_FLOAT,
					"multi_scattering_texture"
				)
			)));
			cache->collect(_multi_scattering_texture, ResourceType::Texture);
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(6);
			binding_set_items[0] = BindingSetItem::create_constant_buffer(0, check_cast<BufferInterface>(cache->require("atmosphere_properties_buffer")));
			binding_set_items[1] = BindingSetItem::create_constant_buffer(1, _pass_constant_buffer);
			binding_set_items[2] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("transmittance_texture")));
			binding_set_items[3] = BindingSetItem::create_structured_buffer_srv(1, _dir_sample_buffer);
			binding_set_items[4] = BindingSetItem::create_texture_uav(0, _multi_scattering_texture);
			binding_set_items[5] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout
			)));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.get());
			_compute_state.pipeline = _pipeline.get();
		}

        return true;
    }

    bool MultiScatteringLUTPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
    {
		ReturnIfFalse(cmdlist->open());

		// Update constant.
		{
			ReturnIfFalse(cache->get_world()->each<DirectionalLight>(
				[this](Entity* entity, DirectionalLight* light) -> bool
				{
					_pass_constants.sun_intensity = float3(light->intensity * light->color);
					return true;
				}
			));
			float3* groud_albedo;
			ReturnIfFalse(cache->require_constants("ground_albedo", reinterpret_cast<void**>(&groud_albedo)));
			_pass_constants.ground_albedo = *groud_albedo;

			ReturnIfFalse(cmdlist->write_buffer(_pass_constant_buffer.get(), &_pass_constants, sizeof(constant::MultiScatteringPassConstant)));
		}
		
		if (!_resource_writed)
		{
			ReturnIfFalse(cmdlist->write_buffer(_dir_sample_buffer.get(), _dir_samples.data(), _dir_samples.size() * sizeof(float2)));
			_resource_writed = true;
		}

		uint2 thread_group_num = {
			static_cast<uint32_t>(align(MULTI_SCATTERING_LUT_RES, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X),
			static_cast<uint32_t>(align(MULTI_SCATTERING_LUT_RES, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
		};

		ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y));

		ReturnIfFalse(cmdlist->close());

        return true;
    }

	bool MultiScatteringLUTPass::finish_pass()
	{
		if (!_dir_samples.empty())
		{
			_dir_samples.clear();
			_dir_samples.shrink_to_fit();
		}

		return true;
	}

}
