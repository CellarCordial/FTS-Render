#ifndef GUI_PANEL_H
#define GUI_PANEL_H
#include <imgui.h>
#include <glfw3.h>
#include "../../DynamicRHI/include/Device.h"

namespace FTS 
{
    namespace Gui
    {
        void Initialize(GLFWwindow* pWindow, IDevice* pDevice);
        void Destroy();

        void Add(std::function<void()>&& InFunction);
        void Reset(); 
        
        BOOL Execution(ICommandList* pCmdList);

		void MenuSetup();
		BOOL HasFileSelected();
        std::string GetSelectedFilePath();
	}
}











#endif