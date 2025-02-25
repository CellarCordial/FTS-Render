#include "mipmap_generation.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/virtual_texture.h"
#include "../../scene/geometry.h"
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 8u
#define THREAD_GROUP_SIZE_Y 8u
 
	bool MipmapGenerationPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        cache->get_world()->get_global_entity()->get_component<event::GenerateMipmap>()->add_event(
            [this](Entity* entity) -> bool
            {
				_current_model = entity; 
				return true;
            }
        );

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

		// Texture Heap.
		{
			ReturnIfFalse(_geometry_texture_heap = std::shared_ptr<HeapInterface>(device->create_heap(
				HeapDesc{ 
					.name = "geometry_texture_heap", 
					.type = HeapType::Upload, 
					.capacity = _geometry_texture_heap_capacity 
				}
			)));
			cache->collect(_geometry_texture_heap, ResourceType::Heap);
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "deferred/generate_mipmap_cs.hlsl";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
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

		// Compute state.
		{
			_compute_state.binding_sets.resize(1);
			_compute_state.pipeline = _pipeline.get();
		}

        ReturnIfFalse(_linear_clamp_sampler = check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
 
		return true;
	}

	bool MipmapGenerationPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		if (_current_model)
		{
			ReturnIfFalse(cmdlist->open());

			DeviceInterface* device = cmdlist->get_deivce();

			const auto& model_name = *_current_model->get_component<std::string>();

			const Material* material = _current_model->get_component<Material>();
			const auto& image = material->submaterials[_current_submaterial_index].images[_current_image_index];

			uint32_t mip_levels = std::log2( material->image_resolution / VT_PAGE_SIZE) + 1;
			_textures.resize(mip_levels);

			ReturnIfFalse(_textures[0] = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_virtual_shader_resource_texture(
					image.width, 
					image.height,
					image.format,
					get_geometry_texture_name(
						_current_submaterial_index, 
						_current_image_index, 
						0, 
						model_name
					)
				)
			)));
			cache->collect(_textures[0], ResourceType::Texture);

			_textures[0]->bind_memory(_geometry_texture_heap, _current_heap_offset);
			_current_heap_offset += image.size;
			ReturnIfFalse(cmdlist->write_texture(_textures[0].get(), 1, 1, image.data.get(), image.size / image.height));


			uint2 texture_resolution = uint2(image.width, image.height);
			for (uint32_t mip_level = 1; mip_level < mip_levels; ++mip_level)
			{
				texture_resolution.x >>= 1;
				texture_resolution.y >>= 1;
				ReturnIfFalse(_textures[mip_level] = std::shared_ptr<TextureInterface>(device->create_texture(
					TextureDesc::create_virtual_shader_resource_texture(
						texture_resolution.x, 
						texture_resolution.y,
						image.format,
						get_geometry_texture_name(
							_current_submaterial_index, 
							_current_image_index, 
							mip_level, 
							model_name
						)
					)
				)));
				cache->collect(_textures[mip_level], ResourceType::Texture);

				_textures[mip_level]->bind_memory(_geometry_texture_heap, _current_heap_offset);
				_current_heap_offset += image.size >> (mip_level * 2);

				// Binding Set.
				{
					BindingSetItemArray binding_set_items(3);
					binding_set_items[0] = BindingSetItem::create_texture_srv(0, _textures[mip_level - 1]);
					binding_set_items[1] = BindingSetItem::create_texture_uav(0, _textures[mip_level]);
					binding_set_items[2] = BindingSetItem::create_sampler(0, _linear_clamp_sampler);
					ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
						BindingSetDesc{ .binding_items = binding_set_items },
						_binding_layout
					)));
				}

				uint2 thread_group_num = {
					static_cast<uint32_t>((align(texture_resolution.x, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
					static_cast<uint32_t>((align(texture_resolution.y, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
				};

				ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y));
			}

			ReturnIfFalse(cmdlist->close());
		}

		return true;
	}

	bool MipmapGenerationPass::finish_pass(RenderResourceCache* cache)
	{
		_current_image_index++;
		if (_current_image_index == Material::TextureType_Num)
		{
			_current_image_index = 0;
			_current_submaterial_index++;
			if (_current_submaterial_index == _current_model->get_component<Material>()->submaterials.size()) 
			{
				_current_submaterial_index = 0;

				// available_task_num--.
				(*_current_model->get_component<uint32_t>())--;
				
				std::string model_name = *_current_model->get_component<std::string>();
				
				_current_model = nullptr;
				LOG_INFO(model_name + "geometry texture mip generated.");
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






