#include "gui_panel.h"
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#include <d3d12.h>
#include "imgui_file_browser.h"
#include <imgui_notify.h>
#include "../dynamic_rhi/dx12/dx12_device.h"
// #include "../dynamic_rhi/vulkan/vk_device.h"
#include "../core/tools/check_cast.h"

namespace fantasy 
{
    namespace gui 
    {
        static void check_vk_result(VkResult err)
        {
            if (err == VK_SUCCESS) return;
            LOG_ERROR("ImGui init vulkan failed.");
        }

        static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        static std::vector<std::function<void()>> gui_funcitons;
        static std::mutex funtion_mutex;
        static ImGui::FileBrowser* file_brower = nullptr;
        static VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;

        bool initialize(GLFWwindow* window, DeviceInterface* device)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            ImGui::StyleColorsDark();


            GraphicsAPI api = device->get_graphics_api();
            switch (api) 
            {
            case GraphicsAPI::D3D12: 
                {
                    ReturnIfFalse(ImGui_ImplGlfw_InitForOther(window, true));

                    DX12DescriptorHeap* descriptor_heap = &check_cast<DX12Device*>(device)->descriptor_manager.shader_resource_heap;
                    ID3D12DescriptorHeap* srv_font_descriptor_heap = reinterpret_cast<ID3D12DescriptorHeap*>(
                        descriptor_heap->get_shader_visible_heap()
                    );

                    uint32_t view_index = descriptor_heap->allocate_descriptor();
                    descriptor_heap->copy_to_shader_visible_heap(view_index);

                    ReturnIfFalse(ImGui_ImplDX12_Init(
                        reinterpret_cast<ID3D12Device*>(device->get_native_object()), 
                        FLIGHT_FRAME_NUM, 
                        DXGI_FORMAT_R8G8B8A8_UNORM, 
                        srv_font_descriptor_heap, 
                        descriptor_heap->get_shader_visible_cpu_handle(view_index),
                        descriptor_heap->get_gpu_handle(view_index)
                    ));
                }
                break;
            case GraphicsAPI::Vulkan: 
                {    
                    // ReturnIfFalse(ImGui_ImplGlfw_InitForVulkan(window, true));

                    // VKDevice* vk_device = check_cast<VKDevice*>(device);
                    // VKCommandQueue* vk_queue = vk_device->get_queue(CommandQueueType::Graphics);

                    // VkDescriptorPoolSize pool_sizes[] =
                    // {
                    //     { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
                    // };
                    // VkDescriptorPoolCreateInfo pool_info = {};
                    // pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                    // pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
                    // pool_info.maxSets = 0;
                    // for (VkDescriptorPoolSize& pool_size : pool_sizes)
                    //     pool_info.maxSets += pool_size.descriptorCount;
                    // pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
                    // pool_info.pPoolSizes = pool_sizes;
                    // ReturnIfFalse(VK_SUCCESS == vkCreateDescriptorPool(
                    //     vk_device->desc.vk_device, 
                    //     &pool_info, 
                    //     (VkAllocationCallbacks*)(vk_device->context.allocation_callbacks), 
                    //     &vk_descriptor_pool
                    // ));
                    // // TODO
                    // ImGui_ImplVulkan_InitInfo init_info{};
                    // init_info.Instance = vk_device->desc.vk_instance;
                    // init_info.PhysicalDevice = vk_device->desc.vk_physical_device;
                    // init_info.Device = vk_device->desc.vk_device;
                    // init_info.QueueFamily = vk_queue->queue_family_index;
                    // init_info.Queue = vk_queue->vk_queue;
                    // init_info.PipelineCache = vk_device->context.vk_pipeline_cache;
                    // init_info.DescriptorPool = vk_descriptor_pool;
                    // init_info.RenderPass = VK_NULL_HANDLE;
                    // init_info.Subpass = 0;
                    // init_info.MinImageCount = 1;
                    // init_info.ImageCount = FLIGHT_FRAME_NUM;
                    // init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
                    // init_info.Allocator = (VkAllocationCallbacks*)(vk_device->context.allocation_callbacks);
                    // init_info.CheckVkResultFn = check_vk_result;
                    // ReturnIfFalse(ImGui_ImplVulkan_Init(&init_info));
                }
                break;
            }

            std::string asset_path = "asset";
            file_brower = new ImGui::FileBrowser(0, PROJ_DIR + asset_path);
            file_brower->SetTitle("File Browser");

            return true;
        }

        void destroy(DeviceInterface* device)
        {
            delete file_brower;

            GraphicsAPI api = device->get_graphics_api();
            switch (api) 
            {
            case GraphicsAPI::D3D12: ImGui_ImplDX12_Shutdown(); break;
            case GraphicsAPI::Vulkan: 
                // VKDevice* vk_device = check_cast<VKDevice*>(device);
                // ImGui_ImplVulkan_Shutdown();     
                // vkDestroyDescriptorPool(
                //     vk_device->desc.vk_device, 
                //     vk_descriptor_pool, 
                //     (VkAllocationCallbacks*)vk_device->context.allocation_callbacks
                // );
                break;
            }
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        bool execution(CommandListInterface* cmdlist, GraphicsAPI api)
        {
            switch (api) 
            {
            case GraphicsAPI::D3D12: 
                ImGui_ImplDX12_NewFrame(); break;
            case GraphicsAPI::Vulkan: 
                // ImGui_ImplVulkan_NewFrame(); 
                break;
            }
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            ImGui::Begin("FTSRender", nullptr, ImGuiWindowFlags_MenuBar);

            menu_setup();
            for (const auto& function : gui_funcitons) { function(); }
            ImGui::End();

            ImGui::RenderNotifications();
            file_brower->Display();

            ImGui::Render();

            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), reinterpret_cast<ID3D12GraphicsCommandList*>(cmdlist->get_native_object()));
            
            return true;
        }

		void add(std::function<void()>&& function)
        {
            std::lock_guard lock_guard(funtion_mutex);
            gui_funcitons.push_back(std::move(function));
        }

        void reset()
        {
            std::lock_guard lock_guard(funtion_mutex);
            gui_funcitons.clear();
        }


		void menu_setup()
		{
            if (ImGui::BeginMenuBar())
			{
				if (ImGui::MenuItem("File"))
				{
                    file_brower->Open();
				}
				if (ImGui::MenuItem("Console"))
				{

				}
				if (ImGui::MenuItem("Log"))
				{

				}
				ImGui::EndMenuBar();
			}
		}

        bool has_file_selected()
        {
            return file_brower->HasSelected();
        }

        std::string get_selected_file_path()
        {
            std::string str = file_brower->GetSelected().string();
            file_brower->ClearSelected();
            return str;
        }

		void notify_message(ENotifyType type, std::string str)
		{
			ImGuiToast toast(static_cast<ImGuiToastType>(type));
			toast.set_content(str.c_str());
			ImGui::InsertNotification(toast);
		}

	}

}