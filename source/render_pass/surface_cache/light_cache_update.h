#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "../../render_graph/render_pass.h"
#include <memory>

namespace fantasy
{
	class LightCacheUpdatePass : public RenderPassInterface
	{
	public:
		LightCacheUpdatePass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

	private:
		std::shared_ptr<TextureInterface> _surface_base_color_atlas_texture;
		std::shared_ptr<TextureInterface> _surface_normal_atlas_texture;
		std::shared_ptr<TextureInterface> _surface_pbr_atlas_texture;
		std::shared_ptr<TextureInterface> _surface_emissive_atlas_texture;
		std::shared_ptr<TextureInterface> _surface_light_cache_atlas_texture;
	};
}
#endif

