#include "gui_pass.h"
#include "../core/tools/check_cast.h"
#include "../dynamic_rhi/dx12/dx12_cmdlist.h"
#include <memory>

namespace fantasy
{
	bool GuiPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		ReturnIfFalse(_final_texture = check_cast<TextureInterface>(cache->require("FinalTexture")));

		FrameBufferAttachmentArray color_attachment(1);
		color_attachment[0].texture = _final_texture;
		
		ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(
			device->create_frame_buffer(FrameBufferDesc{ .color_attachments = color_attachment })
		));

		return true;
	}

	bool GuiPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		cmdlist->set_texture_state(_final_texture.get(), TextureSubresourceSet{}, ResourceStates::RenderTarget);
		
		GraphicsAPI api = cmdlist->get_deivce()->get_graphics_api();
		switch (api) 
		{
		case GraphicsAPI::D3D12:
			{
				ID3D12GraphicsCommandList* d3d12_cmdlist = check_cast<DX12CommandList*>(cmdlist)->get_current_command_list()->d3d12_cmdlist.Get();
				d3d12_cmdlist->OMSetRenderTargets();
				d3d12_cmdlist->SetDescriptorHeaps(1, );
			}
			break;
		case GraphicsAPI::Vulkan:
			{
			}
			break;
		}


		cmdlist->commit_barriers();
		gui::execution(cmdlist);

		uint32_t* back_buffer_index;
		ReturnIfFalse(cache->require_constants("BackBufferIndex", reinterpret_cast<void**>(&back_buffer_index)));
		std::string back_buffer_name = "BackBuffer" + std::to_string(*back_buffer_index);

		std::shared_ptr<TextureInterface> back_buffer = check_cast<TextureInterface>(cache->require(back_buffer_name.c_str()));

		cmdlist->copy_texture(back_buffer.get(), TextureSlice{}, _final_texture.get(), TextureSlice{});
		cmdlist->set_texture_state(back_buffer.get(), TextureSubresourceSet{}, ResourceStates::Present);

		ReturnIfFalse(clear_color_attachment(cmdlist, _frame_buffer.get(), 0));

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}

