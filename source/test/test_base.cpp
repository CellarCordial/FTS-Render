

#include "test_base.h"
#include "../core/tools/log.h"
#include "../dynamic_rhi/dynamic_rhi.h"
#include "../dynamic_rhi/resource.h"
#include "../shader/shader_compiler.h"
#include "../core/parallel/parallel.h"
#include "../gui/gui_pass.h"
#include "../scene/scene.h"
#include "../scene/camera.h"
#include <d3d12.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>
#include <memory>
#include <set>

#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3native.h>


#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 4;}
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace fantasy 
{
    TestBase::TestBase(GraphicsAPI api) : _api(api)
    {
        parallel::initialize();
    }

    TestBase::~TestBase()
    {
        parallel::destroy();
		glfwDestroyWindow(_window);
		glfwTerminate();
    }

    bool TestBase::initialize()
    {
        ReturnIfFalse(init_window() && init_scene());

        switch (_api)
        {
        case GraphicsAPI::D3D12: ReturnIfFalse(init_d3d12()); set_shader_platform(ShaderPlatform::DXIL); break;
        case GraphicsAPI::Vulkan: ReturnIfFalse(init_vulkan()); set_shader_platform(ShaderPlatform::SPIRV); break;
        }

        return create_samplers() && init_passes();
    }

    bool TestBase::run()
    {
        ReturnIfFalse(_render_graph->compile());
		while (!glfwWindowShouldClose(_window))
		{
			_world.tick(_timer.tick());
			ReturnIfFalse(_render_graph->execute());
			glfwPollEvents();
		}

        return true;
    }

    bool TestBase::init_passes()
    {
        std::shared_ptr<GuiPass> gui_pass = std::make_shared<GuiPass>();
		gui_pass->init(_window, _device.get());
		_render_graph->add_pass(gui_pass);
		
        RenderPassInterface* pass = init_render_pass(_render_graph.get());
		if (pass) pass->precede(gui_pass.get());

        return true;
    }


    bool TestBase::init_window()
    {
		ReturnIfFalse(glfwInit());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window = glfwCreateWindow(CLIENT_WIDTH, CLIENT_HEIGHT, "GPU-Driven-Pipeline", nullptr, nullptr);
		return _window != nullptr;
    }

    bool TestBase::init_scene()
    {
		SceneSystem* system = new SceneSystem();
		_world.register_system(system);

		system->confirm_init_models(_init_model_paths);
		auto* camera = _world.get_global_entity()->assign<Camera>(_window);
        gui::add(
            [camera]()
            {
				ImGui::SliderInt("Camera Speed", &camera->speed, 1, 30);
                ImGui::Separator();
            }
        );
        return true;
    }

    bool TestBase::init_d3d12()
    {
#ifdef DEBUG
		{
			Microsoft::WRL::ComPtr<ID3D12Debug> d3d12_debug_controller;
			ReturnIfFalse(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(d3d12_debug_controller.GetAddressOf()))));
			d3d12_debug_controller->EnableDebugLayer();
		}
#endif
		ID3D12Device* d3d12_device;
		ReturnIfFalse(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&d3d12_device))));


		ID3D12CommandQueue* d3d12_graphics_cmd_queue;
		ID3D12CommandQueue* d3d12_compute_cmd_queue;
		D3D12_COMMAND_QUEUE_DESC d3d12_queue_desc{};
		d3d12_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ReturnIfFalse(SUCCEEDED(d3d12_device->CreateCommandQueue(&d3d12_queue_desc, IID_PPV_ARGS(&d3d12_graphics_cmd_queue))));
		d3d12_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		ReturnIfFalse(SUCCEEDED(d3d12_device->CreateCommandQueue(&d3d12_queue_desc, IID_PPV_ARGS(&d3d12_compute_cmd_queue))));

		d3d12_graphics_cmd_queue->SetName(L"graphics_command_queue");
		d3d12_compute_cmd_queue->SetName(L"compute_command_queue");

		Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
		ReturnIfFalse(SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(dxgi_factory.GetAddressOf()))));

		DXGI_SWAP_CHAIN_DESC swap_chain_desc;
		swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.BufferDesc.Width = CLIENT_WIDTH;
		swap_chain_desc.BufferDesc.Height = CLIENT_HEIGHT;
		swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
		swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
		swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swap_chain_desc.BufferCount = FLIGHT_FRAME_NUM;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.OutputWindow = glfwGetWin32Window(_window);
		swap_chain_desc.SampleDesc = { 1, 0 };
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.Windowed = true;
		swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ReturnIfFalse(SUCCEEDED(dxgi_factory->CreateSwapChain(d3d12_graphics_cmd_queue, &swap_chain_desc, _d3d12_swap_chain.GetAddressOf())));

		ID3D12Resource* swap_chain_buffers[FLIGHT_FRAME_NUM];
		for (uint32_t ix = 0; ix < FLIGHT_FRAME_NUM; ++ix)
		{
			_d3d12_swap_chain->GetBuffer(ix, IID_PPV_ARGS(&swap_chain_buffers[ix]));
		}


		DX12DeviceDesc dx12_device_desc;
		dx12_device_desc.d3d12_device = d3d12_device;
		dx12_device_desc.d3d12_graphics_cmd_queue = d3d12_graphics_cmd_queue;
		dx12_device_desc.d3d12_compute_cmd_queue = d3d12_compute_cmd_queue;
		ReturnIfFalse(_device = std::shared_ptr<DeviceInterface>(CreateDevice(dx12_device_desc)));

		_render_graph = std::make_unique<RenderGraph>(
			_device,
			[this]() 
            { 
                _d3d12_swap_chain->Present(0, 0); 
                _current_back_buffer_index = (_current_back_buffer_index + 1) % FLIGHT_FRAME_NUM; 
            }
		);
		ReturnIfFalse(_render_graph->initialize(&_world));

		RenderResourceCache* cache = _render_graph->get_resource_cache();
		cache->collect_constants("back_buffer_index", &_current_back_buffer_index);

		TextureDesc back_buffer_desc;
		back_buffer_desc.width = CLIENT_WIDTH;
		back_buffer_desc.height = CLIENT_HEIGHT;
		back_buffer_desc.format = Format::RGBA8_UNORM;
		back_buffer_desc.use_clear_value = true;
		back_buffer_desc.clear_value = Color(0.0f);

		for (uint32_t ix = 0; ix < FLIGHT_FRAME_NUM; ++ix)
		{
			back_buffer_desc.name = "back_buffer" + std::to_string(ix);
			ReturnIfFalse(_back_buffers[ix] = std::shared_ptr<TextureInterface>(_device->create_texture_from_native(swap_chain_buffers[ix], back_buffer_desc)));
			cache->collect(_back_buffers[ix], ResourceType::Texture);
		}

		ReturnIfFalse(_final_texture = std::shared_ptr<TextureInterface>(_device->create_texture(
			TextureDesc::create_render_target_texture(CLIENT_WIDTH, CLIENT_HEIGHT, Format::RGBA8_UNORM, "final_texture", true)
		)));
		cache->collect(_final_texture, ResourceType::Texture);


        return true;
    }

	bool TestBase::create_samplers()
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


		RenderResourceCache* cache = _render_graph->get_resource_cache();
		cache->collect(linear_clamp_sampler, ResourceType::Sampler);
		cache->collect(point_clamp_sampler, ResourceType::Sampler);
		cache->collect(linear_warp_sampler, ResourceType::Sampler);
		cache->collect(point_wrap_sampler, ResourceType::Sampler);
		cache->collect(anisotropic_wrap_sampler, ResourceType::Sampler);

		return true;
	}

	vk::Bool32 debug_call_back(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_serverity,
		VkDebugUtilsMessageTypeFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data
	)
	{
		LOG_ERROR(callback_data->pMessage);
		return VK_FALSE;
	}

    bool TestBase::init_vulkan()
    {
		vk::ApplicationInfo vk_app_info{};
		vk_app_info.pApplicationName = "Vulkan Test";
		vk_app_info.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
		vk_app_info.engineVersion = VK_MAKE_VERSION(1, 3, 0);
		vk_app_info.apiVersion = VK_API_VERSION_1_3;	
		
		
		std::vector<const char*> validation_layers;
		std::vector<const char*> instance_extensions;
		
		uint32_t glfw_extension_count = 0;
		const CHAR** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
		instance_extensions.insert(instance_extensions.end(), glfw_extensions, glfw_extensions + glfw_extension_count);
		
#ifdef DEBUG
		validation_layers.push_back("VK_LAYER_KHRONOS_validation");
		instance_extensions.push_back("VK_EXT_debug_utils");
#endif
		
		vk::InstanceCreateInfo instance_info{};
		instance_info.pApplicationInfo = &vk_app_info;
		instance_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
		instance_info.ppEnabledLayerNames = validation_layers.data();
		instance_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
		instance_info.ppEnabledExtensionNames = instance_extensions.data();

        vk::Instance vk_instance;
		ReturnIfFalse(vk::createInstance(&instance_info, nullptr, &vk_instance) == vk::Result::eSuccess);

		VkDebugUtilsMessengerCreateInfoEXT vk_debug_util_info{};
		vk_debug_util_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		vk_debug_util_info.messageSeverity = 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		vk_debug_util_info.messageType = 
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		vk_debug_util_info.pfnUserCallback = debug_call_back;

		auto func_vk_create_debug_utils_messenger = 
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_instance, "vkCreateDebugUtilsMessengerEXT");
		func_vk_create_debug_utils_messenger(vk_instance, &vk_debug_util_info, nullptr, &_vk_debug_callback);

		VkSurfaceKHR vk_surface;
		ReturnIfFalse(glfwCreateWindowSurface(vk_instance, _window, nullptr, &vk_surface) == VK_SUCCESS);

		
		uint32_t physical_device_count = 0;
		ReturnIfFalse(vk_instance.enumeratePhysicalDevices(&physical_device_count, nullptr) == vk::Result::eSuccess);
		ReturnIfFalse(physical_device_count != 0);
		std::vector<vk::PhysicalDevice> physical_devices(physical_device_count);
		ReturnIfFalse(vk_instance.enumeratePhysicalDevices(&physical_device_count, physical_devices.data()) == vk::Result::eSuccess);

		struct 
		{
			uint32_t graphics_index = INVALID_SIZE_32;
			uint32_t present_index = INVALID_SIZE_32;
		} _queue_family_index;

		std::vector<const char*> device_extensions;
		device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		struct
		{
			vk::SurfaceCapabilitiesKHR surface_capabilities;
			std::vector<vk::SurfaceFormatKHR> surface_formats;
			std::vector<vk::PresentModeKHR> present_modes;
		} vk_swapchain_info;

		vk::PhysicalDevice vk_physical_device;
		vk::PhysicalDeviceMemoryProperties vk_memory_properties;

		for (const auto& physical_device : physical_devices)
		{
			vk::PhysicalDeviceProperties vk_physical_device_properties;
			vk::PhysicalDeviceFeatures vk_physical_device_features;
			physical_device.getProperties(&vk_physical_device_properties);
			physical_device.getFeatures(&vk_physical_device_features);

			bool device_type_support = 
				vk_physical_device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu || 
				vk_physical_device_properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu;
			
			uint32_t queue_family_count = 0;
			physical_device.getQueueFamilyProperties(&queue_family_count, nullptr);
			std::vector<vk::QueueFamilyProperties> vk_queue_family_properties(queue_family_count);
			physical_device.getQueueFamilyProperties(&queue_family_count, vk_queue_family_properties.data());

			for (uint32_t ix = 0; ix < vk_queue_family_properties.size(); ++ix)
			{
				vk::Bool32 present_support = false;
	
				if (physical_device.getSurfaceSupportKHR(ix, vk_surface, &present_support) == vk::Result::eSuccess && present_support) 
				{
					_queue_family_index.present_index = ix;
				}
				if (vk_queue_family_properties[ix].queueCount > 0 && vk_queue_family_properties[ix].queueFlags & vk::QueueFlagBits::eGraphics)
				{
					_queue_family_index.graphics_index = ix;
				}
	
				if (_queue_family_index.graphics_index != INVALID_SIZE_32 && _queue_family_index.graphics_index != INVALID_SIZE_32) 
				{
					break;
				}
			}

			uint32_t extension_count = 0;
			ReturnIfFalse(physical_device.enumerateDeviceExtensionProperties(nullptr, &extension_count, nullptr) == vk::Result::eSuccess);
			std::vector<vk::ExtensionProperties> vk_extension_properties(extension_count);
			ReturnIfFalse(physical_device.enumerateDeviceExtensionProperties(nullptr, &extension_count, vk_extension_properties.data()) == vk::Result::eSuccess);
	
			bool support_device_extension = false;
			for (const auto& property : vk_extension_properties)
			{
				if (strcmp(property.extensionName, device_extensions[0]) == 0)
				{
					support_device_extension = true;
				}
			}

			bool support_swapchain = false;

			ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfaceCapabilitiesKHR(
				vk_surface, &vk_swapchain_info.surface_capabilities
			));
	
			uint32_t surface_format_count = 0;
			ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfaceFormatsKHR(
				vk_surface, 
				&surface_format_count, 
				nullptr
			));
			support_swapchain = surface_format_count > 0;
	
			vk_swapchain_info.surface_formats.resize(surface_format_count);
			ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfaceFormatsKHR(
				vk_surface, 
				&surface_format_count, 
				vk_swapchain_info.surface_formats.data()
			));
	
			uint32_t present_mode_count = 0;
			ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfacePresentModesKHR(
				vk_surface, 
				&present_mode_count, 
				nullptr
			));
			support_swapchain = present_mode_count > 0;
	
			vk_swapchain_info.present_modes.resize(present_mode_count);
			ReturnIfFalse(vk::Result::eSuccess == physical_device.getSurfacePresentModesKHR(
				vk_surface, 
				&present_mode_count, 
				vk_swapchain_info.present_modes.data()
			));
			
	
			if (
				device_type_support && 
				_queue_family_index.graphics_index != INVALID_SIZE_32 && 
				_queue_family_index.graphics_index != INVALID_SIZE_32 &&
				support_device_extension && 
				support_swapchain
			)
			{
				vk_physical_device = physical_device;
				vk_physical_device.getMemoryProperties(&vk_memory_properties);
				break;
			}
		}


		std::set<uint32_t> queue_family_indices = { _queue_family_index.graphics_index, _queue_family_index.present_index };
		std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;

		FLOAT queue_priority = 1.0f;
		for (uint32_t ix : queue_family_indices)
		{
			auto& create_info = queue_create_infos.emplace_back();
			create_info.queueFamilyIndex = ix;
			create_info.queueCount = 1;
			create_info.pQueuePriorities = &queue_priority;
		}

		vk::DeviceCreateInfo vk_device_info{};
		vk_device_info.pQueueCreateInfos = queue_create_infos.data();
		vk_device_info.queueCreateInfoCount = 1;
		vk::PhysicalDeviceFeatures device_features{};
		vk_device_info.pEnabledFeatures = &device_features;
		vk_device_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
		vk_device_info.ppEnabledExtensionNames = device_extensions.data();
#if DEBUG
		vk_device_info.ppEnabledLayerNames = validation_layers.data();
		vk_device_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
#endif


		vk::Device vk_device;
		vk::Queue vk_graphics_queue;
		vk::Queue vk_present_queue;

		ReturnIfFalse(vk_physical_device.createDevice(&vk_device_info, nullptr, &vk_device) == vk::Result::eSuccess);
		vk_device.getQueue(_queue_family_index.graphics_index, 0, &vk_graphics_queue);
		vk_device.getQueue(_queue_family_index.present_index, 0, &vk_present_queue);


		uint32_t frames_in_flight_count = std::min(vk_swapchain_info.surface_capabilities.minImageCount + 1, static_cast<uint32_t>(FLIGHT_FRAME_NUM));
		
		if (
			vk_swapchain_info.surface_capabilities.maxImageCount > 0 &&
			frames_in_flight_count > vk_swapchain_info.surface_capabilities.maxImageCount
		)
		{
			frames_in_flight_count = vk_swapchain_info.surface_capabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapchain_create_info{};
		swapchain_create_info.surface = vk_surface;
		swapchain_create_info.minImageCount = frames_in_flight_count;

		if (
			vk_swapchain_info.surface_formats.size() == 1 &&
			vk_swapchain_info.surface_formats[0].format == vk::Format::eUndefined
		)
		{
			swapchain_create_info.imageFormat = vk::Format::eR8G8B8A8Unorm;
			swapchain_create_info.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
		}
		else 
		{
			bool found = false;
			for (const auto& surface_format : vk_swapchain_info.surface_formats)
			{
				if (
					surface_format.format == vk::Format::eR8G8B8A8Unorm &&
					surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear
				)
				{
					found = true;
					swapchain_create_info.imageFormat = vk::Format::eR8G8B8A8Unorm;
					swapchain_create_info.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
				}
			}
			if (!found)
			{
				swapchain_create_info.imageFormat = vk_swapchain_info.surface_formats[0].format;
				swapchain_create_info.imageColorSpace = vk_swapchain_info.surface_formats[0].colorSpace; 
			}
		}
		_vk_swapchain_format = swapchain_create_info.imageFormat;

		for (const auto& present_mode : vk_swapchain_info.present_modes)
		{
			if (present_mode == vk::PresentModeKHR::eMailbox) { swapchain_create_info.presentMode = present_mode; break; }
			else if (present_mode == vk::PresentModeKHR::eImmediate) { swapchain_create_info.presentMode = present_mode; break; }
			else if (present_mode == vk::PresentModeKHR::eFifo) { swapchain_create_info.presentMode = present_mode; break; }
		}

		swapchain_create_info.imageExtent = vk::Extent2D{ CLIENT_WIDTH, CLIENT_HEIGHT };
		swapchain_create_info.imageArrayLayers = 1;
		swapchain_create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
		swapchain_create_info.clipped = VK_TRUE;

		uint32_t queue_indices[] = { _queue_family_index.graphics_index, _queue_family_index.present_index };
		if (_queue_family_index.graphics_index != _queue_family_index.present_index)
		{
			swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
			swapchain_create_info.queueFamilyIndexCount = 2;
			swapchain_create_info.pQueueFamilyIndices = queue_indices;
		}
		else 
		{
			swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
			swapchain_create_info.queueFamilyIndexCount = 0;
			swapchain_create_info.pQueueFamilyIndices = nullptr;
		}
		swapchain_create_info.preTransform = vk_swapchain_info.surface_capabilities.currentTransform;
		swapchain_create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

		ReturnIfFalse(vk_device.createSwapchainKHR(&swapchain_create_info, nullptr, &_vk_swapchain) == vk::Result::eSuccess);

		uint32_t back_buffer_count = 0;
		ReturnIfFalse(vk_device.getSwapchainImagesKHR(_vk_swapchain, &back_buffer_count, nullptr) == vk::Result::eSuccess);
		ReturnIfFalse(back_buffer_count == FLIGHT_FRAME_NUM);
		ReturnIfFalse(vk_device.getSwapchainImagesKHR(_vk_swapchain, &back_buffer_count, _vk_back_buffers.data()) == vk::Result::eSuccess);
		
		_back_buffer_views.resize(back_buffer_count);
		for (uint32_t ix = 0; ix < back_buffer_count; ++ix)
		{
			vk::ImageViewCreateInfo view_create_info{};
			view_create_info.viewType = vk::ImageViewType::e2D;
			view_create_info.image = _vk_back_buffers[ix];
			view_create_info.format = swapchain_create_info.imageFormat;
			view_create_info.components.r = vk::ComponentSwizzle::eIdentity;
			view_create_info.components.g = vk::ComponentSwizzle::eIdentity;
			view_create_info.components.b = vk::ComponentSwizzle::eIdentity;
			view_create_info.components.a = vk::ComponentSwizzle::eIdentity;
			view_create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			view_create_info.subresourceRange.baseMipLevel = 0;
			view_create_info.subresourceRange.levelCount = 1;
			view_create_info.subresourceRange.baseArrayLayer = 0;
			view_create_info.subresourceRange.layerCount = 1;

			ReturnIfFalse(vk_device.createImageView(&view_create_info, nullptr, &_back_buffer_views[ix]) == vk::Result::eSuccess);
		}

        return true;
    }
}