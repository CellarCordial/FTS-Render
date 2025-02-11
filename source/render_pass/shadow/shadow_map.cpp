#include "shadow_map.h"
#include "../../scene/light.h"
#include "../../shader/shader_compiler.h"
#include <memory>

namespace fantasy
{
#define SHADOW_MAP_RESOLUTION 2048

	bool ShadowMapPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(1);
			binding_layout_items[0] = BindingLayoutItem::create_push_constants(0, sizeof(constant::ShadowMapPassConstant));
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Input Layout.
		{
			VertexAttributeDescArray vertex_attributes(4);
			vertex_attributes[0].name = "POSITION";
			vertex_attributes[0].format = Format::RGB32_FLOAT;
			vertex_attributes[0].offset = offsetof(Vertex, position);
			vertex_attributes[0].element_stride = sizeof(Vertex);
			vertex_attributes[1].name = "NORMAL";
			vertex_attributes[1].format = Format::RGB32_FLOAT;
			vertex_attributes[1].offset = offsetof(Vertex, normal);
			vertex_attributes[1].element_stride = sizeof(Vertex);
			vertex_attributes[2].name = "TANGENT";
			vertex_attributes[2].format = Format::RGB32_FLOAT;
			vertex_attributes[2].offset = offsetof(Vertex, tangent);
			vertex_attributes[2].element_stride = sizeof(Vertex);
			vertex_attributes[3].name = "TEXCOORD";
			vertex_attributes[3].format = Format::RG32_FLOAT;
			vertex_attributes[3].offset = offsetof(Vertex, uv);
			vertex_attributes[3].element_stride = sizeof(Vertex);
			ReturnIfFalse(_input_layout = std::unique_ptr<InputLayoutInterface>(device->create_input_layout(
				vertex_attributes.data(), 
				vertex_attributes.size()
			)));
		}

		// Shader.
		{
			ShaderCompileDesc vs_compile_desc;
			vs_compile_desc.shader_name = "shadow/shadow_map_vs.slang";
			vs_compile_desc.entry_point = "main";
			vs_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = shader_compile::compile_shader(vs_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "main";
			vs_desc.shader_type = ShaderType::Vertex;
			_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size()));
		}

		// Load vertices.
		{
			ReturnIfFalse(cache->get_world()->each<Mesh>(
				[this](Entity* entity, Mesh* mesh) -> bool
				{
					uint64_t old_size = _draw_arguments.size();
					_draw_arguments.resize(old_size + mesh->submeshes.size());
					_draw_arguments[old_size].index_count = static_cast<uint32_t>(mesh->submeshes[0].indices.size());

					if (old_size > 0)
					{
						_draw_arguments[old_size].start_index_location = _indices.size();
						_draw_arguments[old_size].start_vertex_location = _vertices.size();
					}

					for (uint64_t ix = old_size; ix < mesh->submeshes.size(); ++ix)
					{
						const auto& submesh = mesh->submeshes[ix];
						_vertices.insert(_vertices.end(), submesh.vertices.begin(), submesh.vertices.end());
						_indices.insert(_indices.end(), submesh.indices.begin(), submesh.indices.end());

						if (ix != 0)
						{
							_draw_arguments[ix].index_count = submesh.indices.size();
							_draw_arguments[ix].start_index_location = _draw_arguments[ix - 1].start_index_location + mesh->submeshes[ix - 1].indices.size();
							_draw_arguments[ix].start_vertex_location = _draw_arguments[ix - 1].start_vertex_location + mesh->submeshes[ix - 1].vertices.size();
						}
					}

					return true;
				}
			));
		}

		// Buffer.
		{
			ReturnIfFalse(_vertex_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_vertex_buffer(sizeof(Vertex) * _vertices.size(), "GeometryVertexBuffer")
			)));

			ReturnIfFalse(_index_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_index_buffer(sizeof(uint32_t) * _indices.size(), "GeometryIndexBuffer")
			)));

			cache->collect(_vertex_buffer, ResourceType::Buffer);
			cache->collect(_index_buffer, ResourceType::Buffer);
		}

		// Texture.
		{
			ReturnIfFalse(_shadow_map_texture = std::shared_ptr<TextureInterface>(device->create_texture(
				TextureDesc::create_depth_stencil_texture(CLIENT_WIDTH, CLIENT_HEIGHT, Format::D32, "shadow_map_texture")
			)));
			cache->collect(_shadow_map_texture, ResourceType::Texture);
		}

		// Frame Buffer.
		{
			FrameBufferDesc FrameBufferDesc;
			FrameBufferDesc.depth_stencil_attachment = FrameBufferAttachment::create_attachment(_shadow_map_texture);
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(FrameBufferDesc)));
		}

		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.vertex_shader = _vs;
			pipeline_desc.input_layout = _input_layout;
			pipeline_desc.binding_layouts.push_back(_binding_layout);
			pipeline_desc.render_state.depth_stencil_state.enable_depth_test = true;
			pipeline_desc.render_state.depth_stencil_state.enable_depth_write = true;
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())));
		}

		// Graphics state.
		{
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.vertex_buffer_bindings.push_back(VertexBufferBinding{ .buffer = _vertex_buffer});
			_graphics_state.index_buffer_binding = IndexBufferBinding{ .buffer = _index_buffer, .format = Format::R32_UINT };
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		ReturnIfFalse(cache->collect_constants("GeometryDrawArguments", _draw_arguments.data(), _draw_arguments.size()));

		return true;
	}

	bool ShadowMapPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		clear_depth_stencil_attachment(cmdlist, _frame_buffer.get());

		if (!_resource_writed)
		{
			ReturnIfFalse(cmdlist->write_buffer(_vertex_buffer.get(), _vertices.data(), _vertices.size() * sizeof(Vertex)));
			ReturnIfFalse(cmdlist->write_buffer(_index_buffer.get(), _indices.data(), _indices.size() * sizeof(uint32_t)));
			_resource_writed = true;
		}

		// Update constant.
		{
			ReturnIfFalse(cache->get_world()->each<DirectionalLight>(
				[this](Entity* entity, DirectionalLight* pLight) -> bool
				{
					_pass_constant.directional_light_view_proj = pLight->view_proj;
					return true;
				}
			));
		}

		uint64_t submesh_index = 0;
		ReturnIfFalse(cache->get_world()->each<Mesh>(
			[this, cmdlist, &submesh_index](Entity* entity, Mesh* mesh) -> bool
			{
				for (uint64_t ix = 0; ix < mesh->submeshes.size(); ++ix)
				{
					_pass_constant.world_matrix = mesh->submeshes[ix].world_matrix;

					ReturnIfFalse(cmdlist->draw_indexed(_graphics_state, _draw_arguments[submesh_index++], &_pass_constant));
				}
				return true;
			}
		));
		ReturnIfFalse(submesh_index == _draw_arguments.size());

		ReturnIfFalse(cmdlist->close());

		return true;
	}

	bool ShadowMapPass::finish_pass()
	{
		_vertices.clear();
		_indices.clear();
		return true;
	}

}