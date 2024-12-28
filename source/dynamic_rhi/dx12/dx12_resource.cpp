#include "dx12_resource.h"
#include "dx12_Converts.h"
#include "dx12_forward.h"
#include <combaseapi.h>
#include <d3d12.h>
#include <dxgiformat.h>
#include <memory>
#include <winerror.h>
#include "../forward.h"
#include "../../core/tools/check_cast.h"

namespace fantasy 
{
    DX12Texture::DX12Texture(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, const TextureDesc& desc) :
        _context(context), _descriptor_heaps(descriptor_heaps), _desc(desc)
    {
    }

    DX12Texture::~DX12Texture() noexcept
    {
        for (const auto& pair : rtv_indices) _descriptor_heaps->render_target_heap.release_descriptor(pair.second);
        for (const auto& pair : dsv_indices) _descriptor_heaps->depth_stencil_heap.release_descriptor(pair.second);
        for (const auto& pair : _srv_indices) _descriptor_heaps->shader_resource_heap.release_descriptor(pair.second);
        for (const auto& pair : _uav_indices) _descriptor_heaps->shader_resource_heap.release_descriptor(pair.second);

        for (auto index : _clear_mip_level_uav_indices) _descriptor_heaps->shader_resource_heap.release_descriptor(index);
    }

    bool DX12Texture::initialize()
    {
        if (!_desc.is_render_target && !_desc.is_depth_stencil && _desc.use_clear_value)
        {
            LOG_ERROR("ClearValue must not be used when texture is neither render target nor depth stencil.");
            return false;
        }

        _d3d12_resource_desc = convert_texture_desc(_desc);

        // If the resource is created in bindTextureMemory. 
        if (_desc.is_virtual) return true;

        D3D12_HEAP_PROPERTIES d3d12_heap_properties{};
        d3d12_heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE d3d12_clear_value = convert_clear_value(_desc);

        if (FAILED(_context->device->CreateCommittedResource(
            &d3d12_heap_properties, 
            D3D12_HEAP_FLAG_NONE, 
            &_d3d12_resource_desc, 
            convert_resource_states(_desc.initial_state), 
            _desc.use_clear_value ? &d3d12_clear_value : nullptr, 
            IID_PPV_ARGS(_d3d12_resource.GetAddressOf())
        )))
        {
            LOG_ERROR("Call to ID3D12Device::CreateCommittedResource failed.");
            return false;
        }
        
        std::wstring name(_desc.name.begin(), _desc.name.end());
		_d3d12_resource->SetName(name.c_str());

        if (_desc.is_uav)
        {
            _clear_mip_level_uav_indices.resize(_desc.mip_levels);
            std::fill(_clear_mip_level_uav_indices.begin(), _clear_mip_level_uav_indices.end(), INVALID_SIZE_32);
        }

        plane_count = _descriptor_heaps->get_format_plane_num(_d3d12_resource_desc.Format);

        return true;
    }


    uint32_t DX12Texture::get_view_index(
        ViewType type,
        TextureSubresourceSet subresource,
        bool is_read_only_dsv
    )
    {
        switch (type)
        {
        case ViewType::DX12_GPU_Texture_SRV:
            {
                DX12TextureBindingKey binding_key(subresource, _desc.format);
                uint32_t descriptor_index;
                auto iter = _srv_indices.find(binding_key);
                if (iter == _srv_indices.end())    // If not found, then create one. 
                {
                    descriptor_index = _descriptor_heaps->shader_resource_heap.allocate_descriptor();
                    _srv_indices[binding_key] = descriptor_index;
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle = 
                        _descriptor_heaps->shader_resource_heap.get_cpu_handle(descriptor_index);

                    create_srv(cpu_descriptor_handle.ptr, _desc.format, _desc.dimension, subresource);
                    _descriptor_heaps->shader_resource_heap.copy_to_shader_visible_heap(descriptor_index);
                }
                else    // If found, then return it directly. 
                {
                    descriptor_index = iter->second;
                }

                return descriptor_index;
            }
        case ViewType::DX12_GPU_Texture_UAV:
            {
                DX12TextureBindingKey binding_key(subresource, _desc.format);
                uint32_t descriptor_index;
                auto iter = _uav_indices.find(binding_key);
                if (iter == _uav_indices.end())    // If not found, then create one. 
                {
                    descriptor_index = _descriptor_heaps->shader_resource_heap.allocate_descriptor();
                    _uav_indices[binding_key] = descriptor_index;
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle = 
                        _descriptor_heaps->shader_resource_heap.get_cpu_handle(descriptor_index);

                    create_uav(cpu_descriptor_handle.ptr, _desc.format, _desc.dimension, subresource);
                    _descriptor_heaps->shader_resource_heap.copy_to_shader_visible_heap(descriptor_index);
                }
                else    // If found, then return it directly. 
                {
                    descriptor_index = iter->second;
                }

				return descriptor_index;
            }
        case ViewType::DX12_RenderTargetView:
            {
                DX12TextureBindingKey binding_key(subresource, _desc.format);
                uint32_t descriptor_index;
                auto iter = rtv_indices.find(binding_key);
                if (iter == rtv_indices.end())    // If not found, then create one. 
                {
                    descriptor_index = _descriptor_heaps->render_target_heap.allocate_descriptor();
                    rtv_indices[binding_key] = descriptor_index;
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle = 
                        _descriptor_heaps->render_target_heap.get_cpu_handle(descriptor_index);

                    CreateRTV(cpu_descriptor_handle.ptr, _desc.format, subresource);
                }
                else    // If found, then return it directly. 
                {
                    descriptor_index = iter->second;
                }

				return descriptor_index;
            }
        case ViewType::DX12_DepthStencilView:
            {
                DX12TextureBindingKey binding_key(subresource, _desc.format, is_read_only_dsv);
                uint32_t descriptor_index;
                auto iter = dsv_indices.find(binding_key);
                if (iter == dsv_indices.end())    // If not found, then create one. 
                {
                    descriptor_index = _descriptor_heaps->depth_stencil_heap.allocate_descriptor();
                    dsv_indices[binding_key] = descriptor_index;
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle = 
                        _descriptor_heaps->depth_stencil_heap.get_cpu_handle(descriptor_index);

                    CreateDSV(cpu_descriptor_handle.ptr, subresource, is_read_only_dsv);
                }
                else    // If found, then return it directly. 
                {
                    descriptor_index = iter->second;
                }

				return descriptor_index;
            }
        default:
            assert(false && "invalid enum.");
        }
        return INVALID_SIZE_32;
    }

    void DX12Texture::create_srv(uint64_t descriptor_address, Format Format, TextureDimension dimension, const TextureSubresourceSet& subresource_set)
    {
        TextureSubresourceSet resolved_subresource_set = subresource_set.resolve(_desc, false);

        if (dimension == TextureDimension::Unknown) dimension = _desc.dimension;

        D3D12_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Format = get_dxgi_format_mapping(Format == Format::UNKNOWN ? _desc.format : Format).srv_format;
        view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        uint32_t plane_slice = 0;

        switch (dimension)
        {
        case TextureDimension::Texture1D: 
            view_desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipLevels       = resolved_subresource_set.mip_level_count;
            view_desc.Texture1D.MostDetailedMip = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture1DArray: 
            view_desc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            view_desc.Texture1DArray.ArraySize       = resolved_subresource_set.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture1DArray.MipLevels       = resolved_subresource_set.mip_level_count;
            view_desc.Texture1DArray.MostDetailedMip = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2D: 
            view_desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
            view_desc.Texture2D.MipLevels       = resolved_subresource_set.mip_level_count;
            view_desc.Texture2D.MostDetailedMip = resolved_subresource_set.base_mip_level;
            view_desc.Texture2D.PlaneSlice      = plane_slice;
            break;
        case TextureDimension::Texture2DArray: 
            view_desc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture2DArray.ArraySize       = resolved_subresource_set.array_slice_count;
            view_desc.Texture2DArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture2DArray.MipLevels       = resolved_subresource_set.mip_level_count;
            view_desc.Texture2DArray.MostDetailedMip = resolved_subresource_set.base_mip_level;
            view_desc.Texture2DArray.PlaneSlice      = plane_slice;
            break;
        case TextureDimension::TextureCube: 
            view_desc.ViewDimension               = D3D12_SRV_DIMENSION_TEXTURECUBE;
            view_desc.TextureCube.MipLevels       = resolved_subresource_set.mip_level_count;
            view_desc.TextureCube.MostDetailedMip = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::TextureCubeArray: 
            view_desc.ViewDimension                     = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            view_desc.TextureCubeArray.MipLevels        = resolved_subresource_set.mip_level_count;
            view_desc.TextureCubeArray.MostDetailedMip  = resolved_subresource_set.base_mip_level;
            view_desc.TextureCubeArray.First2DArrayFace = resolved_subresource_set.base_array_slice;
            view_desc.TextureCubeArray.NumCubes         = resolved_subresource_set.array_slice_count / 6;
            break;
        case TextureDimension::Texture2DMS: 
            view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
            break;
        case TextureDimension::Texture2DMSArray: 
            view_desc.ViewDimension                    = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
            view_desc.Texture2DMSArray.ArraySize       = resolved_subresource_set.array_slice_count;
            view_desc.Texture2DMSArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            break;
        case TextureDimension::Texture3D: 
            view_desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE3D;
            view_desc.Texture3D.MipLevels       = resolved_subresource_set.mip_level_count;
            view_desc.Texture3D.MostDetailedMip = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Unknown: 
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateShaderResourceView(
            _d3d12_resource.Get(), 
            &view_desc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor_address }
        );
    }

    void DX12Texture::create_uav(uint64_t descriptor_address, Format format, TextureDimension dimension, const TextureSubresourceSet& subresource_set)
    {
        TextureSubresourceSet resolved_subresource_set = subresource_set.resolve(_desc, true);     // uav 应该为单个 miplevel

        if (dimension == TextureDimension::Unknown) dimension = _desc.dimension;

        D3D12_UNORDERED_ACCESS_VIEW_DESC view_desc{};
        view_desc.Format = get_dxgi_format_mapping(format == Format::UNKNOWN ? _desc.format : format).srv_format;

        switch (dimension)
        {
        case TextureDimension::Texture1D:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture1DArray:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            view_desc.Texture1DArray.ArraySize = resolved_subresource_set.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture1DArray.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2D:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            view_desc.Texture2D.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture1DArray.ArraySize = resolved_subresource_set.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture1DArray.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture3D:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            view_desc.Texture3D.FirstWSlice = 0;
            view_desc.Texture3D.WSize = _desc.depth;
            view_desc.Texture3D.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2DMS:
        case TextureDimension::Texture2DMSArray:
        case TextureDimension::Unknown:
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateUnorderedAccessView(
            _d3d12_resource.Get(), 
            nullptr,
            &view_desc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor_address }
        );
    }

    void DX12Texture::CreateRTV(uint64_t descriptor_address, Format format, const TextureSubresourceSet& subresource_set)
    {
        TextureSubresourceSet resolved_subresource_set = subresource_set.resolve(_desc, true);

        D3D12_RENDER_TARGET_VIEW_DESC view_desc{};
        view_desc.Format = get_dxgi_format_mapping(format == Format::UNKNOWN ? _desc.format : format).rtv_format;

        switch (_desc.dimension)
        {
        case TextureDimension::Texture1D:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture1DArray:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            view_desc.Texture1DArray.ArraySize = resolved_subresource_set.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture1DArray.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2D:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            view_desc.Texture1D.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture1DArray.ArraySize = resolved_subresource_set.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture1DArray.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2DMS:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            break;
        case TextureDimension::Texture2DMSArray:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
            view_desc.Texture2DMSArray.ArraySize = resolved_subresource_set.array_slice_count;
            view_desc.Texture2DMSArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            break;
        case TextureDimension::Texture3D:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
            view_desc.Texture3D.FirstWSlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture3D.WSize = resolved_subresource_set.array_slice_count;
            view_desc.Texture3D.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Unknown:
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateRenderTargetView(
            _d3d12_resource.Get(), 
            &view_desc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor_address }
        );
    }

    void DX12Texture::CreateDSV(uint64_t descriptor_address, const TextureSubresourceSet& subresource_set, bool is_read_only)
    {
        TextureSubresourceSet resolved_subresource_set = subresource_set.resolve(_desc, true);

        D3D12_DEPTH_STENCIL_VIEW_DESC view_desc{};
        view_desc.Format = get_dxgi_format_mapping(_desc.format).rtv_format;

        if (is_read_only)
        {
            view_desc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
            if (view_desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT || view_desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
            {
                view_desc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
            }
        }

        switch (_desc.dimension)
        {
        case TextureDimension::Texture1D:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture1DArray:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            view_desc.Texture1DArray.ArraySize = resolved_subresource_set.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture1DArray.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2D:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            view_desc.Texture1D.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture1DArray.ArraySize = resolved_subresource_set.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            view_desc.Texture1DArray.MipSlice = resolved_subresource_set.base_mip_level;
            break;
        case TextureDimension::Texture2DMS:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            break;
        case TextureDimension::Texture2DMSArray:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
            view_desc.Texture2DMSArray.ArraySize = resolved_subresource_set.array_slice_count;
            view_desc.Texture2DMSArray.FirstArraySlice = resolved_subresource_set.base_array_slice;
            break;
        case TextureDimension::Texture3D:
        case TextureDimension::Unknown:
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateDepthStencilView(
            _d3d12_resource.Get(), 
            &view_desc, 
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor_address }
        );
    }

    uint32_t DX12Texture::GetClearMipLevelUAVIndex(uint32_t mip_level)
    {
        assert(_desc.bIsUAV);

        uint32_t descriptor_index = _clear_mip_level_uav_indices[mip_level];
        if (descriptor_index != INVALID_SIZE_32) return descriptor_index;

        descriptor_index = _descriptor_heaps->shader_resource_heap.allocate_descriptor();

        assert(descriptor_index != INVALID_SIZE_32);
        
        TextureSubresourceSet resolved_subresource_set{ mip_level, 1, 0, static_cast<uint32_t>(-1) };
        create_uav(
            _descriptor_heaps->shader_resource_heap.get_cpu_handle(descriptor_index).ptr,
            Format::UNKNOWN,
            TextureDimension::Unknown,
            resolved_subresource_set
        );
        _descriptor_heaps->shader_resource_heap.copy_to_shader_visible_heap(descriptor_index);
        _clear_mip_level_uav_indices[mip_level] = descriptor_index;

        return descriptor_index;
    }


    MemoryRequirements DX12Texture::get_memory_requirements()
    {
        D3D12_RESOURCE_ALLOCATION_INFO allocation_info = _context->device->GetResourceAllocationInfo(1, 1, &_d3d12_resource_desc);

        return MemoryRequirements{ allocation_info.Alignment, allocation_info.SizeInBytes };
    }


    bool DX12Texture::bind_memory(HeapInterface* heap, uint64_t offset)
    {
        if (heap == nullptr || !_desc.is_virtual || _d3d12_resource == nullptr) return false;

        DX12Heap* dx12_heap = check_cast<DX12Heap*>(heap);
        ReturnIfFalse(dx12_heap->_d3d12_heap.Get() != nullptr);

        D3D12_CLEAR_VALUE d3d12_clear_value = convert_clear_value(_desc);
        if (FAILED(_context->device->CreatePlacedResource(
            dx12_heap->_d3d12_heap.Get(), 
            offset, 
            &_d3d12_resource_desc, 
            convert_resource_states(_desc.initial_state), 
            _desc.use_clear_value ? &d3d12_clear_value : nullptr,  
            IID_PPV_ARGS(_d3d12_resource.GetAddressOf())
        )))
        {
            LOG_ERROR("Bind texture memory failed.");
            return false;
        }

        _heap = heap;
                
        if (_desc.is_uav)
        {
            _clear_mip_level_uav_indices.resize(_desc.mip_levels);
            std::fill(_clear_mip_level_uav_indices.begin(), _clear_mip_level_uav_indices.end(), INVALID_SIZE_32);
        }

        plane_count = _descriptor_heaps->get_format_plane_num(_d3d12_resource_desc.Format);

        return true;
    }


    DX12Buffer::DX12Buffer(const DX12Context* context, DX12DescriptorHeaps* descriptor_heaps, const BufferDesc& desc) :
        _context(context), _descriptor_heaps(descriptor_heaps), _desc(desc)
    {
    }

    DX12Buffer::~DX12Buffer() noexcept
    {
        if (_clear_uav_index != INVALID_SIZE_32)
        {
            _descriptor_heaps->shader_resource_heap.release_descriptor(_clear_uav_index);
            _clear_uav_index = INVALID_SIZE_32;
        }
		if (_srv_index != INVALID_SIZE_32) _descriptor_heaps->shader_resource_heap.release_descriptor(_srv_index);
		if (_uav_index != INVALID_SIZE_32) _descriptor_heaps->shader_resource_heap.release_descriptor(_uav_index);
    }

    bool DX12Buffer::initialize()
    {
        if (_desc.is_constant_buffer)
        {
            _desc.byte_size = align(_desc.byte_size, static_cast<uint64_t>(CONSTANT_BUFFER_OFFSET_SIZE_ALIGMENT));
        }
        
        // Do not create any resources for volatile buffers.
        if (_desc.is_volatile) return true;

        _d3d12_resource_desc.SampleDesc = { 1, 0 };
        _d3d12_resource_desc.Alignment = 0;
        _d3d12_resource_desc.DepthOrArraySize = 1;
        _d3d12_resource_desc.Height = 1;
        _d3d12_resource_desc.Width = _desc.byte_size;
        _d3d12_resource_desc.MipLevels = 1;
        _d3d12_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        _d3d12_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
        _d3d12_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (_desc.can_have_uavs) _d3d12_resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        if (_desc.is_virtual) return true;


        D3D12_HEAP_PROPERTIES d3d12_heap_properties{};
        D3D12_RESOURCE_STATES d3d12_initial_state = D3D12_RESOURCE_STATE_COMMON;

        switch (_desc.cpu_access)
        {
        case CpuAccessMode::None:
            d3d12_heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            d3d12_initial_state = D3D12_RESOURCE_STATE_COMMON;
            break;

        case CpuAccessMode::Read:
            d3d12_heap_properties.Type = D3D12_HEAP_TYPE_READBACK;
            d3d12_initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
            break;

        case CpuAccessMode::Write:
            d3d12_heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
            d3d12_initial_state = D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        }

        if (FAILED(_context->device->CreateCommittedResource(
            &d3d12_heap_properties, 
            D3D12_HEAP_FLAG_NONE, 
            &_d3d12_resource_desc, 
            d3d12_initial_state, 
            nullptr, 
            IID_PPV_ARGS(_d3d12_resource.GetAddressOf())
        )))
        {
            LOG_ERROR("Call to ID3D12Device::CreateCommittedResource failed.");
            return false;
        }

        std::wstring name(_desc.name.begin(), _desc.name.end());
        _d3d12_resource->SetName(name.c_str());
        return true;
    }
    
    
    void* DX12Buffer::map(CpuAccessMode cpu_access, HANDLE fence_event)
    {
        if (last_used_d3d12_fence != nullptr)
        {
            wait_for_fence(last_used_d3d12_fence.Get(), last_used_fence_value, fence_event);
            last_used_d3d12_fence = nullptr;
        }

        D3D12_RANGE range;
        if (cpu_access == CpuAccessMode::Read)
        {
            range = { 0, _desc.byte_size };
        }
        else    // cpu_access == CpuAccessMode::Write
        {
            range = { 0, 0 };
        }

        void* mapped_address = nullptr;
        if (FAILED(_d3d12_resource->Map(0, &range, &mapped_address)))
        {
            LOG_ERROR("Map buffer failed. ");
            return nullptr;
        }

        return mapped_address;
    }

    void DX12Buffer::unmap()
    {
        _d3d12_resource->Unmap(0, nullptr);
    }
    
    MemoryRequirements DX12Buffer::get_memory_requirements()
    {
        D3D12_RESOURCE_ALLOCATION_INFO alloc_info = _context->device->GetResourceAllocationInfo(1, 1, &_d3d12_resource_desc);

        return MemoryRequirements{ 
            alloc_info.Alignment, 
            alloc_info.SizeInBytes 
        };
    }

    bool DX12Buffer::bind_memory(HeapInterface* heap, uint64_t offset)
    {
        if (heap == nullptr || _d3d12_resource == nullptr || _desc.is_virtual) return false;
        
        DX12Heap* pDX12Heap = check_cast<DX12Heap*>(heap);
        ReturnIfFalse(pDX12Heap->_d3d12_heap.Get() != nullptr);

        if (FAILED(_context->device->CreatePlacedResource(
            pDX12Heap->_d3d12_heap.Get(), 
            offset, 
            &_d3d12_resource_desc, 
            convert_resource_states(_desc.initial_state), 
            nullptr, 
            IID_PPV_ARGS(_d3d12_resource.GetAddressOf())
        )))
        {
            LOG_ERROR("Bind memory failed.");
            return false;
        }

        _heap = heap;

        return true;
    }

    
	uint32_t DX12Buffer::get_view_index(ViewType type, const BufferRange& range)
	{
        bool srv_or_uav = true; // SRV is true, UAV is false.
        ResourceViewType resource_type = ResourceViewType::None;

        switch (type)
        {
        case ViewType::DX12_GPU_TypedBuffer_SRV: 
            srv_or_uav = true; 
            resource_type = ResourceViewType::TypedBuffer_SRV; 
            break;
		case ViewType::DX12_GPU_StructuredBuffer_SRV:
			srv_or_uav = true;
			resource_type = ResourceViewType::StructuredBuffer_SRV;
			break;
		case ViewType::DX12_GPU_RawBuffer_SRV:
			srv_or_uav = true;
			resource_type = ResourceViewType::RawBuffer_SRV;
			break;

		case ViewType::DX12_GPU_TypedBuffer_UAV:
			srv_or_uav = false;
			resource_type = ResourceViewType::TypedBuffer_UAV;
			break;
		case ViewType::DX12_GPU_StructuredBuffer_UAV:
			srv_or_uav = false;
			resource_type = ResourceViewType::StructuredBuffer_UAV;
			break;
		case ViewType::DX12_GPU_RawBuffer_UAV:
			srv_or_uav = false;
			resource_type = ResourceViewType::RawBuffer_UAV;
			break;

		default:
			assert(false && "invalid enum.");
        }

        if (srv_or_uav)
		{
			uint32_t descriptor_index;
			if (_srv_index == INVALID_SIZE_32)
			{
				descriptor_index = _descriptor_heaps->shader_resource_heap.allocate_descriptor();
				_srv_index = descriptor_index;

				const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle =
					_descriptor_heaps->shader_resource_heap.get_cpu_handle(descriptor_index);

				create_srv(cpu_descriptor_handle.ptr, _desc.format, range, resource_type);
				_descriptor_heaps->shader_resource_heap.copy_to_shader_visible_heap(descriptor_index);
			}
			else
			{
				descriptor_index = _srv_index;
			}

			return descriptor_index;
		}
        else
		{
			uint32_t descriptor_index;
			if (_uav_index == INVALID_SIZE_32)
			{
				descriptor_index = _descriptor_heaps->shader_resource_heap.allocate_descriptor();
				_uav_index = descriptor_index;

				const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle =
					_descriptor_heaps->shader_resource_heap.get_cpu_handle(descriptor_index);

				create_uav(cpu_descriptor_handle.ptr, _desc.format, range, resource_type);
				_descriptor_heaps->shader_resource_heap.copy_to_shader_visible_heap(descriptor_index);
			}
			else
			{
				descriptor_index = _uav_index;
			}

			return descriptor_index;
		}
        return INVALID_SIZE_32;
	}

	void DX12Buffer::create_cbv(uint64_t descriptor_address, const BufferRange& range)
    {
        assert(_desc.bIsConstantBuffer);

        BufferRange resolved_range = range.resolve(_desc);
        assert(resolved_range.byte_size <= UINT_MAX);

        D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc;
        view_desc.BufferLocation = _d3d12_resource->GetGPUVirtualAddress() + resolved_range.byte_offset;
        view_desc.SizeInBytes = align(static_cast<uint32_t>(resolved_range.byte_size), CONSTANT_BUFFER_OFFSET_SIZE_ALIGMENT);

        _context->device->CreateConstantBufferView(&view_desc, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor_address });
    }

    void DX12Buffer::create_srv(uint64_t descriptor_address, Format Format, const BufferRange& range, ResourceViewType ResourceViewType)
    {
        BufferRange resolved_range = range.resolve(_desc);

        if (Format == Format::UNKNOWN) Format = _desc.format;

        D3D12_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

        switch (ResourceViewType)
        {
        case ResourceViewType::StructuredBuffer_SRV:
            if (_desc.struct_stride == 0)
            {
                LOG_ERROR("StructuredBuffer's stride can't be 0.");
                return;
            }
            view_desc.Format = DXGI_FORMAT_UNKNOWN;
            view_desc.Buffer.StructureByteStride = _desc.struct_stride;
            view_desc.Buffer.FirstElement = resolved_range.byte_offset / _desc.struct_stride;
            view_desc.Buffer.NumElements = static_cast<uint32_t>(resolved_range.byte_size / _desc.struct_stride);
            break;
        case ResourceViewType::TypedBuffer_SRV:
            {
                assert(Format != Format::UNKNOWN);
                view_desc.Format = get_dxgi_format_mapping(Format).srv_format;

                uint8_t pixel_size = get_format_info(Format).byte_size_per_pixel;
                view_desc.Buffer.FirstElement = resolved_range.byte_offset / pixel_size;
                view_desc.Buffer.NumElements = static_cast<uint32_t>(resolved_range.byte_size / pixel_size);
                break;
            }
        case ResourceViewType::RawBuffer_SRV:
            view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            view_desc.Buffer.FirstElement = resolved_range.byte_offset / 4;
            view_desc.Buffer.NumElements = static_cast<uint32_t>(resolved_range.byte_size / 4);
            view_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            break;
        default:
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateShaderResourceView(_d3d12_resource.Get(), &view_desc, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor_address });
    }
    
    void DX12Buffer::create_uav(uint64_t descriptor_address, Format format, const BufferRange& range, ResourceViewType ResourceViewType)
    {
        BufferRange resolved_range = range.resolve(_desc);

        if (format == Format::UNKNOWN) format = _desc.format;

        D3D12_UNORDERED_ACCESS_VIEW_DESC view_desc{};
        view_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        
        switch (ResourceViewType) 
        {
        case ResourceViewType::StructuredBuffer_UAV:
            assert(_desc.struct_stride != 0);
            view_desc.Format = DXGI_FORMAT_UNKNOWN;
            view_desc.Buffer.StructureByteStride = _desc.struct_stride;
            view_desc.Buffer.FirstElement = resolved_range.byte_offset / _desc.struct_stride;
            view_desc.Buffer.NumElements = static_cast<uint32_t>(resolved_range.byte_size / _desc.struct_stride);
            break;
        case ResourceViewType::TypedBuffer_UAV:
            {
                assert(Format != Format::UNKNOWN);
                view_desc.Format = get_dxgi_format_mapping(format).srv_format;

                uint8_t btBytesPerElement = get_format_info(format).byte_size_per_pixel;
                view_desc.Buffer.FirstElement = resolved_range.byte_offset / btBytesPerElement;
                view_desc.Buffer.NumElements = static_cast<uint32_t>(resolved_range.byte_size / btBytesPerElement);
                break;
            }
        case ResourceViewType::RawBuffer_UAV:
            view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            view_desc.Buffer.FirstElement = resolved_range.byte_offset / 4;
            view_desc.Buffer.NumElements = static_cast<uint32_t>(resolved_range.byte_size / 4);
            view_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;
        default:
			assert(!"invalid Enumeration value");
			return;
        }

        _context->device->CreateUnorderedAccessView(_d3d12_resource.Get(), nullptr, &view_desc, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor_address });
    }
    
    uint32_t DX12Buffer::get_clear_uav_index()
    {
        assert(_desc.bCanHaveUAVs);

        if (_clear_uav_index != INVALID_SIZE_32) return _clear_uav_index;

        // If not found, then create one. 
        create_uav(
            _descriptor_heaps->shader_resource_heap.get_cpu_handle(_clear_uav_index).ptr, 
            Format::R32_UINT,  // Raw buffer format. 
            entire_buffer_range, 
            ResourceViewType::RawBuffer_UAV
        );

        _descriptor_heaps->shader_resource_heap.copy_to_shader_visible_heap(_clear_uav_index);

        return _clear_uav_index;
    }

    DX12StagingTexture::DX12StagingTexture(const DX12Context* context, const TextureDesc& desc, CpuAccessMode CpuAccessMode) :
        _context(context), 
        _desc(desc), 
        _mapped_cpu_access_mode(CpuAccessMode), 
        _d3d12_resource_desc(convert_texture_desc(desc))
    {
    }

    bool DX12StagingTexture::initialize(DX12DescriptorHeaps* descriptor_heaps)
    {
        compute_subresource_offsets();

        BufferDesc buffer_desc{};
        buffer_desc.name = _desc.name;
        buffer_desc.byte_size = get_required_size();
        buffer_desc.struct_stride = 0;
        buffer_desc.cpu_access = _mapped_cpu_access_mode;
        buffer_desc.initial_state = _desc.initial_state;

        DX12Buffer* buffer = new DX12Buffer(_context, descriptor_heaps, buffer_desc);
        if (!buffer->initialize())
        {
            delete buffer;
            return false;
        }
        _buffer = std::unique_ptr<BufferInterface>(buffer);
        return true;
    }


    DX12SliceRegion DX12StagingTexture::get_slice_region(const TextureSlice& texture_slice) const
    {
        DX12SliceRegion ret;
        const uint32_t subresource_index = calculate_texture_subresource(
            texture_slice.mip_level, 
            texture_slice.array_slice, 
            0, 
            _desc.mip_levels,
            _desc.array_size
        );

        assert(subresource_index < _subresource_offsets.size());

        uint64_t size = 0;
        _context->device->GetCopyableFootprints(
            &_d3d12_resource_desc, 
            subresource_index, 
            1, 
            _subresource_offsets[subresource_index], 
            &ret.footprint, 
            nullptr, 
            nullptr, 
            &size
        );

        ret.offset = ret.footprint.Offset;
        ret.size = size;

        return ret;
    }


    void* DX12StagingTexture::map(const TextureSlice& texture_slice, CpuAccessMode cpu_access_mode, HANDLE fence_event, uint64_t* row_pitch)
    {
        if (row_pitch == nullptr ||
            texture_slice.x != 0 || 
            texture_slice.y != 0 || 
            cpu_access_mode == CpuAccessMode::None || 
            _mapped_region.size != 0 || 
            _mapped_access_mode != CpuAccessMode::None
        )
        {
            LOG_ERROR("This staging texture is not allowed to call map().");
            return nullptr;
        } 

        TextureSlice solved_slice = texture_slice.resolve(_desc);

        DX12SliceRegion slice_region = get_slice_region(solved_slice);

        if (last_used_d3d12_fence != nullptr)
        {
            wait_for_fence(last_used_d3d12_fence.Get(), last_used_fence_value, fence_event);
            last_used_d3d12_fence = nullptr;
        }

        D3D12_RANGE d3d12_range;
        if (cpu_access_mode == CpuAccessMode::Read)
        {
            d3d12_range = { slice_region.offset, slice_region.offset + slice_region.size };
        }
        else    // CpuAccessMode::Write
        {
            d3d12_range = { 0, 0 };
        }

        DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(_buffer.get());
        ID3D12Resource* d3d12_buffer = reinterpret_cast<ID3D12Resource*>(dx12_buffer->get_native_object());

        uint8_t* mapped_address = nullptr;
        if (FAILED(d3d12_buffer->Map(0, &d3d12_range, reinterpret_cast<void**>(&mapped_address))))
        {
            LOG_ERROR("Staging texture map failed.");
            return nullptr;
        }

        _mapped_region = slice_region;
        _mapped_access_mode = cpu_access_mode;

        *row_pitch = slice_region.footprint.Footprint.RowPitch;
        return mapped_address + _mapped_region.offset;
    }

    void DX12StagingTexture::unmap()
    {
        assert(_mapped_region.size != 0 && _mapped_access_mode != CpuAccessMode::None);

        D3D12_RANGE d3d12_range;
        if (_mapped_cpu_access_mode == CpuAccessMode::Write)
        {
            d3d12_range = { _mapped_region.offset, _mapped_region.offset + _mapped_region.size };
        }
        else    // CpuAccessMode::Read
        {
            d3d12_range = { 0, 0 };
        }

        DX12Buffer* dx12_buffer = check_cast<DX12Buffer*>(_buffer.get());
        ID3D12Resource* d3d12_buffer = reinterpret_cast<ID3D12Resource*>(dx12_buffer->get_native_object());

        d3d12_buffer->Unmap(0, &d3d12_range);

        _mapped_region.size = 0;
        _mapped_cpu_access_mode = CpuAccessMode::None;
    }

    uint64_t DX12StagingTexture::get_required_size() const
    {
        const uint32_t last_subresource_index = calculate_texture_subresource(
            _desc.mip_levels - 1, 
            _desc.array_size - 1, 
            0, 
            _desc.mip_levels, 
            _desc.array_size
        );
        assert(last_subresource_index < _subresource_offsets.size());

        uint64_t stLastSubresourceSize;
        _context->device->GetCopyableFootprints(
            &_d3d12_resource_desc, 
            last_subresource_index, 
            1, 
            0, 
            nullptr, 
            nullptr, 
            nullptr, 
            &stLastSubresourceSize
        );

        return _subresource_offsets[last_subresource_index] + stLastSubresourceSize;
    }

    void DX12StagingTexture::compute_subresource_offsets()
    {
        const uint32_t last_subresource_index = calculate_texture_subresource(
            _desc.mip_levels - 1, 
            _desc.array_size - 1, 
            0, 
            _desc.mip_levels, 
            _desc.array_size
        );

        const uint32_t subresource_count = last_subresource_index + 1;
        _subresource_offsets.resize(subresource_count);

        uint64_t stBaseOffset = 0;
        for (uint32_t ix = 0; ix < subresource_count; ++ix)
        {
            uint64_t subresource_size;
            _context->device->GetCopyableFootprints(&_d3d12_resource_desc, ix, 1, 0, nullptr, nullptr, nullptr, &subresource_size);

            _subresource_offsets[ix] = stBaseOffset;
            stBaseOffset += subresource_size;
            stBaseOffset = align(stBaseOffset, static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT));
        }
    }


    DX12Sampler::DX12Sampler(const DX12Context* context, const SamplerDesc& desc) :
        _context(context), _desc(desc)
    {
    }

    bool DX12Sampler::initialize()
    {
        uint32_t reduction_type = convert_sampler_reduction_type(_desc.reduction_type);
        if (_desc.max_anisotropy > 1.0f)
        {
            _d3d12_sampler_desc.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(reduction_type);
        }
        else 
        {
            _d3d12_sampler_desc.Filter = D3D12_ENCODE_BASIC_FILTER(
                _desc.min_filter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                _desc.max_filter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                _desc.mip_filter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                reduction_type
            );
        }

        _d3d12_sampler_desc.AddressU = convert_sampler_address_mode(_desc.address_u);
        _d3d12_sampler_desc.AddressV = convert_sampler_address_mode(_desc.address_v);
        _d3d12_sampler_desc.AddressW = convert_sampler_address_mode(_desc.address_w);

        _d3d12_sampler_desc.MipLODBias = _desc.mip_bias;
        _d3d12_sampler_desc.MaxAnisotropy = std::max(static_cast<uint32_t>(_desc.max_anisotropy), 1u);
        _d3d12_sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
        _d3d12_sampler_desc.BorderColor[0] = _desc.border_color.r;
        _d3d12_sampler_desc.BorderColor[1] = _desc.border_color.g;
        _d3d12_sampler_desc.BorderColor[2] = _desc.border_color.b;
        _d3d12_sampler_desc.BorderColor[3] = _desc.border_color.a;
        _d3d12_sampler_desc.MinLOD = 0;
        _d3d12_sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;

        return true;
    }

    
    void DX12Sampler::create_descriptor(uint64_t descriptor_address) const
    {
        _context->device->CreateSampler(&_d3d12_sampler_desc, D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor_address });
    }
}