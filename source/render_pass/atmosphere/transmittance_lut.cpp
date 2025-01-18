#include "transmittance_lut.h"
#include "../../shader/shader_compiler.h"
#include "../../core/math/vector.h"
#include <memory>
#include <string>

namespace fantasy
{
#define THREAD_GROUP_SIZE_X 16 
#define THREAD_GROUP_SIZE_Y 16 
#define RAY_STEP_COUNT 1000
#define TRANSMITTANCE_LUT_RES 256

    bool TransmittanceLUTPass::compile(DeviceInterface* device, RenderResourceCache* cache)
    {
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(2);
			binding_layout_items[0] = BindingLayoutItem::create_constant_buffer(0, false);
			binding_layout_items[1] = BindingLayoutItem::create_texture_uav(0);

			_binding_layout = std::unique_ptr<BindingLayoutInterface>(
				device->create_binding_layout(BindingLayoutDesc{ .binding_layout_items = binding_layout_items })
			);
			ReturnIfFalse(_binding_layout != nullptr);
		}

        // Shader.
        {
			ShaderCompileDesc cs_compile_desc;
			cs_compile_desc.shader_name = "atmosphere/transmittance_lut_cs.slang";
			cs_compile_desc.entry_point = "main";
			cs_compile_desc.target = ShaderTarget::Compute;
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_X=" + std::to_string(THREAD_GROUP_SIZE_X));
			cs_compile_desc.defines.push_back("THREAD_GROUP_SIZE_Y=" + std::to_string(THREAD_GROUP_SIZE_Y));
			cs_compile_desc.defines.push_back("STEP_COUNT=" + std::to_string(RAY_STEP_COUNT));
			ShaderData cs_data = shader_compile::compile_shader(cs_compile_desc);

			ShaderDesc cs_desc;
			cs_desc.entry = "main";
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

		// Buffer.
		{
			ReturnIfFalse(_atomsphere_properties_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_constant(
					sizeof(constant::AtmosphereProperties), 
					false, 
					"AtmospherePropertiesBuffer"
				)
			)));
			cache->collect(_atomsphere_properties_buffer, ResourceType::Buffer);
		}
        
        // Texture.
		{
			TextureDesc texture_desc = TextureDesc::create_read_write(
				TRANSMITTANCE_LUT_RES,
				TRANSMITTANCE_LUT_RES,
				Format::RGBA32_FLOAT,
				"TransmittanceTexture"
			);
			ReturnIfFalse(_transmittance_texture = std::shared_ptr<TextureInterface>(device->create_texture(texture_desc)));
			cache->collect(_transmittance_texture, ResourceType::Texture);
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(2);
			binding_set_items[0] = BindingSetItem::create_constant_buffer(0, _atomsphere_properties_buffer);
			binding_set_items[1] = BindingSetItem::create_texture_uav(0, _transmittance_texture);
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout.get()
			)));
		}

		// Compute state.
		{
			_compute_state.binding_sets.push_back(_binding_set.get());
			_compute_state.pipeline = _pipeline.get();
		}

        return true;
    }

    bool TransmittanceLUTPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
    {
		ReturnIfFalse(cmdlist->open());

		// Update constant.
		{
			ReturnIfFalse(cache->get_world()->each<constant::AtmosphereProperties>(
				[this](Entity* entity, constant::AtmosphereProperties* pProperties) -> bool
				{
					_standard_atomsphere_properties = pProperties->to_standard_unit();
					return true;
				}
			));
			ReturnIfFalse(cmdlist->write_buffer(
				_atomsphere_properties_buffer.get(), 
				&_standard_atomsphere_properties, 
				sizeof(constant::AtmosphereProperties)
			));
		}


		uint2 thread_group_num = {
			static_cast<uint32_t>(align(TRANSMITTANCE_LUT_RES, THREAD_GROUP_SIZE_X) / THREAD_GROUP_SIZE_X),
			static_cast<uint32_t>(align(TRANSMITTANCE_LUT_RES, THREAD_GROUP_SIZE_Y) / THREAD_GROUP_SIZE_Y),
		};

		ReturnIfFalse(cmdlist->set_compute_state(_compute_state));
		ReturnIfFalse(cmdlist->dispatch(thread_group_num.x, thread_group_num.y));

		ReturnIfFalse(cmdlist->close());
        return true;
    }


}