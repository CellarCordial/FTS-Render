#ifndef GUI_PANEL_H
#define GUI_PANEL_H
#include <imgui.h>
#include <GLFW/glfw3.h>
#include "../dynamic_rhi/device.h"

namespace fantasy::gui 
{
    bool initialize(GLFWwindow* window, DeviceInterface* device);
    void destroy();

    void add(std::function<void()>&& InFunction);
    void reset(); 
    
    bool execution(CommandListInterface* cmdlist);

    void menu_setup();
    bool has_file_selected();
    std::string get_selected_file_path();

    enum class ENotifyType : uint8_t
    {
        None,
        Success,
        Warning,
        Error,
        Info
    };
    void notify_message(ENotifyType type, std::string str);
}











#endif