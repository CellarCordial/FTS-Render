#include "mipmap_generation.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/virtual_texture.h"


namespace fantasy
{
#define THREAD_GROUP_SIZE_X 8u
#define THREAD_GROUP_SIZE_Y 8u
 
	bool MipmapGenerationPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
        cache->get_world()->get_global_entity()->get_component<event::GenerateMipmap>()->add_event(
            [this](Entity* entity) -> bool
            {
				recompute();
				_current_model = entity; 
				return true;
            }
        );

		std::fill(_current_heap_offsets.begin(), _current_heap_offsets.end(), 0);

		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(4);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::MipmapGenerationPassConstant));
			binding_layout_items[1] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_uav(0);
			binding_layout_items[3] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Texture Heap.
		{
			for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
			{
				ReturnIfFalse(_geometry_texture_heaps[ix] = std::shared_ptr<HeapInterface>(device->create_heap(
					HeapDesc{ 
						.name = get_texture_type_name(ix) + "_texture_heap",
						.type = HeapType::Default, 
						.capacity = _geometry_texture_heap_capacity 
					}
				)));
				cache->collect(_geometry_texture_heaps[ix], ResourceType::Heap);
			}
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

#if SINGLE_GEOMETRY_TEST
	bool MipmapGenerationPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		DeviceInterface* device = cmdlist->get_deivce();
		const auto& model_name = *_current_model->get_component<std::string>();
		const Mesh* mesh = _current_model->get_component<Mesh>();
		const Material* material = _current_model->get_component<Material>();

		uint32_t texture_resolution = material->image_resolution;
		uint32_t mip_levels = std::log2(texture_resolution / VT_PAGE_SIZE) + 1;

		ReturnIfFalse(cmdlist->open());

		for (uint32_t ix = 0; ix < material->submaterials.size(); ++ix)
		{
			for (uint32_t type = 0; type < Material::TextureType_Num; ++type)
			{
				const auto& image = material->submaterials[ix].images[type];

				if (image.is_valid())
				{
					ReturnIfFalse(_current_textures[type] = std::shared_ptr<TextureInterface>(device->create_texture(
						TextureDesc::create_virtual_read_write_texture(
							image.width, 
							image.height,
							image.format,
							mip_levels,
							get_geometry_texture_name(
								ix, 
								type, 
								model_name
							)
						)
					)));
					cache->collect(_current_textures[type], ResourceType::Texture);
		
					ReturnIfFalse(_current_textures[type]->bind_memory(_geometry_texture_heaps[type], _current_heap_offsets[type]));
					_current_heap_offsets[type] += _current_textures[type]->get_memory_requirements().size;
		
					ReturnIfFalse(cmdlist->write_texture(_current_textures[type].get(), 0, 0, image.data.get(), image.size));
				}
			}
			
			for (uint32_t mip = 1; mip < mip_levels; ++mip)
			{
				for (uint32_t type = 0; type < Material::TextureType_Num; ++type)
				{
					if (!_current_textures[type]) continue;
					
					// Binding Set.
					{
						auto& binding_set = _binding_sets.emplace_back();
						BindingSetItemArray binding_set_items(4);
						binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::MipmapGenerationPassConstant));
						binding_set_items[1] = BindingSetItem::create_texture_srv(0, _current_textures[type], TextureSubresourceSet{ .base_mip_level = mip - 1 });
						binding_set_items[2] = BindingSetItem::create_texture_uav(0, _current_textures[type], TextureSubresourceSet{ .base_mip_level = mip });
						binding_set_items[3] = BindingSetItem::create_sampler(0, _linear_clamp_sampler);
						ReturnIfFalse(binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
							BindingSetDesc{ .binding_items = binding_set_items },
							_binding_layout
						)));
						
						_compute_state.binding_sets[0] = binding_set.get();
					}

					auto& constant = _pass_constants.emplace_back();
					constant.input_mip_level = mip - 1;
					constant.output_resolution = texture_resolution >> mip;
	
					uint2 thread_group_num = {
						static_cast<uint32_t>((align(constant.output_resolution, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
						static_cast<uint32_t>((align(constant.output_resolution, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
					};
	
					ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &constant));
				}
			}
		}


		ReturnIfFalse(cmdlist->close());

		return true;
	}
#else
	bool MipmapGenerationPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		if (_current_model)
		{
			DeviceInterface* device = cmdlist->get_deivce();
			ReturnIfFalse(cmdlist->open());

			if (_current_mip_levels == 1)
			{
				const auto& model_name = *_current_model->get_component<std::string>();
				const Material* material = _current_model->get_component<Material>();
				_current_texture_resolution = material->image_resolution;

				for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
				{
					const auto& image = material->submaterials[_current_submaterial_index].images[ix];

					if (image.is_valid())
					{
						_current_mip_levels = std::log2(material->image_resolution / VT_PAGE_SIZE) + 1;
		
						ReturnIfFalse(_current_textures[ix] = std::shared_ptr<TextureInterface>(device->create_texture(
							TextureDesc::create_virtual_read_write_texture(
								image.width, 
								image.height,
								image.format,
								_current_mip_levels,
								get_geometry_texture_name(
									_current_submaterial_index, 
									ix, 
									model_name
								)
							)
						)));
						cache->collect(_current_textures[ix], ResourceType::Texture);
			
						ReturnIfFalse(_current_textures[ix]->bind_memory(_geometry_texture_heaps[ix], _current_heap_offsets[ix]));
						_current_heap_offsets[ix] += _current_textures[ix]->get_memory_requirements().size;
			
						ReturnIfFalse(cmdlist->write_texture(_current_textures[ix].get(), 0, 0, image.data.get(), image.size));
					}
				}
			}
			
			if (_current_mip_levels > 1)
			{
				for (uint32_t ix = 0; ix < Material::TextureType_Num; ++ix)
				{
					if (!_current_textures[ix]) continue;
					
					// Binding Set.
					{
						BindingSetItemArray binding_set_items(4);
						binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::MipmapGenerationPassConstant));
						binding_set_items[1] = BindingSetItem::create_texture_srv(0, _current_textures[ix], TextureSubresourceSet{ .base_mip_level = _current_calculate_mip - 1 });
						binding_set_items[2] = BindingSetItem::create_texture_uav(0, _current_textures[ix], TextureSubresourceSet{ .base_mip_level = _current_calculate_mip });
						binding_set_items[3] = BindingSetItem::create_sampler(0, _linear_clamp_sampler);
						ReturnIfFalse(_binding_sets[ix] = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
							BindingSetDesc{ .binding_items = binding_set_items },
							_binding_layout
						)));
					}
					_compute_state.binding_sets[0] = _binding_sets[ix].get();
					_pass_constants[ix].input_mip_level = _current_calculate_mip - 1;
					_pass_constants[ix].output_resolution = _current_texture_resolution >> _current_calculate_mip;
	
					uint2 thread_group_num = {
						static_cast<uint32_t>((align(_pass_constants[ix].output_resolution, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X)),
						static_cast<uint32_t>((align(_pass_constants[ix].output_resolution, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y)),
					};
	
					ReturnIfFalse(cmdlist->dispatch(_compute_state, thread_group_num.x, thread_group_num.y, 1, &_pass_constants[ix]));
				}
			}

			ReturnIfFalse(cmdlist->close());
		}

		return true;
	}
#endif

	bool MipmapGenerationPass::finish_pass(RenderResourceCache* cache)
	{
#if !SINGLE_GEOMETRY_TEST
		_current_calculate_mip++;
		if (_current_calculate_mip == _current_mip_levels || _current_mip_levels == 1)
		{
			_current_mip_levels = 1;
			_current_calculate_mip = 1;
			_current_submaterial_index++;
			if (_current_submaterial_index == _current_model->get_component<Material>()->submaterials.size()) 
			{
				_current_submaterial_index = 0;

				uint32_t* available_task_num = _current_model->get_component<uint32_t>();
				(*available_task_num)--;
				
				_current_model = nullptr;
			
				return true;
			}
		}
		recompute();
#endif
		return true;
	}
}






