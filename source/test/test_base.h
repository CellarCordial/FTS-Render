#ifndef TEST_BASE_H
#define TEST_BASE_H


#include <memory>
#include <wrl.h>
#include <dxgi.h>
#include <vulkan/vulkan.hpp>
#include "../core/tools/timer.h"
#include "../core/tools/ecs.h"
#include "../render_graph/render_graph.h"
#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>

namespace fantasy 
{
    class TestBase
    {
    public:
        explicit TestBase(GraphicsAPI api);
        ~TestBase();

        bool initialize();
        bool run();

        virtual RenderPassInterface* init_render_pass(RenderGraph* render_graph) = 0;

    private:
        bool init_passes();
        bool init_window();
        bool init_scene();
        bool init_d3d12();
        bool init_vulkan();
        bool create_samplers();

        
		bool enumerate_support_extension();

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
        GraphicsAPI _api;

        // D3D12.
        Microsoft::WRL::ComPtr<IDXGISwapChain> _d3d12_swap_chain;
        uint32_t _current_back_buffer_index = 0;


        // Vulkan
		vk::DebugUtilsMessengerEXT _vk_debug_messager;
		VkDebugUtilsMessengerEXT _vk_debug_callback;

		vk::Extent2D _vk_client_resolution;
		vk::SwapchainKHR _vk_swapchain;
		vk::Format _vk_swapchain_format;
		std::vector<vk::Image> _vk_back_buffers;
		std::vector<vk::ImageView> _back_buffer_views;


        std::shared_ptr<DeviceInterface> _device;
        std::shared_ptr<TextureInterface> _final_texture;
        std::shared_ptr<TextureInterface> _back_buffers[FLIGHT_FRAME_NUM];
        std::unique_ptr<RenderGraph> _render_graph;
    };
}










#endif