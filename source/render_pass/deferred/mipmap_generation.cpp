#include "mipmap_generation.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/virtual_texture.h"
#include "../../scene/geometry.h"
#include <cmath>
#include <cstdint>
#include <string>


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 8u
#define THREAD_GROUP_SIZE_Y 8u
 
	bool MipmapGenerationPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        ReturnIfFalse(cache->get_world()->each<event::ModelLoaded>(
            [this](Entity* entity, event::ModelLoaded* event) -> bool
            {
				event->add_event(
					[&]() 
					{ 
						_current_model = entity; 
						return true; 
					}
				);
				return true;
            }
        ));

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(3);
			binding_layout_items[0] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[1] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[2] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "deferred/generate_mipmap_cs.slang";
			cs_compile_desc.entry_point = "compute_shader";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			ShaderData cs_data = shader_compile::compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "compute_shader";
			cs_desc.shader_type = ShaderType::Compute;
			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.compute_shader = _cs.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));
		}

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
			_compute_state.pipeline = _pipeline.get();
		}

        ReturnIfFalse(_linear_clamp_sampler = check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
 
		return true;
	}

	std::string convert_image_name(uint32_t image_index, const std::string& model_name)
	{
		std::string texture_name = model_name;
		switch (image_index) 
		{
		case Material::TextureType_BaseColor: texture_name += "_base_color"; break;
		case Material::TextureType_Normal:  texture_name += "_normal"; break;
		case Material::TextureType_Metallic:  texture_name += "_metallic"; break;
		case Material::TextureType_Roughness:  texture_name += "_roughness"; break;
		case Material::TextureType_Emissive:  texture_name += "_emissive"; break;
		case Material::TextureType_Occlusion:  texture_name += "_occlusion"; break;
		default: break;
		}
		return texture_name;
	}

	bool MipmapGenerationPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		if (_current_model)
		{
			ReturnIfFalse(cmdlist->open());

			DeviceInterface* device = cmdlist->get_deivce();

			const auto& model_name = *_current_model->get_component<std::string>();

			const auto& material = _current_model->get_component<Material>();
			const auto& image = 
				material->submaterials[_current_submaterial_index].images[_current_image_index];

			uint32_t mip_levels = std::log2(previous_power_of_2(std::max(image.width, image.height)) / page_size) + 1;
			_textures.resize(mip_levels);

			ReturnIfFalse(_textures[0] = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_shader_resource(
					image.width, 
					image.height,
					image.format,
					true,
					convert_image_name(_current_image_index, model_name) + "_mip0"
				)
			)));
			cache->collect(_textures[0], ResourceType::Texture);
			ReturnIfFalse(cmdlist->write_texture(_textures[0].get(), 1, 1, image.data.get(), image.size / image.height));


			uint2 texture_resolution = uint2(image.width, image.height);
			for (uint32_t mip_level = 1; mip_level <= mip_levels - 1; ++mip_level)
			{
				texture_resolution.x >>= 1;
				texture_resolution.y >>= 1;
				ReturnIfFalse(_textures[mip_level] = std::shared_ptr<TextureInterface>(device->create_texture(
					TextureDesc::create_shader_resource(
						texture_resolution.x, 
						texture_resolution.y,
						image.format,
						true,
						convert_image_name(_current_image_index, model_name) + "_mip" + std::to_string(mip_level)
					)
				)));
				cache->collect(_textures[mip_level], ResourceType::Texture);

				// Binding Set.
				{
					BindingSetItemArray binding_set_items(3);
					binding_set_items[0] = BindingSetItem::create_texture_srv(0, _textures[mip_level - 1]);
					binding_set_items[1] = BindingSetItem::create_texture_uav(0, _textures[mip_level]);
					binding_set_items[2] = BindingSetItem::create_sampler(0, _linear_clamp_sampler);
					ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
						BindingSetDesc{ .binding_items = binding_set_items },
						_binding_layout.get()
					)));
				}

				uint2 thread_group_num = {
					static_cast<uint32_t>((align(texture_resolution.x, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
					static_cast<uint32_t>((align(texture_resolution.y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
				};

				ReturnIfFalse(cmdlist->set_compute_state(_compute_state));
				ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y));
			}

			ReturnIfFalse(cmdlist->close());
		}

		return true;
	}

	bool MipmapGenerationPass::finish_pass()
	{
		_current_image_index++;
		if (_current_image_index == Material::TextureType_Num)
		{
			_current_image_index = 0;
			_current_submaterial_index++;
			if (_current_submaterial_index == _current_model->get_component<Material>()->submaterials.size()) 
			{
				_current_model = nullptr;
			}
			else 
			{
				recompute();
			}
		}
	
		_binding_layout.reset();
		_textures.clear();
		return true;
	}
}






