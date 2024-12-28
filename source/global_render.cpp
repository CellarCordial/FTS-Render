#include "global_render.h"

#include "core/tools/log.h"
#include "dynamic_rhi/dynamic_rhi.h"
#include "dynamic_rhi/resource.h"
#include "gui/gui_pass.h"
#include "shader/shader_compiler.h"
#include "core/parallel/parallel.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include <d3d12.h>
#include <memory>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#ifndef ReturnIfFailed
#define ReturnIfFailed(res)	\
	if (FAILED(res)) return false 
#endif

namespace fantasy
{
	bool GlobalRender::run()
	{
		_atmosphere_test.setup(_render_graph.get()); _atmosphere_test.get_last_pass()->precede(_gui_pass.get());
		// _sdf_test.setup(_render_graph.get()); _sdf_test.get_last_pass()->precede(_gui_pass.get());
		// _restir_test.setup(_render_graph.get()); _restir_test.get_last_pass()->precede(_gui_pass.get());


		ReturnIfFalse(_render_graph->compile());
		while (!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
			_world.tick(_timer.tick());
			ReturnIfFalse(_render_graph->execute());
		}
		return true;
	}


	GlobalRender::~GlobalRender()
	{
		shader_compile::destroy();
		parallel::destroy();
		glfwDestroyWindow(_window);
		glfwTerminate();
	}

	bool GlobalRender::Init()
	{
		ReturnIfFalse(glfwInit());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window = glfwCreateWindow(CLIENT_WIDTH, CLIENT_HEIGHT, "Fantasy-Render", nullptr, nullptr);
		if (!_window)
		{
			glfwTerminate();
			return false;
		}
		shader_compile::initialize();
		parallel::initialize();

		_world.register_system(new SceneSystem());
		_world.create_entity()->assign<Camera>(_window);

		ReturnIfFalse(D3D12Init());
		ReturnIfFalse(create_samplers());

		_gui_pass = std::make_shared<GuiPass>();
		_gui_pass->init(_window, _device.get());
		_render_graph->add_pass(_gui_pass);

		return true;
	}

	bool GlobalRender::D3D12Init()
	{
#ifdef DEBUG
		{
			Microsoft::WRL::ComPtr<ID3D12Debug> d3d12_debug_controller;
			D3D12GetDebugInterface(IID_PPV_ARGS(d3d12_debug_controller.GetAddressOf()));
			d3d12_debug_controller->EnableDebugLayer();
		}
#endif
		ID3D12Device* d3d12_device;
		ReturnIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12_device)));

		ID3D12CommandQueue* d3d12_graphics_cmd_queue;
		ID3D12CommandQueue* d3d12_compute_cmd_queue;
		D3D12_COMMAND_QUEUE_DESC d3d12_queue_desc{};
		d3d12_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ReturnIfFailed(d3d12_device->CreateCommandQueue(&d3d12_queue_desc, IID_PPV_ARGS(&d3d12_graphics_cmd_queue)));
		d3d12_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		ReturnIfFailed(d3d12_device->CreateCommandQueue(&d3d12_queue_desc, IID_PPV_ARGS(&d3d12_compute_cmd_queue)));

		Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
		ReturnIfFailed(CreateDXGIFactory(IID_PPV_ARGS(dxgi_factory.GetAddressOf())));

		DXGI_SWAP_CHAIN_DESC swap_chain_desc;
		swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.BufferDesc.Width = CLIENT_WIDTH;
		swap_chain_desc.BufferDesc.Height = CLIENT_HEIGHT;
		swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
		swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
		swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swap_chain_desc.BufferCount = NUM_FRAMES_IN_FLIGHT;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.OutputWindow = glfwGetWin32Window(_window);
		swap_chain_desc.SampleDesc = { 1, 0 };
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.Windowed = true;
		swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ReturnIfFailed(dxgi_factory->CreateSwapChain(d3d12_graphics_cmd_queue, &swap_chain_desc, _swap_chain.GetAddressOf()));

		ID3D12Resource* swap_chain_buffers[NUM_FRAMES_IN_FLIGHT];
		for (uint32_t ix = 0; ix < NUM_FRAMES_IN_FLIGHT; ++ix)
		{
			_swap_chain->GetBuffer(ix, IID_PPV_ARGS(&swap_chain_buffers[ix]));
		}


		DX12DeviceDesc dx12_device_desc;
		dx12_device_desc.d3d12_device = d3d12_device;
		dx12_device_desc.d3d12_graphics_cmd_queue = d3d12_graphics_cmd_queue;
		dx12_device_desc.d3d12_compute_cmd_queue = d3d12_compute_cmd_queue;
		ReturnIfFalse(_device = std::shared_ptr<DeviceInterface>(CreateDevice(dx12_device_desc)));

		_render_graph = std::make_unique<RenderGraph>(
			_device,
			[this]() { _swap_chain->Present(0, 0); _current_back_buffer_index = (_current_back_buffer_index + 1) % NUM_FRAMES_IN_FLIGHT; }
		);
		ReturnIfFalse(_render_graph->initialize(&_world));

		RenderResourceCache* cache = _render_graph->GetResourceCache();
		cache->collect_constants("BackBufferIndex", &_current_back_buffer_index);

		TextureDesc back_buffer_desc;
		back_buffer_desc.width = CLIENT_WIDTH;
		back_buffer_desc.height = CLIENT_HEIGHT;
		back_buffer_desc.initial_state = ResourceStates::Present;
		back_buffer_desc.format = Format::RGBA8_UNORM;
		back_buffer_desc.use_clear_value = true;
		back_buffer_desc.clear_value = Color(0.0f, 0.0f, 0.0f, 1.0f);

		for (uint32_t ix = 0; ix < NUM_FRAMES_IN_FLIGHT; ++ix)
		{
			back_buffer_desc.name = "BackBuffer" + std::to_string(ix);
			ReturnIfFalse(_back_buffers[ix] = std::shared_ptr<TextureInterface>(_device->create_texture_from_native(swap_chain_buffers[ix], back_buffer_desc)));
			cache->collect(_back_buffers[ix], ResourceType::Texture);
		}

		ReturnIfFalse(_final_texture = std::shared_ptr<TextureInterface>(_device->create_texture(
			TextureDesc::create_render_target(CLIENT_WIDTH, CLIENT_HEIGHT, Format::RGBA8_UNORM, "FinalTexture")
		)));
		cache->collect(_final_texture, ResourceType::Texture);

		return true;
	}

	bool GlobalRender::create_samplers()
	{
		std::shared_ptr<SamplerInterface> linear_clamp_sampler;
		std::shared_ptr<SamplerInterface> point_clamp_sampler;
		std::shared_ptr<SamplerInterface> linear_warp_sampler;
		std::shared_ptr<SamplerInterface> point_wrap_sampler;
		std::shared_ptr<SamplerInterface> anisotropic_wrap_sampler;

		SamplerDesc sampler_desc;
		sampler_desc.name = "linear_wrap_sampler";
		ReturnIfFalse(linear_warp_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));
		sampler_desc.SetFilter(false);
		sampler_desc.name = "point_wrap_sampler";
		ReturnIfFalse(point_wrap_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));
		sampler_desc.SetAddressMode(SamplerAddressMode::Clamp);
		sampler_desc.name = "point_clamp_sampler";
		ReturnIfFalse(point_clamp_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));
		sampler_desc.SetFilter(true);
		sampler_desc.name = "linear_clamp_sampler";
		ReturnIfFalse(linear_clamp_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));

		sampler_desc.name = "anisotropic_wrap_sampler";
		sampler_desc.SetAddressMode(SamplerAddressMode::Wrap);
		sampler_desc.max_anisotropy = 8.0f;
		ReturnIfFalse(anisotropic_wrap_sampler = std::shared_ptr<SamplerInterface>(_device->create_sampler(sampler_desc)));


		RenderResourceCache* cache = _render_graph->GetResourceCache();
		cache->collect(linear_clamp_sampler, ResourceType::Sampler);
		cache->collect(point_clamp_sampler, ResourceType::Sampler);
		cache->collect(linear_warp_sampler, ResourceType::Sampler);
		cache->collect(point_wrap_sampler, ResourceType::Sampler);
		cache->collect(anisotropic_wrap_sampler, ResourceType::Sampler);

		return true;
	}
}