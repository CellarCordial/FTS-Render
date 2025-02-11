#include "gui_pass.h"
#include "../core/tools/check_cast.h"
#include "../dynamic_rhi/dx12/dx12_cmdlist.h"
#include "../dynamic_rhi/dx12/dx12_device.h"
#include "../dynamic_rhi/dx12/dx12_resource.h"
#include <d3d12.h>
#include <memory>

namespace fantasy
{
	bool GuiPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		_device = device;
		ReturnIfFalse(_final_texture = check_cast<TextureInterface>(cache->require("final_texture")));

		return true;
	}

	bool GuiPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		cmdlist->set_texture_state(_final_texture.get(), TextureSubresourceSet{}, ResourceStates::RenderTarget);
		cmdlist->commit_barriers();
		
		DeviceInterface* device = cmdlist->get_deivce();
		GraphicsAPI api = device->get_graphics_api();
		switch (api) 
		{
		case GraphicsAPI::D3D12:
			{
				DX12DescriptorManager* descriptor_manager = &check_cast<DX12Device*>(device)->descriptor_manager;
				
				uint32_t rtv_index = check_cast<DX12Texture>(_final_texture)->get_view_index(
					ResourceViewType::Texture_RTV, 
					entire_subresource_set
				);
				D3D12_CPU_DESCRIPTOR_HANDLE rtv = descriptor_manager->render_target_heap.get_cpu_handle(rtv_index);
				
				ID3D12DescriptorHeap* srv_heap = descriptor_manager->shader_resource_heap.get_shader_visible_heap();
				
				ID3D12GraphicsCommandList* d3d12_cmdlist = check_cast<DX12CommandList*>(cmdlist)->get_current_command_list()->d3d12_cmdlist.Get();
				d3d12_cmdlist->OMSetRenderTargets(1, &rtv, false, nullptr);
				d3d12_cmdlist->SetDescriptorHeaps(1, &srv_heap);
			}
			break;
		case GraphicsAPI::Vulkan:
			{
			}
			break;
		}

		gui::execution(cmdlist, device->get_graphics_api());

		uint32_t* back_buffer_index;
		ReturnIfFalse(cache->require_constants("BackBufferIndex", reinterpret_cast<void**>(&back_buffer_index)));
		std::string back_buffer_name = "BackBuffer" + std::to_string(*back_buffer_index);

		std::shared_ptr<TextureInterface> back_buffer = check_cast<TextureInterface>(cache->require(back_buffer_name.c_str()));

		cmdlist->copy_texture(back_buffer.get(), TextureSlice{}, _final_texture.get(), TextureSlice{});
		cmdlist->set_texture_state(back_buffer.get(), TextureSubresourceSet{}, ResourceStates::Present);

		cmdlist->clear_texture_float(
			_final_texture.get(), 
			entire_subresource_set, 
			_final_texture->get_desc().clear_value
		);

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}

