#ifndef TEST_BASE_H
#define TEST_BASE_H


#include <memory>
#include <wrl.h>
#include <dxgi.h>
#include <vulkan/vulkan.hpp>
#include "../core/tools/timer.h"
#include "../core/tools/ecs.h"
#include "../gui/gui_pass.h"
#include "../render_graph/render_graph.h"

namespace fantasy 
{
    class TestBase
    {
    public:
        TestBase();
        ~TestBase();

        bool initialize(GraphicsAPI api);
        bool run();

        virtual RenderPassInterface* init_render_pass(RenderGraph* render_graph) = 0;

    private:
        bool init_gui();
        bool init_window();
        bool init_scene();
        bool init_d3d12();
        bool init_vulkan();
        bool create_samplers();

        
		bool create_instance();
		bool check_validation_layer_support();
		bool enumerate_support_extension();

		bool create_debug_utils_messager();
		bool destroy_debug_utils_messager();

		bool pick_physical_device();
		bool find_queue_family(const auto& physical_device);

		bool check_device_extension(const auto& physical_device);
		bool check_swapchain_support(const auto& physical_device);

		bool create_device();
		bool create_swapchain();
    private:
        Timer _timer;
        World _world;
        GLFWwindow* _window = nullptr;

        // D3D12.
        Microsoft::WRL::ComPtr<IDXGISwapChain> _swap_chain;
        uint32_t _current_back_buffer_index = 0;

        // Vulkan
        vk::Instance _vk_instance;

		vk::DebugUtilsMessengerEXT _vk_debug_messager;
		std::vector<const char*> _vk_validation_layers;
		std::vector<const char*> _vk_instance_extensions;
		VkDebugUtilsMessengerEXT _vk_debug_callback;

		vk::SurfaceKHR _surface;

		std::vector<const char*> _vk_device_extensions;
		vk::PhysicalDevice _vk_physical_device = VK_NULL_HANDLE;
		vk::PhysicalDeviceMemoryProperties _vk_memory_properties;

		struct 
		{
			uint32_t graphics_index = INVALID_SIZE_32;
			uint32_t present_index = INVALID_SIZE_32;
		} _queue_family_index;

		vk::Device _vk_device;
		vk::Queue _vk_graphics_queue;
		vk::Queue _vk_present_queue;

		struct
		{
			vk::SurfaceCapabilitiesKHR surface_capabilities;
			std::vector<vk::SurfaceFormatKHR> surface_formats;
			std::vector<vk::PresentModeKHR> present_modes;
		} _vk_swapchain_info;

		vk::Extent2D _vk_client_resolution;
		vk::SwapchainKHR _vk_swapchain;
		vk::Format _vk_swapchain_format;
		std::vector<vk::Image> _vk_back_buffers;
		std::vector<vk::ImageView> _back_buffer_views;


        std::shared_ptr<DeviceInterface> _device;
        std::shared_ptr<TextureInterface> _final_texture;
        std::shared_ptr<TextureInterface> _back_buffers[FLIGHT_FRAME_NUM];
        std::unique_ptr<RenderGraph> _render_graph;
        std::shared_ptr<GuiPass> _gui_pass;
    };
}










#endif