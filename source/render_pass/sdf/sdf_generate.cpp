#include "sdf_generate.h"
#include "../../shader/shader_compiler.h"
#include "../../gui/gui_panel.h"
#include "../../core/math/bvh.h"
#include <cstdint>
#include <memory>
#include <string>

namespace fantasy
{
#define THREAD_GROUP_SIZE_Y 8
#define THREAD_GROUP_SIZE_Z 8
#define BVH_STACK_SIZE 32
#define UPPER_BOUND_ESTIMATE_PRESION 6
#define X_SLICE_SIZE 8

    bool SdfGeneratePass::compile(DeviceInterface* device, RenderResourceCache* cache)
    {
		cache->get_world()->get_global_entity()->get_component<event::GenerateSdf>()->add_event(
			[this](Entity* entity) 
			{ 
				recompute();
				_modle_entity = entity;
				_distance_field = entity->get_component<DistanceField>();

				ReturnIfFalse(_distance_field != nullptr);

				if (!_distance_field->check_sdf_cache_exist())
				{
					const auto& crMeshDF = _distance_field->mesh_distance_fields[0];
					std::string strSdfName = *entity->get_component<std::string>() + ".sdf";
					_binary_output = std::make_unique<serialization::BinaryOutput>(std::string(PROJ_DIR) + "asset/sdf/" + strSdfName);
				}
				return true;
			}
		);


        // Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(4);
            binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::SdfGeneratePassConstants));
			binding_layout_items[1] = BindingLayoutItem::create_structured_buffer_srv(0);
			binding_layout_items[2] = BindingLayoutItem::create_structured_buffer_srv(1);
            binding_layout_items[3] = BindingLayoutItem::create_texture_uav(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
                BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

        // Shader.
		{
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "sdf/sdf_generate_cs.slang";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("GROUP_THREAD_NUM_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_compile_desc.defines.push_back("GROUP_THREAD_NUM_Z=" + std::to_string(THREAD_GROUP_SIZE_Z));
			cs_compile_desc.defines.push_back("UPPER_BOUND_ESTIMATE_PRECISON=" + std::to_string(UPPER_BOUND_ESTIMATE_PRESION));
			cs_compile_desc.defines.push_back("BVH_STACK_SIZE=" + std::to_string(BVH_STACK_SIZE));
			ShaderData cs_data = shader_compile::compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.shader_type = ShaderType::Compute;
			cs_desc.entry = "main";
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

        return true;
    }

    bool SdfGeneratePass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
    {
		ReturnIfFalse(cmdlist->open());
		
		const auto& mesh_df = _distance_field->mesh_distance_fields[_current_mesh_sdf_index];

		if (!_resource_writed)
        {
			DeviceInterface* device = cmdlist->get_deivce();

			// Texture.
			{
				ReturnIfFalse(_sdf_output_texture = std::shared_ptr<TextureInterface>(device->create_texture(
					TextureDesc::create_read_write(
						SDF_RESOLUTION,
						SDF_RESOLUTION,
						SDF_RESOLUTION,
						Format::R32_FLOAT,
						mesh_df.sdf_texture_name
					))
				));
				cache->collect(_sdf_output_texture, ResourceType::Texture);
			}

			if (!_distance_field->check_sdf_cache_exist())
			{
				// Buffer.
				{
					ReturnIfFalse(_bvh_node_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
						BufferDesc::create_structured(
							mesh_df.bvh.GetNodes().size() * sizeof(Bvh::Node),
							sizeof(Bvh::Node),
							true
						)
					)));
					ReturnIfFalse(_bvh_vertex_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
						BufferDesc::create_structured(
							mesh_df.bvh.GetVertices().size() * sizeof(Bvh::Vertex),
							sizeof(Bvh::Vertex),
							true
						)
					)));
				}

				// Texture.
				{
					ReturnIfFalse(_read_back_texture = std::unique_ptr<StagingTextureInterface>(device->create_staging_texture(
						TextureDesc::create_read_back(
							SDF_RESOLUTION,
							SDF_RESOLUTION,
							SDF_RESOLUTION,
							Format::R32_FLOAT
						),
						CpuAccessMode::Read
					)));
				}

				// Binding Set.
				{
					BindingSetItemArray binding_set_items(4);
					binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::SdfGeneratePassConstants));
					binding_set_items[1] = BindingSetItem::create_structured_buffer_srv(0, _bvh_node_buffer);
					binding_set_items[2] = BindingSetItem::create_structured_buffer_srv(1, _bvh_vertex_buffer);
					binding_set_items[3] = BindingSetItem::create_texture_uav(0, _sdf_output_texture);
					ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
						BindingSetDesc{ .binding_items = binding_set_items },
						_binding_layout.get()
					)));
				}

				// Compute state.
				{
					_compute_state.binding_sets[0] = _binding_set.get();
				}


				_pass_constants.triangle_num = mesh_df.bvh.triangle_num;
				_pass_constants.sdf_lower = mesh_df.sdf_box._lower;
				_pass_constants.sdf_upper = mesh_df.sdf_box._upper;
				_pass_constants.sdf_extent = _pass_constants.sdf_upper - _pass_constants.sdf_lower;

				const auto& crNodes = mesh_df.bvh.GetNodes();
				const auto& crVertices = mesh_df.bvh.GetVertices();
				ReturnIfFalse(cmdlist->write_buffer(_bvh_node_buffer.get(), crNodes.data(), crNodes.size() * sizeof(Bvh::Node)));
				ReturnIfFalse(cmdlist->write_buffer(_bvh_vertex_buffer.get(), crVertices.data(), crVertices.size() * sizeof(Bvh::Vertex)));
			}

			_resource_writed = true;
        }

		if (!_distance_field->check_sdf_cache_exist())
		{
			ReturnIfFalse(cmdlist->set_compute_state(_compute_state));

			_pass_constants.x_begin = _begin_x;
			_pass_constants.x_end = _begin_x + X_SLICE_SIZE;
			ReturnIfFalse(cmdlist->set_push_constants(&_pass_constants, sizeof(constant::SdfGeneratePassConstants)));
			ReturnIfFalse(cmdlist->dispatch(
				1,
				static_cast<uint32_t>(align(SDF_RESOLUTION, static_cast<uint32_t>(THREAD_GROUP_SIZE_Y)) / THREAD_GROUP_SIZE_Y),
				static_cast<uint32_t>(align(SDF_RESOLUTION, static_cast<uint32_t>(THREAD_GROUP_SIZE_Z)) / THREAD_GROUP_SIZE_Z)
			));

			_begin_x += X_SLICE_SIZE;
			if (_begin_x == SDF_RESOLUTION)
			{
				ReturnIfFalse(cmdlist->copy_texture(
					_read_back_texture.get(), 
					TextureSlice{}, 
					_sdf_output_texture.get(), 
					TextureSlice{}
				));

				if (_current_mesh_sdf_index + 1 == static_cast<uint32_t>(_distance_field->mesh_distance_fields.size()))
				{
					ReturnIfFalse(cache->get_world()->get_global_entity()->get_component<event::UpdateGlobalSdf>()->broadcast());
				}
			}
		}
		else
		{
			uint32_t dwPixelSize = get_format_info(Format::R32_FLOAT).byte_size_per_pixel;
			ReturnIfFalse(cmdlist->write_texture(
				_sdf_output_texture.get(),
				0,
				0,
				mesh_df.sdf_data.data(),
				SDF_RESOLUTION * dwPixelSize,
				SDF_RESOLUTION * SDF_RESOLUTION* dwPixelSize
			));

			if (_current_mesh_sdf_index + 1 == static_cast<uint32_t>(_distance_field->mesh_distance_fields.size()))
			{
				ReturnIfFalse(cache->get_world()->get_global_entity()->get_component<event::UpdateGlobalSdf>()->broadcast());
			}
		}

		ReturnIfFalse(cmdlist->close());
		return true;
    }

	bool SdfGeneratePass::finish_pass()
	{
		auto& mesh_df = _distance_field->mesh_distance_fields[_current_mesh_sdf_index];

		if (!_distance_field->check_sdf_cache_exist())
		{
			if (_begin_x < SDF_RESOLUTION)
			{
				recompute();
				return true;
			}
			_begin_x = 0;

			std::vector<float> sdf_data(SDF_RESOLUTION * SDF_RESOLUTION * SDF_RESOLUTION);
			HANDLE fence_event = CreateEvent(nullptr, false, false, nullptr);

			uint64_t row_pitch = 0;
			uint64_t row_size = sizeof(float) * SDF_RESOLUTION;
			uint8_t* mapped_data = static_cast<uint8_t*>(_read_back_texture->map(TextureSlice{}, CpuAccessMode::Read, fence_event, &row_pitch));
			ReturnIfFalse(mapped_data && row_pitch == row_size);

			uint8_t* dst = reinterpret_cast<uint8_t*>(sdf_data.data());
			for (uint32_t z = 0; z < SDF_RESOLUTION; ++z)
			{
				for (uint32_t y = 0; y < SDF_RESOLUTION; ++y)
				{
					uint8_t* src = mapped_data + row_pitch * y;
					memcpy(dst, src, row_size);
					dst += row_size;
				}
				mapped_data += row_pitch * SDF_RESOLUTION;
			}

			if (_current_mesh_sdf_index == 0) (*_binary_output)(SDF_RESOLUTION);

			(*_binary_output)(
				mesh_df.sdf_box._lower.x,
				mesh_df.sdf_box._lower.y,
				mesh_df.sdf_box._lower.z,
				mesh_df.sdf_box._upper.x,
				mesh_df.sdf_box._upper.y,
				mesh_df.sdf_box._upper.z
			);
			_binary_output->save_binary_data(sdf_data.data(), sdf_data.size() * sizeof(float));

			_read_back_texture.reset();
			_bvh_node_buffer.reset();
			_bvh_vertex_buffer.reset();
			mesh_df.bvh.clear();

			if (++_current_mesh_sdf_index == static_cast<uint32_t>(_distance_field->mesh_distance_fields.size()))
			{
				std::string strSdfName = mesh_df.sdf_texture_name.substr(0, mesh_df.sdf_texture_name.find("SdfTexture")) + ".sdf";
				gui::notify_message(gui::ENotifyType::Info, strSdfName + " bake finished.");
				_distance_field = nullptr;
				_binary_output.reset();
				_current_mesh_sdf_index = 0;
			}
			else
			{
				recompute();
			}
		}
		else
		{
			if (++_current_mesh_sdf_index == static_cast<uint32_t>(_distance_field->mesh_distance_fields.size()))
			{
				std::string strSdfName = mesh_df.sdf_texture_name.substr(0, mesh_df.sdf_texture_name.find("SdfTexture")) + ".sdf";
				gui::notify_message(gui::ENotifyType::Info, strSdfName + " bake finished.");

				// finished_task_num++;
				(*_modle_entity->get_component<uint32_t>())++;
				for (auto& df : _distance_field->mesh_distance_fields) df.sdf_data.clear();

				_modle_entity = nullptr;
				_distance_field = nullptr;
				_current_mesh_sdf_index = 0;
			}
			else
			{
				recompute();
			}
		}

		_sdf_output_texture.reset();
		_resource_writed = false;
		return true;
	}

}



