#include "gui_panel.h"
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_glfw.h>
#include <d3d12.h>
#include <imgui_file_browser.h>
#include <imgui_notify.h>

namespace fantasy 
{
    namespace gui 
    {
        static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        static std::vector<std::function<void()>> gui_funcitons;
        static std::mutex funtion_mutex;
        static ImGui::FileBrowser* file_brower = nullptr;

        void initialize(GLFWwindow* window, DeviceInterface* device)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            ImGui::StyleColorsDark();

            ImGui_ImplGlfw_InitForOther(window, true);

            ID3D12DescriptorHeap* srv_font_descriptor_heap = reinterpret_cast<ID3D12DescriptorHeap*>(device->get_native_descriptor_heap(DescriptorHeapType::ShaderResourceView));

            ImGui_ImplDX12_Init(
                reinterpret_cast<ID3D12Device*>(device->get_native_object()), 
                NUM_FRAMES_IN_FLIGHT, 
                DXGI_FORMAT_R8G8B8A8_UNORM, 
                srv_font_descriptor_heap, 
                srv_font_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                srv_font_descriptor_heap->GetGPUDescriptorHandleForHeapStart()
            );

            std::string asset_path = "asset";
            file_brower = new ImGui::FileBrowser(0, PROJ_DIR + asset_path);
            file_brower->SetTitle("File Browser");
            file_brower->SetTypeFilters({ ".gltf" });
        }

        void destroy()
        {
            delete file_brower;
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        bool execution(CommandListInterface* cmdlist)
        {
            ImGui_ImplDX12_NewFrame();
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