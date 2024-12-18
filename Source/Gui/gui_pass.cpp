#include "gui_pass.h"
#include "../core/tools/check_cast.h"
#include <memory>

namespace fantasy
{
	bool GuiPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{
		ReturnIfFalse(_final_texture = check_cast<TextureInterface>(cache->require("FinalTexture")));

		FrameBufferAttachmentArray color_attachment(1);
		color_attachment[0].format = Format::RGBA8_UNORM;
		color_attachment[0].texture = _final_texture;
		
		
		ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(
			device->create_frame_buffer(FrameBufferDesc{ .color_attachments = color_attachment })
		));

		return true;
	}

	bool GuiPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		ReturnIfFalse(cmdlist->bind_frame_buffer(_frame_buffer.get()));
		ReturnIfFalse(cmdlist->commit_descriptor_heaps());
		gui::execution(cmdlist);

		uint32_t* back_buffer_index;
		ReturnIfFalse(cache->require_constants("BackBufferIndex", reinterpret_cast<void**>(&back_buffer_index)));
		std::string back_buffer_name = "BackBuffer" + std::to_string(*back_buffer_index);

		std::shared_ptr<TextureInterface> back_buffer = check_cast<TextureInterface>(cache->require(back_buffer_name.c_str()));

		ReturnIfFalse(cmdlist->copy_texture(back_buffer.get(), TextureSlice{}, _final_texture.get(), TextureSlice{}));
		ReturnIfFalse(cmdlist->set_texture_state(back_buffer.get(), TextureSubresourceSet{}, ResourceStates::Present));

		ReturnIfFalse(cmdlist->close());
		return true;
	}
}

