#include "global_sdf.h"
#include "../../core/tools/check_cast.h"
#include "../../shader/shader_compiler.h"
#include <memory>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 8
#define THREAD_GROUP_SIZE_Y 8
#define THREAD_GROUP_SIZE_Z 8

	bool GlobalSdfPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		cache->get_world()->get_global_entity()->get_component<event::UpdateGlobalSdf>()->add_event(
			[this]() 
			{ 
				recompute();
				return true;
			}
		);

		ReturnIfFalse(PipelineSetup(device));
		ReturnIfFalse(ComputeStateSetup(device, cache));
		return true;
	}

	bool GlobalSdfPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		// Texture.
		{
			bool update_model_sdf_texture_resolution = (cache->get_world()->each<SceneGrid>(
				[&](Entity* entity, SceneGrid* pGrid) -> bool
				{
					for (const auto& crChunk : pGrid->chunks)
					{
						if (crChunk.model_moved)
						{
							uint32_t dwCounter = 0;
							for (const auto* cpModel : crChunk.model_entities)
							{
								DistanceField* pDF = cpModel->get_component<DistanceField>();
								for (const auto& crMeshDF : pDF->mesh_distance_fields)
								{
									const DistanceField::MeshDistanceField* pMeshSdf = &crMeshDF;
									auto iter = std::find(_mesh_sdfs.begin(), _mesh_sdfs.end(), pMeshSdf);
									if (iter == _mesh_sdfs.end())
									{
										_mesh_sdfs.emplace_back(&crMeshDF);
									}
								}
							}
						}
					}
					return true;
				}
			));
			ReturnIfFalse(update_model_sdf_texture_resolution);
		}

		// Binding Set.
		{
			_bindless_set->resize(static_cast<uint32_t>(_mesh_sdfs.size()), false);
			for (uint32_t ix = 0; ix < _mesh_sdfs.size(); ++ix)
			{
				_bindless_set->set_slot(BindingSetItem::create_texture_srv(
					ix, 
					check_cast<TextureInterface>(cache->require(_mesh_sdfs[ix]->sdf_texture_name.c_str()))
				));
			}
		}

		// Buffer
		{
			bool update_model_sdf_data_resolution = (cache->get_world()->each<SceneGrid>(
				[&](Entity* entity, SceneGrid* grid) -> bool
				{
					for (const auto& chunk : grid->chunks)
					{
						if (chunk.model_moved)
						{
							uint32_t counter = 0;
							for (const auto* model : chunk.model_entities)
							{
								DistanceField* distance_field = model->get_component<DistanceField>();
								Transform* transform = model->get_component<Transform>();

								for (const auto& crMeshDF : distance_field->mesh_distance_fields)
								{
									const DistanceField::MeshDistanceField* mesh_sdf = &crMeshDF;
									auto iter = std::find(_mesh_sdfs.begin(), _mesh_sdfs.end(), mesh_sdf);
									ReturnIfFalse(iter != _mesh_sdfs.end());

									DistanceField::TransformData data = crMeshDF.get_transformed(transform);
									_model_sdf_datas.emplace_back(
										constant::ModelSdfData{
											.coord_matrix = data.coord_matrix,
											.sdf_lower = data.sdf_box._lower,
											.sdf_upper = data.sdf_box._upper,
											.mesh_sdf_index = static_cast<uint32_t>(std::distance(_mesh_sdfs.begin(), iter))
										}
									);
								}
							}
						}
					}
					return true;
				}
			));
			ReturnIfFalse(update_model_sdf_data_resolution);

			DeviceInterface* device = cmdlist->get_deivce();

			if (_model_sdf_datas.size() > _model_sdf_data_default_count)
			{
				_model_sdf_data_buffer.reset();
				_model_sdf_data_default_count = static_cast<uint32_t>(_model_sdf_datas.size());

				ReturnIfFalse(_model_sdf_data_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
					BufferDesc::create_structured(
						_model_sdf_datas.size() * sizeof(constant::ModelSdfData),
						sizeof(constant::ModelSdfData),
						true
					)
				)));

				_dynamic_binding_set.reset();

				BindingSetItemArray dynamic_binding_set_items(1);
				dynamic_binding_set_items[0] = BindingSetItem::create_structured_buffer_srv(0, _model_sdf_data_buffer);
				ReturnIfFalse(_dynamic_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
					BindingSetDesc{ .binding_items = dynamic_binding_set_items },
					_dynamic_binding_layout.get()
				)));

				// Compute state.
				{
					_compute_state.binding_sets[1] = _dynamic_binding_set.get();
				}
			}

			ReturnIfFalse(!_model_sdf_datas.empty());
			ReturnIfFalse(cmdlist->write_buffer(
				_model_sdf_data_buffer.get(),
				_model_sdf_datas.data(),
				_model_sdf_datas.size() * sizeof(constant::ModelSdfData)
			));
		}


		uint32_t chunk_num_per_axis = GLOBAL_SDF_RESOLUTION / VOXEL_NUM_PER_CHUNK;
		float voxel_size = SCENE_GRID_SIZE / GLOBAL_SDF_RESOLUTION;
		float offset = -SCENE_GRID_SIZE * 0.5f + voxel_size * 0.5f;

		// clear Pass.
		bool global_sdf_pass_execute_result = cache->get_world()->each<SceneGrid>(
			[&](Entity* entity, SceneGrid* grid) -> bool
			{
				for (uint32_t ix = 0; ix < grid->chunks.size(); ++ix)
				{
					auto& chunk = grid->chunks[ix];
					if (chunk.model_moved && chunk.model_entities.empty())
					{
						auto& pass_constants = _pass_constants.emplace_back();
						pass_constants.voxel_offset = {
							ix % chunk_num_per_axis,
							(ix / chunk_num_per_axis) % chunk_num_per_axis,
							ix / (chunk_num_per_axis * chunk_num_per_axis)
						};
						pass_constants.voxel_offset = (pass_constants.voxel_offset) * VOXEL_NUM_PER_CHUNK;

						ReturnIfFalse(cmdlist->set_compute_state(_clear_pass_compute_state));
						ReturnIfFalse(cmdlist->set_push_constants(&pass_constants, sizeof(constant::GlobalSdfConstants)));

						Vector3I thread_group_num = {
							Align(VOXEL_NUM_PER_CHUNK, static_cast<uint32_t>(THREAD_GROUP_SIZE_X)) / THREAD_GROUP_SIZE_X,
							Align(VOXEL_NUM_PER_CHUNK, static_cast<uint32_t>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y,
							Align(VOXEL_NUM_PER_CHUNK, static_cast<uint32_t>(THREAD_GROUP_SIZE_Z)) / THREAD_GROUP_SIZE_Z
						};

						ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y, thread_group_num.z));
						chunk.model_moved = false;
					}
				}
				return true;
			}
		);
		ReturnIfFalse(global_sdf_pass_execute_result);

		uint32_t mesh_index = 0;
		global_sdf_pass_execute_result = cache->get_world()->each<SceneGrid>(
			[&](Entity* entity, SceneGrid* grid) -> bool
			{
				for (uint32_t ix = 0; ix < grid->chunks.size(); ++ix)
				{
					auto& chunk = grid->chunks[ix];
					if (chunk.model_moved)
					{
						uint32_t mesh_index_begin = mesh_index;
						for (const auto* model : chunk.model_entities)
						{
							mesh_index += static_cast<uint32_t>(model->get_component<DistanceField>()->mesh_distance_fields.size());
						}

						auto& pass_constants = _pass_constants.emplace_back();
						pass_constants.gi_max_distance = SCENE_GRID_SIZE;
						pass_constants.mesh_sdf_begin = mesh_index_begin;
						pass_constants.mesh_sdf_end = mesh_index;

						pass_constants.voxel_offset = {
							ix % chunk_num_per_axis,
							(ix / chunk_num_per_axis) % chunk_num_per_axis,
							ix / (chunk_num_per_axis * chunk_num_per_axis)
						};
						pass_constants.voxel_offset = (pass_constants.voxel_offset) * VOXEL_NUM_PER_CHUNK;

						pass_constants.voxel_world_matrix = {
							voxel_size, 0.0f,		0.0f,		0.0f,
							0.0f,		voxel_size, 0.0f,		0.0f,
							0.0f,		0.0f,		voxel_size, 0.0f,
							offset,	offset,	offset,	1.0f
						};

						ReturnIfFalse(cmdlist->set_compute_state(_compute_state));
						ReturnIfFalse(cmdlist->set_push_constants(&pass_constants, sizeof(constant::GlobalSdfConstants)));

						Vector3I thread_group_num = {
							Align(VOXEL_NUM_PER_CHUNK, static_cast<uint32_t>(THREAD_GROUP_SIZE_X)) / THREAD_GROUP_SIZE_X,
							Align(VOXEL_NUM_PER_CHUNK, static_cast<uint32_t>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y,
							Align(VOXEL_NUM_PER_CHUNK, static_cast<uint32_t>(THREAD_GROUP_SIZE_Z)) / THREAD_GROUP_SIZE_Z
						};

						ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y, thread_group_num.z));

						chunk.model_moved = false;
					}
				}
				return true;
			}
		);

		ReturnIfFalse(global_sdf_pass_execute_result);
		ReturnIfFalse(cmdlist->close());

		return true;
	}

	bool GlobalSdfPass::finish_pass()
	{
		_mesh_sdfs.clear(); _mesh_sdfs.shrink_to_fit();
		_model_sdf_datas.clear(); _model_sdf_datas.shrink_to_fit();
		_pass_constants.clear(); _pass_constants.shrink_to_fit();
		return true;
	}


	bool GlobalSdfPass::PipelineSetup(DeviceInterface* device)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(3);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::GlobalSdfConstants));
			binding_layout_items[1] = BindingLayoutItem::create_sampler(0);
			binding_layout_items[2] = BindingLayoutItem::create_texture_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));

			BindingLayoutItemArray dynamic_binding_layout_items(1);
			dynamic_binding_layout_items[0] = BindingLayoutItem::create_structured_buffer_srv(0);
			ReturnIfFalse(_dynamic_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = dynamic_binding_layout_items }
			)));

			BindingLayoutItemArray bindless_layout_items(1);
			bindless_layout_items[0] = BindingLayoutItem::create_bindless_srv();
			ReturnIfFalse(_bindingless_layout = std::unique_ptr<BindingLayoutInterface>(device->create_bindless_layout(
				BindlessLayoutDesc{
					.binding_layout_items = bindless_layout_items,
					.first_slot = 1
				}
			)));

			BindingLayoutItemArray clear_binding_layout_items(2);
			clear_binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::GlobalSdfConstants));
			clear_binding_layout_items[1] = BindingLayoutItem::create_texture_uav(0);
			ReturnIfFalse(clear_pass_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = clear_binding_layout_items }
			)));
		}

		// Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "sdf/sdf_merge_cs.slang";
			cs_compile_desc.entry_point = "compute_shader";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Z=" + std::to_string(THREAD_GROUP_SIZE_Z));
			ShaderData cs_data = shader_compile::compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.shader_type = ShaderType::Compute;
			cs_desc.entry = "compute_shader";
			ReturnIfFalse(_cs = std::unique_ptr<Shader>(create_shader(cs_desc, cs_data.data(), cs_data.size())));


			cs_compile_desc.shader_name = "sdf/sdf_clear_cs.slang";
			ShaderData clear_pass_cs_data = shader_compile::compile_shader(cs_compile_desc);
			ReturnIfFalse(_clear_pass_cs = std::unique_ptr<Shader>(create_shader(cs_desc, clear_pass_cs_data.data(), clear_pass_cs_data.size())));
		}

		// Pipeline.
		{
			ComputePipelineDesc pipeline_desc;
			pipeline_desc.compute_shader = _cs.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			pipeline_desc.binding_layouts.push_back(_dynamic_binding_layout.get());
			pipeline_desc.binding_layouts.push_back(_bindingless_layout.get());
			ReturnIfFalse(_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(pipeline_desc)));

			ComputePipelineDesc clear_pass_pipeline_desc;
			clear_pass_pipeline_desc.compute_shader = _clear_pass_cs.get();
			clear_pass_pipeline_desc.binding_layouts.push_back(clear_pass_binding_layout.get());
			ReturnIfFalse(_clear_pass_pipeline = std::unique_ptr<ComputePipelineInterface>(device->create_compute_pipeline(clear_pass_pipeline_desc)));
		}

		return true;
	}

	bool GlobalSdfPass::ComputeStateSetup(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Buffer.
		{
			ReturnIfFalse(_model_sdf_data_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_structured(
					_model_sdf_data_default_count * sizeof(constant::ModelSdfData),
					sizeof(constant::ModelSdfData),
					true
				)
			)));
		}

		// Texture.
		{
			ReturnIfFalse(_global_sdf_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_read_write(
					GLOBAL_SDF_RESOLUTION,
					GLOBAL_SDF_RESOLUTION,
					GLOBAL_SDF_RESOLUTION,
					Format::R32_FLOAT,
					"GlobalSdfTexture"
				)
			)));
			cache->collect(_global_sdf_texture, ResourceType::Texture);
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(3);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::GlobalSdfConstants));
			binding_set_items[1] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("LinearClampSampler")));
			binding_set_items[2] = BindingSetItem::create_texture_uav(0, _global_sdf_texture);
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout.get()
			)));

			BindingSetItemArray dynamic_binding_set_items(1);
			dynamic_binding_set_items[0] = BindingSetItem::create_structured_buffer_srv(0, _model_sdf_data_buffer);
			ReturnIfFalse(_dynamic_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = dynamic_binding_set_items },
				_dynamic_binding_layout.get()
			)));

			ReturnIfFalse(_bindless_set = std::unique_ptr<BindlessSetInterface>(device->create_bindless_set(_bindingless_layout.get())));


			BindingSetItemArray clear_pass_binding_set_items(2);
			clear_pass_binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::GlobalSdfConstants));
			clear_pass_binding_set_items[1] = BindingSetItem::create_texture_uav(0, _global_sdf_texture);
			ReturnIfFalse(_clear_pass_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = clear_pass_binding_set_items },
				clear_pass_binding_layout.get()
			)));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.get());
			_compute_state.binding_sets.push_back(_dynamic_binding_set.get());
			_compute_state.binding_sets.push_back(_bindless_set.get());
			_compute_state.pipeline = _pipeline.get();

			_clear_pass_compute_state.binding_sets.push_back(_clear_pass_binding_set.get());
			_clear_pass_compute_state.pipeline = _clear_pass_pipeline.get();
		}

		return true;
	}

}

