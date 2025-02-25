#include "test_atmosphere.h"
#include "../shader/shader_compiler.h"
#include "../core/tools/check_cast.h"
#include "../gui/gui_panel.h"
#include "../scene/camera.h"
#include "../scene/light.h"
#include <memory>

namespace fantasy
{
	bool AtmosphereDebugPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(10);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::AtmosphereDebugPassConstant0));
			binding_layout_items[1] = BindingLayoutItem::create_constant_buffer(1);
			binding_layout_items[2] = BindingLayoutItem::create_constant_buffer(2);
			binding_layout_items[3] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[4] = BindingLayoutItem::create_texture_srv(1);
			binding_layout_items[5] = BindingLayoutItem::create_texture_srv(2);
			binding_layout_items[6] = BindingLayoutItem::create_texture_srv(3);
			binding_layout_items[7] = BindingLayoutItem::create_sampler(0);
			binding_layout_items[8] = BindingLayoutItem::create_sampler(1);
			binding_layout_items[9] = BindingLayoutItem::create_sampler(2);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Input Layout.
		{
			VertexAttributeDescArray vertex_attribute_descs(4);
			vertex_attribute_descs[0].name = "POSITION";
			vertex_attribute_descs[0].format = Format::RGB32_FLOAT;
			vertex_attribute_descs[0].offset = offsetof(Vertex, position);
			vertex_attribute_descs[0].element_stride = sizeof(Vertex);
			vertex_attribute_descs[1].name = "NORMAL";
			vertex_attribute_descs[1].format = Format::RGB32_FLOAT;
			vertex_attribute_descs[1].offset = offsetof(Vertex, normal);
			vertex_attribute_descs[1].element_stride = sizeof(Vertex);
			vertex_attribute_descs[2].name = "TANGENT";
			vertex_attribute_descs[2].format = Format::RGB32_FLOAT;
			vertex_attribute_descs[2].offset = offsetof(Vertex, tangent);
			vertex_attribute_descs[2].element_stride = sizeof(Vertex);
			vertex_attribute_descs[3].name = "TEXCOORD";
			vertex_attribute_descs[3].format = Format::RG32_FLOAT;
			vertex_attribute_descs[3].offset = offsetof(Vertex, uv);
			vertex_attribute_descs[3].element_stride = sizeof(Vertex);
			ReturnIfFalse(_input_layout = std::unique_ptr<InputLayoutInterface>(device->create_input_layout(
				vertex_attribute_descs.data(), 
				vertex_attribute_descs.size()
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "test/atmosphere_test_vs.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = compile_shader(shader_compile_desc);
			shader_compile_desc.shader_name = "test/atmosphere_test_ps.hlsl";
			shader_compile_desc.entry_point = "main";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "main";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));
			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "main";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}

		// Frame Buffer.
		{
			FrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.color_attachments.push_back(FrameBufferAttachment::create_attachment(check_cast<TextureInterface>(cache->require("final_texture"))));
			FrameBufferDesc.depth_stencil_attachment = FrameBufferAttachment::create_attachment(check_cast<TextureInterface>(cache->require("depth_texture")));
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(FrameBufferDesc)));
		}

		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs;
			pipeline_desc.pixel_shader = _ps;
			pipeline_desc.input_layout = _input_layout;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			pipeline_desc.render_state.depth_stencil_state.enable_depth_test = true;
			pipeline_desc.render_state.depth_stencil_state.enable_depth_write = true;
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(device->create_graphics_pipeline(
				pipeline_desc,
				_frame_buffer.get()
			)));
		}

		// Buffer.
		{
			ReturnIfFalse(_pass_constant1_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_constant_buffer(sizeof(constant::AtmosphereDebugPassConstant1))
			)));
		}

		// Texture.
		{
			std::string image_path = std::string(PROJ_DIR) + "asset/image/BlueNoise.png";
			_blue_noise_image = Image::load_image_from_file(image_path.c_str());
			ReturnIfFalse(_blue_noise_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_shader_resource_texture(
					_blue_noise_image.width,
					_blue_noise_image.height,
					_blue_noise_image.format,
					"blue_noise_texture"
				)
			)));
		}

		// Binding Set.
		{
			_transmittance_texture = check_cast<TextureInterface>(cache->require("transmittance_texture"));
			_aerial_lut_texture = check_cast<TextureInterface>(cache->require("aerial_lut_texture"));
			_multi_scattering_texture = check_cast<TextureInterface>(cache->require("multi_scattering_texture"));
			_final_texture = check_cast<TextureInterface>(cache->require("final_texture"));

			BindingSetItemArray binding_set_items(10);
			binding_set_items[0] = BindingSetItem::create_push_constants(0, sizeof(constant::AtmosphereDebugPassConstant0));
			binding_set_items[1] = BindingSetItem::create_constant_buffer(1, check_cast<BufferInterface>(cache->require("atmosphere_properties_buffer")));
			binding_set_items[2] = BindingSetItem::create_constant_buffer(2, _pass_constant1_buffer);
			binding_set_items[3] = BindingSetItem::create_texture_srv(0, _transmittance_texture);
			binding_set_items[4] = BindingSetItem::create_texture_srv(1, _aerial_lut_texture);
			binding_set_items[5] = BindingSetItem::create_texture_srv(2, check_cast<TextureInterface>(cache->require("shadow_map_texture")), TextureSubresourceSet{}, Format::R32_FLOAT);
			binding_set_items[6] = BindingSetItem::create_texture_srv(3, _blue_noise_texture);
			binding_set_items[7] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("linear_clamp_sampler")));
			binding_set_items[8] = BindingSetItem::create_sampler(1, check_cast<SamplerInterface>(cache->require("point_clamp_sampler")));
			binding_set_items[9] = BindingSetItem::create_sampler(2, check_cast<SamplerInterface>(cache->require("point_wrap_sampler")));
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout
			)));
		}

		// Graphics state.
		{
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.binding_sets.push_back(_binding_set.get());
			_graphics_state.vertex_buffer_bindings.push_back(VertexBufferBinding{ .buffer = check_cast<BufferInterface>(cache->require("GeometryVertexBuffer")) });
			_graphics_state.index_buffer_binding = IndexBufferBinding{ 
				.buffer = check_cast<BufferInterface>(cache->require("GeometryIndexBuffer")), 
				.format = Format::R32_UINT 
			};
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		ReturnIfFalse(cache->require_constants("GeometryDrawArguments", reinterpret_cast<void**>(&_draw_arguments), &_draw_argument_count));


		return true;
	}

	bool AtmosphereDebugPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		// Update constant.
		{
			float* pworld_scale;
			float3* pGroundAlbedo;
			ReturnIfFalse(cache->require_constants("world_scale", reinterpret_cast<void**>(&pworld_scale)));
			ReturnIfFalse(cache->require_constants("ground_albedo", reinterpret_cast<void**>(&pGroundAlbedo)));
			_pass_constant1.world_scale = *pworld_scale;
			_pass_constant1.ground_albedo = *pGroundAlbedo;

			TextureDesc aerial_lut_desc = _aerial_lut_texture->get_desc();
			_pass_constant1.jitter_factor = {
				_jitter_radius / aerial_lut_desc.width,
				_jitter_radius / aerial_lut_desc.height
			};
			_pass_constant1.blue_noise_uv_factor = {
				(1.0f * CLIENT_WIDTH) / _blue_noise_image.width,
				(1.0f * CLIENT_HEIGHT) / _blue_noise_image.height
			};

			ReturnIfFalse(cache->get_world()->each<DirectionalLight>(
				[this](Entity* entity, DirectionalLight* pLight) -> bool
				{
					_pass_constant1.sun_direction = pLight->direction;
					_pass_constant1.sun_theta = std::asin(-pLight->direction.y);
					_pass_constant1.sun_radiance = float3(pLight->intensity * pLight->color);
					_pass_constant1.shadow_view_proj = pLight->get_view_proj();
					return true;
				}
			));


			ReturnIfFalse(cache->get_world()->each<Camera>(
				[this](Entity* entity, Camera* _camera) -> bool
				{
					_pass_constant0.view_proj = _camera->get_view_proj();
					_pass_constant1.camera_position = _camera->position;
					return true;
				}
			));

			void* mapped_address = _pass_constant1_buffer->map(CpuAccessMode::Write);
			memcpy(mapped_address, &_pass_constant1, sizeof(constant::AtmosphereDebugPassConstant1));
			_pass_constant1_buffer->unmap();

			uint64_t stSubmeshIndex = 0;
			ReturnIfFalse(cache->get_world()->each<Mesh>(
				[this, cmdlist, &stSubmeshIndex](Entity* entity, Mesh* pMesh) -> bool
				{
					for (uint64_t ix = 0; ix < pMesh->submeshes.size(); ++ix)
					{
						_pass_constant0.world_matrix = pMesh->submeshes[ix].world_matrix;

						ReturnIfFalse(cmdlist->draw_indexed(_graphics_state, _draw_arguments[stSubmeshIndex++], &_pass_constant0));
					}
					return true;
				}
			));
			ReturnIfFalse(stSubmeshIndex == _draw_argument_count);
		}

		if (!_writed_resource)
		{
			ReturnIfFalse(cmdlist->write_texture(_blue_noise_texture.get(), 0, 0, _blue_noise_image.data.get(), _blue_noise_image.size / _blue_noise_image.height));
			_writed_resource = true;
		}

		cmdlist->set_texture_state(_aerial_lut_texture.get(), TextureSubresourceSet{}, ResourceStates::UnorderedAccess);

		ReturnIfFalse(cmdlist->close());
		return true;
	}


	RenderPassInterface* AtmosphereTest::init_render_pass(RenderGraph* render_graph)
	{
		_transmittance_lut_pass = std::make_shared<TransmittanceLUTPass>();
		_multi_scattering_lut_pass = std::make_shared<MultiScatteringLUTPass>();
		_shadow_map_pass = std::make_shared<ShadowMapPass>();
		_sky_lut_pass = std::make_shared<SkyLUTPass>();
		_aerial_lut_pass = std::make_shared<AerialLUTPass>();
		_sky_pass = std::make_shared<SkyPass>();
		_sun_disk_pass = std::make_shared<SunDiskPass>();
		_atmosphere_debug_pass = std::make_shared<AtmosphereDebugPass>();

		render_graph->add_pass(_transmittance_lut_pass);
		render_graph->add_pass(_multi_scattering_lut_pass);
		render_graph->add_pass(_shadow_map_pass);
		render_graph->add_pass(_sky_lut_pass);
		render_graph->add_pass(_aerial_lut_pass);
		render_graph->add_pass(_sky_pass);
		render_graph->add_pass(_sun_disk_pass);
		render_graph->add_pass(_atmosphere_debug_pass);

		_transmittance_lut_pass->precede(_multi_scattering_lut_pass.get());


		_shadow_map_pass->precede(_aerial_lut_pass.get());
		_sky_lut_pass->precede(_aerial_lut_pass.get());
		_sky_lut_pass->precede(_sky_pass.get());
		_aerial_lut_pass->precede(_sun_disk_pass.get());
		_sky_pass->precede(_sun_disk_pass.get());
		_sun_disk_pass->precede(_atmosphere_debug_pass.get());


		RenderResourceCache* cache = render_graph->get_resource_cache();
		cache->collect_constants("world_scale", &_world_scale);
		cache->collect_constants("ground_albedo", &_ground_albedo);


		World* world = cache->get_world();
		Entity* entity = world->create_entity();
		constant::AtmosphereProperties* properties = entity->assign<constant::AtmosphereProperties>();


		if (
			!world->broadcast(event::OnModelLoad{ 
				.entity = world->create_entity(),
				.model_path = "asset/model/Mountain/terrain.gltf" 
			})
		)
			return nullptr;


		DirectionalLight light;
		light.update_direction_view_proj();

		Entity* light_entity = world->create_entity();
		DirectionalLight* light_ptr = light_entity->assign<DirectionalLight>(light);


		gui::add(
			[this, properties, light_ptr]()
			{
				if (ImGui::CollapsingHeader("Atmosphere Test"))
				{
					bool dirty = false;
					if (ImGui::TreeNode("Atmosphere Properties"))
					{
						dirty |= ImGui::SliderFloat("Planet Radius         (km)   ", &properties->planet_radius, 0.0f, 10000.0f);
						dirty |= ImGui::SliderFloat("Atmosphere Radius     (km)   ", &properties->atmosphere_radius, 0.0f, 10000.0f);
						dirty |= ImGui::SliderFloat3("Rayleight Scattering  (um^-1)", &properties->raylegh_scatter.x, 0.0f, 100.0f);
						dirty |= ImGui::SliderFloat("Rayleight Density H   (km)   ", &properties->raylegh_density, 0.0f, 30.0f);
						dirty |= ImGui::SliderFloat("Mie Scatter           (um^-1)", &properties->mie_scatter, 0.0f, 10.0f);
						dirty |= ImGui::SliderFloat("Mie Absorb            (um^-1)", &properties->mie_absorb, 0.0f, 10.0f);
						dirty |= ImGui::SliderFloat("Mie Density           (km)   ", &properties->mie_density, 0.0f, 10.0f);
						dirty |= ImGui::SliderFloat("Mie Scatter Asymmetry        ", &properties->mie_asymmetry, 0.0f, 1.0f);
						dirty |= ImGui::SliderFloat3("Ozone Absorb          (um^-1)", &properties->ozone_absorb.x, 0.0f, 5.0f);
						dirty |= ImGui::SliderFloat("Ozone Center height   (km)   ", &properties->ozone_center_height, 0.0f, 100.0f);
						dirty |= ImGui::SliderFloat("Ozone Thickness       (km)   ", &properties->ozone_thickness, 0.0f, 100.0f);

						if (ImGui::Button("reset"))
						{
							dirty = true;
							*properties = constant::AtmosphereProperties{};
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Multi Scatter Pass"))
					{
						dirty |= ImGui::SliderInt("Ray March Step Count", &_multi_scattering_lut_pass->_pass_constants.ray_march_step_count, 10, 500);
						dirty |= ImGui::ColorEdit3("Ground Albedo", &_ground_albedo.x);

						if (ImGui::Button("reset"))
						{
							dirty = true;
							_multi_scattering_lut_pass->_pass_constants.ray_march_step_count = 256;
							_ground_albedo = { 0.3f, 0.3f, 0.3f };
						}

						ImGui::TreePop();
					}

					static bool enable_multi_scattering = true;
					static bool enable_shadow = true;

					if (ImGui::TreeNode("Sky LUT Pass"))
					{
						ImGui::SliderInt("Ray March Step Count", &_sky_lut_pass->_pass_constant.march_step_count, 10, 100);

						if (ImGui::Button("reset"))
						{
							_sky_lut_pass->_pass_constant.march_step_count = 40;
						}

						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Aerial LUT Pass"))
					{
						ImGui::SliderFloat("max Aerial distance", &_aerial_lut_pass->_pass_constant.max_aerial_distance, 100.0f, 5000.0f);
						ImGui::SliderInt("Per Slice March Step Count", &_aerial_lut_pass->_pass_constant.per_slice_march_step_count, 1, 100);

						_atmosphere_debug_pass->_pass_constant1.max_aerial_distance = _aerial_lut_pass->_pass_constant.max_aerial_distance;

						if (ImGui::Button("reset"))
						{
							_aerial_lut_pass->_pass_constant.max_aerial_distance = 2000.0f;
							_aerial_lut_pass->_pass_constant.per_slice_march_step_count = 1;
						}
						ImGui::TreePop();
					}


					if (ImGui::TreeNode("Sun"))
					{
						dirty |= ImGui::SliderFloat("Intensity", &light_ptr->intensity, 0.0f, 20.0f);
						ImGui::ColorEdit3("Color", &light_ptr->color.x);

						bool angle_changed = false;
						if (ImGui::SliderFloat("angle Vert", &light_ptr->angle.x, 0.0f, 360.0f)) angle_changed = true;
						if (ImGui::SliderFloat("angle Horz", &light_ptr->angle.y, 0.0f, 180.0f)) angle_changed = true;

						if (angle_changed)
						{
							light_ptr->update_direction_view_proj();
						}

						if (ImGui::Button("reset"))
						{
							dirty = true;

							*light_ptr = DirectionalLight{};
							light_ptr->update_direction_view_proj();
						}

						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Misc"))
					{
						ImGui::SliderFloat("World scale", &_world_scale, 10.0f, 500.0f);
						ImGui::Checkbox("Enable Shadow", &enable_shadow);
						ImGui::Checkbox("Enable Multi Scattering", &enable_multi_scattering);
						_aerial_lut_pass->_pass_constant.enable_shadow = static_cast<uint32_t>(enable_shadow);
						_aerial_lut_pass->_pass_constant.enable_multi_scattering = static_cast<uint32_t>(enable_multi_scattering);
						_sky_lut_pass->_pass_constant.enable_multi_scattering = static_cast<uint32_t>(enable_multi_scattering);

						if (ImGui::Button("reset"))
						{
							_world_scale = 200.0f;
							enable_shadow = true;
							enable_multi_scattering = true;
						}
						ImGui::TreePop();
					}

					if (dirty)
					{
						_transmittance_lut_pass->recompute();
						_multi_scattering_lut_pass->recompute();
					}
				}
			}
		);
		return _atmosphere_debug_pass.get();
	}

}


// int main()
// {
// 	fantasy::AtmosphereTest test(fantasy::GraphicsAPI::D3D12);
// 	return test.initialize() && test.run() ? 1 : 0;
// }