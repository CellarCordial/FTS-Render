#include "../include/GuiPanel.h"
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_glfw.h>
#include "../../DynamicRHI/include/Device.h"

namespace FTS 
{
    namespace Gui 
    {
        static ImVec4 ClearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        static std::vector<std::function<void()>> Funcitons;
        static std::mutex Mutex;

        void Initialize(GLFWwindow* pWindow, IDevice* pDevice)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            auto& rIO = ImGui::GetIO();
            rIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            ImGui::StyleColorsDark();

            ImGui_ImplGlfw_InitForOther(pWindow, true);

            ID3D12DescriptorHeap* pSrvFontDescriptorHeap = reinterpret_cast<ID3D12DescriptorHeap*>(pDevice->GetNativeDescriptorHeap(EDescriptorHeapType::ShaderResourceView));

            ImGui_ImplDX12_Init(
                reinterpret_cast<ID3D12Device*>(pDevice->GetNativeObject()), 
                NUM_FRAMES_IN_FLIGHT, 
                DXGI_FORMAT_R8G8B8A8_UNORM, 
                pSrvFontDescriptorHeap, 
                pSrvFontDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                pSrvFontDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
            );
        }

        void Destroy()
        {
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        BOOL Execution(ICommandList* pCmdList)
        {
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            ImGui::Begin("FTSRender");
            for (const auto& Function : Funcitons) { Function(); }
            ImGui::End();

            ImGui::Render();

            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), reinterpret_cast<ID3D12GraphicsCommandList*>(pCmdList->GetNativeObject()));
            
            return true;
        }

        void Add(std::function<void()>&& InFunction)
        {
            std::lock_guard Lock_Guard(Mutex);
            Funcitons.push_back(std::move(InFunction));
        }

        void Reset()
        {
            std::lock_guard LockGuard(Mutex);
            Funcitons.clear();
        }

    
    }

}