#include "../include/DynamicRHI.h"
#include "DX12/DX12Device.h"


#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace FTS
{
    BOOL CreateDevice(const FDX12DeviceDesc& Desc, CREFIID criid, void** ppvDevice)
    {
        FDX12Device* pDevice = new FDX12Device(Desc);
        if (!pDevice->Initialize() || !pDevice->QueryInterface(criid, ppvDevice))
        {
            LOG_ERROR("Create Device failed.");
            return false;
        }
        return true;
    }
}