#include "dx12_resource.h"
#include "dx12_convert.h"
#include "dx12_forward.h"
#include <combaseapi.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgiformat.h>
#include <intsafe.h>
#include <memory>
#include <tuple>
#include <utility>
#include <winerror.h>
#include "../forward.h"
#include "../../core/tools/check_cast.h"

namespace fantasy 
{
    DX12Texture::DX12Texture(const DX12Context* context, DX12DescriptorManager* descriptor_heaps, const TextureDesc& desc_) :
        _context(context), 
        _descriptor_manager(descriptor_heaps), 
        desc(desc_), 
        d3d12_resource_desc(convert_texture_desc(desc_))
    {
    }

    DX12Texture::~DX12Texture() noexcept
    {
        for (const auto& iter : view_cache)
        {
            const auto& [subresource, format, view_type] = iter.first;
            switch (view_type) 
            {
            case ResourceViewType::Texture_RTV: _descriptor_manager->render_target_heap.release_descriptor(iter.second); break;
            case ResourceViewType::Texture_DSV: _descriptor_manager->depth_stencil_heap.release_descriptor(iter.second); break;
            default:
                _descriptor_manager->shader_resource_heap.release_descriptor(iter.second); break;
            }
        }

        for (auto index : _mip_uav_cache_for_clear) _descriptor_manager->shader_resource_heap.release_descriptor(index);
    }

    bool DX12Texture::initialize()
    {
        if (desc.is_virtual) return true;

        D3D12_HEAP_PROPERTIES d3d12_heap_properties{};
        d3d12_heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        d3d12_heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        d3d12_heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        d3d12_heap_properties.CreationNodeMask = 1;
        d3d12_heap_properties.VisibleNodeMask = 1;

        D3D12_CLEAR_VALUE d3d12_clear_value = convert_clear_value(desc);

        ReturnIfFalse(SUCCEEDED(_context->device->CreateCommittedResource(
            &d3d12_heap_properties, 
            D3D12_HEAP_FLAG_NONE, 
            &d3d12_resource_desc, 
            D3D12_RESOURCE_STATE_COMMON, 
            desc.use_clear_value ? &d3d12_clear_value : nullptr, 
            IID_PPV_ARGS(d3d12_resource.GetAddressOf())
        )));
        
		d3d12_resource->SetName(std::wstring(desc.name.begin(), desc.name.end()).c_str());

        
        if (desc.allow_unordered_access)
        {
            _mip_uav_cache_for_clear.resize(desc.mip_levels);
            std::fill(_mip_uav_cache_for_clear.begin(), _mip_uav_cache_for_clear.end(), INVALID_SIZE_32);
        }

        return true;
    }

    const TextureDesc& DX12Texture::get_desc() const
    { 
        return desc; 
    }

    void* DX12Texture::get_native_object()
    { 
        return d3d12_resource.Get(); 
    }

    MemoryRequirements DX12Texture::get_memory_requirements()
    {
        D3D12_RESOURCE_ALLOCATION_INFO allocation_info = _context->device->GetResourceAllocationInfo(0, 1, &d3d12_resource_desc);

        return MemoryRequirements{ allocation_info.Alignment, allocation_info.SizeInBytes };
    }


    bool DX12Texture::bind_memory(std::shared_ptr<HeapInterface> heap_, uint64_t offset)
    {
        if (heap_ == nullptr || !desc.is_virtual || d3d12_resource == nullptr) return false;

        heap = heap_;

        auto dx12_heap = check_cast<DX12Heap>(heap);

        D3D12_CLEAR_VALUE d3d12_clear_value = convert_clear_value(desc);

        ReturnIfFalse(SUCCEEDED(_context->device->CreatePlacedResource(
            dx12_heap->d3d12_heap.Get(), 
            offset, 
            &d3d12_resource_desc, 
            D3D12_RESOURCE_STATE_COMMON, 
            desc.use_clear_value ? &d3d12_clear_value : nullptr,  
            IID_PPV_ARGS(d3d12_resource.GetAddressOf())
        )));

        if (desc.allow_unordered_access)
        {
            _mip_uav_cache_for_clear.resize(desc.mip_levels);
            std::fill(_mip_uav_cache_for_clear.begin(), _mip_uav_cache_for_clear.end(), INVALID_SIZE_32);
        }

        return true;
    }

    uint32_t DX12Texture::get_view_index(ResourceViewType view_type, const TextureSubresourceSet& subresource, Format format_)
    {
        uint32_t descriptor_index = INVALID_SIZE_32;

        Format format = format_ == Format::UNKNOWN ? desc.format : format_;
        
        auto view_key = std::make_tuple(subresource, format, view_type);
        auto iter = view_cache.find(view_key);
        if (iter == view_cache.end())
        {
            switch (view_type)
            {
            case ResourceViewType::Texture_SRV:
                {
                    descriptor_index = _descriptor_manager->shader_resource_heap.allocate_descriptor();
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor = 
                        _descriptor_manager->shader_resource_heap.get_cpu_handle(descriptor_index);

                    create_srv(d3d12_cpu_descriptor, subresource, format);
                }
                break;
            case ResourceViewType::Texture_UAV:
                {
                    descriptor_index = _descriptor_manager->shader_resource_heap.allocate_descriptor();
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor = 
                        _descriptor_manager->shader_resource_heap.get_cpu_handle(descriptor_index);

                    create_uav(d3d12_cpu_descriptor, subresource, format);
                }
                break;
            case ResourceViewType::Texture_RTV:
                {
                    descriptor_index = _descriptor_manager->render_target_heap.allocate_descriptor();
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor = 
                        _descriptor_manager->render_target_heap.get_cpu_handle(descriptor_index);

                    create_rtv(d3d12_cpu_descriptor, subresource, format);
                }
                break;
            case ResourceViewType::Texture_DSV:
                {
                    descriptor_index = _descriptor_manager->depth_stencil_heap.allocate_descriptor();
                    
                    const D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor = 
                        _descriptor_manager->depth_stencil_heap.get_cpu_handle(descriptor_index);

                    create_dsv(d3d12_cpu_descriptor, subresource, format);
                }
                break;
            default:
                assert(!"invalid enum.");
                return INVALID_SIZE_32;
            }

            view_cache.emplace(view_key, descriptor_index);
        }
        else
        {
            descriptor_index = iter->second;
        }

        return descriptor_index;
    }

    void DX12Texture::create_srv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const TextureSubresourceSet& subresource, Format format)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Format = format == Format::UNKNOWN ? d3d12_resource_desc.Format : get_dxgi_format_mapping(format).srv_format;
        view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        switch (desc.dimension)
        {
        case TextureDimension::Texture1D: 
            view_desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipLevels       = subresource.mip_level_count;
            view_desc.Texture1D.MostDetailedMip = subresource.base_mip_level;
            break;
        case TextureDimension::Texture1DArray: 
            view_desc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            view_desc.Texture1DArray.ArraySize       = subresource.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = subresource.base_array_slice;
            view_desc.Texture1DArray.MipLevels       = subresource.mip_level_count;
            view_desc.Texture1DArray.MostDetailedMip = subresource.base_mip_level;
            break;
        case TextureDimension::Texture2D: 
            view_desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
            view_desc.Texture2D.MipLevels       = subresource.mip_level_count;
            view_desc.Texture2D.MostDetailedMip = subresource.base_mip_level;
            view_desc.Texture2D.PlaneSlice      = 0;
            break;
        case TextureDimension::Texture2DArray: 
            view_desc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture2DArray.ArraySize       = subresource.array_slice_count;
            view_desc.Texture2DArray.FirstArraySlice = subresource.base_array_slice;
            view_desc.Texture2DArray.MipLevels       = subresource.mip_level_count;
            view_desc.Texture2DArray.MostDetailedMip = subresource.base_mip_level;
            view_desc.Texture2DArray.PlaneSlice      = 0;
            break;
        case TextureDimension::TextureCube: 
            view_desc.ViewDimension               = D3D12_SRV_DIMENSION_TEXTURECUBE;
            view_desc.TextureCube.MipLevels       = subresource.mip_level_count;
            view_desc.TextureCube.MostDetailedMip = subresource.base_mip_level;
            break;
        case TextureDimension::TextureCubeArray: 
            view_desc.ViewDimension                     = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            view_desc.TextureCubeArray.MipLevels        = subresource.mip_level_count;
            view_desc.TextureCubeArray.MostDetailedMip  = subresource.base_mip_level;
            view_desc.TextureCubeArray.First2DArrayFace = subresource.base_array_slice;
            view_desc.TextureCubeArray.NumCubes         = subresource.array_slice_count / 6;
            break;
        case TextureDimension::Texture3D: 
            view_desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE3D;
            view_desc.Texture3D.MipLevels       = subresource.mip_level_count;
            view_desc.Texture3D.MostDetailedMip = subresource.base_mip_level;
            break;
        default: 
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateShaderResourceView(
            d3d12_resource.Get(), 
            &view_desc, 
            d3d12_cpu_descriptor
        );
    }

    void DX12Texture::create_uav(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const TextureSubresourceSet& subresource, Format format)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC view_desc{};
        view_desc.Format = format == Format::UNKNOWN ? d3d12_resource_desc.Format : get_dxgi_format_mapping(format).srv_format;

        switch (desc.dimension)
        {
        case TextureDimension::Texture1D:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture1DArray:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            view_desc.Texture1DArray.ArraySize = subresource.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = subresource.base_array_slice;
            view_desc.Texture1DArray.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture2D:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            view_desc.Texture2D.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture2DArray.ArraySize = subresource.array_slice_count;
            view_desc.Texture2DArray.FirstArraySlice = subresource.base_array_slice;
            view_desc.Texture2DArray.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture3D:
            view_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            view_desc.Texture3D.FirstWSlice = 0;
            view_desc.Texture3D.WSize = desc.depth;
            view_desc.Texture3D.MipSlice = subresource.base_mip_level;
            break;
        default:
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateUnorderedAccessView(
            d3d12_resource.Get(), 
            nullptr,
            &view_desc, 
            d3d12_cpu_descriptor
        );
    }

    void DX12Texture::create_rtv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const TextureSubresourceSet& subresource, Format format)
    {
        D3D12_RENDER_TARGET_VIEW_DESC view_desc{};
        view_desc.Format = format == Format::UNKNOWN ? d3d12_resource_desc.Format : get_dxgi_format_mapping(format).rtv_dsv_format;

        switch (desc.dimension)
        {
        case TextureDimension::Texture1D:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture1DArray:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            view_desc.Texture1DArray.ArraySize = subresource.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = subresource.base_array_slice;
            view_desc.Texture1DArray.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture2D:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            view_desc.Texture2D.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture2DArray.ArraySize = subresource.array_slice_count;
            view_desc.Texture2DArray.FirstArraySlice = subresource.base_array_slice;
            view_desc.Texture2DArray.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture3D:
            view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
            view_desc.Texture3D.FirstWSlice = subresource.base_array_slice;
            view_desc.Texture3D.WSize = subresource.array_slice_count;
            view_desc.Texture3D.MipSlice = subresource.base_mip_level;
            break;
        default:
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateRenderTargetView(
            d3d12_resource.Get(), 
            &view_desc, 
            d3d12_cpu_descriptor
        );
    }

    void DX12Texture::create_dsv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const TextureSubresourceSet& subresource, Format format)
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC view_desc{};
        view_desc.Format = format == Format::UNKNOWN ? d3d12_resource_desc.Format : get_dxgi_format_mapping(format).rtv_dsv_format;
        view_desc.Flags = D3D12_DSV_FLAG_NONE;

        FormatInfo format_info = get_format_info(desc.format);

        switch (desc.dimension)
        {
        case TextureDimension::Texture1D:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture1DArray:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            view_desc.Texture1DArray.ArraySize = subresource.array_slice_count;
            view_desc.Texture1DArray.FirstArraySlice = subresource.base_array_slice;
            view_desc.Texture1DArray.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture2D:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            view_desc.Texture2D.MipSlice = subresource.base_mip_level;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture2DArray.ArraySize = subresource.array_slice_count;
            view_desc.Texture2DArray.FirstArraySlice = subresource.base_array_slice;
            view_desc.Texture2DArray.MipSlice = subresource.base_mip_level;
            break;
        default:
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateDepthStencilView(
            d3d12_resource.Get(), 
            &view_desc, 
            d3d12_cpu_descriptor
        );
    }


    DX12Buffer::DX12Buffer(const DX12Context* context, DX12DescriptorManager* descriptor_heaps, const BufferDesc& desc_) :
        _context(context), _descriptor_manager(descriptor_heaps), desc(desc_)
    {
    }

    DX12Buffer::~DX12Buffer() noexcept
    {
        for (const auto& iter : view_cache)
        {
            _descriptor_manager->shader_resource_heap.release_descriptor(iter.second);
        }
    }

    bool DX12Buffer::initialize()
    {
        if (desc.is_constant_buffer) 
        {
            desc.byte_size = align(desc.byte_size, static_cast<uint64_t>(CONSTANT_BUFFER_OFFSET_SIZE_ALIGMENT));
        }

        d3d12_resource_desc = convert_buffer_desc(desc);

        if (desc.is_virtual) return true;

        D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
        if (desc.cpu_access == CpuAccessMode::Read)                             heap_type = D3D12_HEAP_TYPE_READBACK;
        if (desc.cpu_access == CpuAccessMode::Write || desc.is_constant_buffer) heap_type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_HEAP_PROPERTIES d3d12_heap_properties{};
        d3d12_heap_properties.Type = heap_type;
        d3d12_heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        d3d12_heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        d3d12_heap_properties.CreationNodeMask = 0;
        d3d12_heap_properties.VisibleNodeMask = 0;
        
        ReturnIfFalse(SUCCEEDED(_context->device->CreateCommittedResource(
            &d3d12_heap_properties, 
            D3D12_HEAP_FLAG_NONE, 
            &d3d12_resource_desc, 
            get_buffer_initial_state(desc),
            nullptr, 
            IID_PPV_ARGS(d3d12_resource.GetAddressOf())
        )));

        d3d12_resource->SetName(std::wstring(desc.name.begin(), desc.name.end()).c_str());

        return true;
    }
    
    const BufferDesc& DX12Buffer::get_desc() const
    { 
        return desc; 
    }
    
    void* DX12Buffer::map(CpuAccessMode cpu_access)
    {
        // 不要从与 D3D12_HEAP_TYPE_UPLOAD 的堆关联的资源读取 CPU, 
        // 故 cpu_access == CpuAccessMode::Write 时设置 range 为 { 0, 0 },
        // D3D12_RANGE End 小于或等于 Begin 的范围，指定 CPU 不会读取任何数据是有效的.

        D3D12_RANGE range;
        if (cpu_access == CpuAccessMode::Read)      range = { 0, desc.byte_size };
        else if(cpu_access == CpuAccessMode::Write) range = { 0, 0 };

        void* mapped_address = nullptr;
        if (FAILED(d3d12_resource->Map(0, &range, &mapped_address)))
        {
            LOG_ERROR("Map buffer failed. ");
            return nullptr;
        }

        return mapped_address;
    }

    void DX12Buffer::unmap()
    {
        d3d12_resource->Unmap(0, nullptr);
    }
    
    MemoryRequirements DX12Buffer::get_memory_requirements()
    {
        D3D12_RESOURCE_ALLOCATION_INFO d3d12_alloc_info = _context->device->GetResourceAllocationInfo(1, 1, &d3d12_resource_desc);

        return MemoryRequirements{ 
            d3d12_alloc_info.Alignment, 
            d3d12_alloc_info.SizeInBytes 
        };
    }

    bool DX12Buffer::bind_memory(std::shared_ptr<HeapInterface> heap_, uint64_t offset)
    {
        if (heap_ == nullptr || d3d12_resource == nullptr || desc.is_virtual) return false;
        
        heap = heap_;
        
        auto dx12_heap = check_cast<DX12Heap>(heap_);

        ReturnIfFalse(SUCCEEDED(_context->device->CreatePlacedResource(
            dx12_heap->d3d12_heap.Get(), 
            offset, 
            &d3d12_resource_desc, 
            get_buffer_initial_state(desc), 
            nullptr, 
            IID_PPV_ARGS(d3d12_resource.GetAddressOf())
        )));

        return true;
    }

    
	uint32_t DX12Buffer::get_view_index(ResourceViewType view_type, const BufferRange& range, Format format_)
	{
        // Buffer 自行管理 descriptor.

        auto view_key = std::make_tuple(range, format_, view_type);
        auto iter = view_cache.find(view_key);

        Format format = format_ == Format::UNKNOWN ? desc.format : format_;

        if (iter != view_cache.end())
        {
            return iter->second;
        }
        else
        {
            uint32_t descriptor_index = _descriptor_manager->shader_resource_heap.allocate_descriptor();

            const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle =
                        _descriptor_manager->shader_resource_heap.get_cpu_handle(descriptor_index);

            switch (view_type)
            {
            case ResourceViewType::TypedBuffer_SRV:
            case ResourceViewType::StructuredBuffer_SRV:
            case ResourceViewType::RawBuffer_SRV:
                create_srv(cpu_descriptor_handle, range, view_type, format);
                break;
            case ResourceViewType::TypedBuffer_UAV:
            case ResourceViewType::StructuredBuffer_UAV:
            case ResourceViewType::RawBuffer_UAV:
                create_uav(cpu_descriptor_handle, range, view_type, format);
                break;
            default:
                assert(!"invalid enum.");
            }

            view_cache.emplace(view_key, descriptor_index);
            return descriptor_index;
        }
	}
    
	void DX12Buffer::create_cbv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const BufferRange& range)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc;
        view_desc.BufferLocation = d3d12_resource->GetGPUVirtualAddress() + range.byte_offset;
        view_desc.SizeInBytes = align(static_cast<uint32_t>(range.byte_size), CONSTANT_BUFFER_OFFSET_SIZE_ALIGMENT);

        _context->device->CreateConstantBufferView(&view_desc, d3d12_cpu_descriptor);
    }

    void DX12Buffer::create_srv(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const BufferRange& range, ResourceViewType ResourceViewType, Format format)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

        switch (ResourceViewType)
        {
        case ResourceViewType::StructuredBuffer_SRV:
            view_desc.Format = DXGI_FORMAT_UNKNOWN;
            view_desc.Buffer.FirstElement = range.byte_offset / desc.struct_stride;
            view_desc.Buffer.NumElements = static_cast<uint32_t>(range.byte_size / desc.struct_stride);
            view_desc.Buffer.StructureByteStride = desc.struct_stride;
            view_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            break;
        case ResourceViewType::TypedBuffer_SRV:
            {
                uint8_t size_per_element = get_format_info(desc.format).size;

                view_desc.Format = format == Format::UNKNOWN ? d3d12_resource_desc.Format : get_dxgi_format_mapping(format).srv_format;
                view_desc.Buffer.FirstElement = range.byte_offset / size_per_element;
                view_desc.Buffer.NumElements = static_cast<uint32_t>(range.byte_size / size_per_element);
                view_desc.Buffer.StructureByteStride = 0;
                view_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
                break;
            }
        case ResourceViewType::RawBuffer_SRV:
            view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            view_desc.Buffer.FirstElement = range.byte_offset / 4;
            view_desc.Buffer.NumElements = static_cast<uint32_t>(range.byte_size / 4);
            view_desc.Buffer.StructureByteStride = 0;
            view_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            break;
        default:
            assert(!"invalid Enumeration value");
            return;
        }

        _context->device->CreateShaderResourceView(d3d12_resource.Get(), &view_desc, d3d12_cpu_descriptor);
    }
    
    void DX12Buffer::create_uav(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor, const BufferRange& range, ResourceViewType view_type, Format format)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC view_desc{};
        view_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        
        switch (view_type) 
        {
        case ResourceViewType::StructuredBuffer_UAV:
            view_desc.Format = DXGI_FORMAT_UNKNOWN;
            view_desc.Buffer.FirstElement = range.byte_offset / desc.struct_stride;
            view_desc.Buffer.NumElements = static_cast<uint32_t>(range.byte_size / desc.struct_stride);
            view_desc.Buffer.StructureByteStride = desc.struct_stride;
            view_desc.Buffer.CounterOffsetInBytes = 0;
            view_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            break;
        case ResourceViewType::TypedBuffer_UAV:
            {
                uint8_t size_per_element = get_format_info(desc.format).size;

                view_desc.Format = d3d12_resource_desc.Format;
                view_desc.Buffer.FirstElement = range.byte_offset / size_per_element;
                view_desc.Buffer.NumElements = static_cast<uint32_t>(range.byte_size / size_per_element);
                view_desc.Buffer.StructureByteStride = 0;
                view_desc.Buffer.CounterOffsetInBytes = 0;
                view_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
                break;
            }
        case ResourceViewType::RawBuffer_UAV:
            view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            view_desc.Buffer.FirstElement = range.byte_offset / 4;
            view_desc.Buffer.NumElements = static_cast<uint32_t>(range.byte_size / 4);
            view_desc.Buffer.StructureByteStride = 0;
            view_desc.Buffer.CounterOffsetInBytes = 0;
            view_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;
        default:
			assert(!"invalid Enumeration value");
			return;
        }

        _context->device->CreateUnorderedAccessView(d3d12_resource.Get(), nullptr, &view_desc, d3d12_cpu_descriptor);
    }

    DX12StagingTexture::DX12StagingTexture(const DX12Context* context, const TextureDesc& desc_, CpuAccessMode cpu_access_mode) :
        _context(context), 
        desc(desc_), 
        access_mode(cpu_access_mode)
    {
    }

    bool DX12StagingTexture::initialize(DX12DescriptorManager* descriptor_heaps)
    {
        ReturnIfFalse(access_mode != CpuAccessMode::None);

        const uint32_t last_subresource_index = calculate_texture_subresource(
            desc.mip_levels - 1,
            desc.array_size - 1,
            desc.mip_levels
        );

        _d3d12_resource_desc = convert_texture_desc(desc);

        subresource_offsets.resize(last_subresource_index + 1);

        uint32_t offset = 0;
        for (uint32_t ix = 0; ix < subresource_offsets.size(); ++ix)
        {
            subresource_offsets[ix] = offset;
            
            uint64_t subresource_size;
            _context->device->GetCopyableFootprints(
                &_d3d12_resource_desc, 
                ix, 
                1, 
                0, 
                nullptr, 
                nullptr, 
                nullptr, 
                &subresource_size
            );

            offset += subresource_size;
            offset = align(offset, static_cast<uint32_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT));
        }

        BufferDesc buffer_desc{};
        buffer_desc.name = desc.name;
        buffer_desc.byte_size = offset;
        buffer_desc.cpu_access = access_mode;
        buffer_desc.allow_shader_resource = false;

        DX12Buffer* buffer = new DX12Buffer(_context, descriptor_heaps, buffer_desc);
        if (!buffer->initialize())
        {
            delete buffer;
            return false;
        }
        _buffer = std::unique_ptr<BufferInterface>(buffer);

        return true;
    }


    DX12SliceRegion DX12StagingTexture::get_slice_region(uint32_t mip_level, uint32_t array_slice) const
    {
        const uint32_t subresource_index = calculate_texture_subresource(
            mip_level, 
            array_slice, 
            desc.mip_levels
        );

        if (subresource_index >= subresource_offsets.size())
        {
            return DX12SliceRegion{
                .offset = INVALID_SIZE_32,
                .size = INVALID_SIZE_64
            };
        }

        uint64_t size;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT d3d12_footprint;
        _context->device->GetCopyableFootprints(
            &_d3d12_resource_desc, 
            subresource_index, 
            1, 
            subresource_offsets[subresource_index], 
            &d3d12_footprint, 
            nullptr, 
            nullptr, 
            &size
        );

        return DX12SliceRegion{
            .offset = d3d12_footprint.Offset,
            .size = size,
            .d3d12_foot_print = d3d12_footprint
        };
    }

    const TextureDesc& DX12StagingTexture::get_desc() const
    { 
        return desc; 
    }

    void* DX12StagingTexture::map(const TextureSlice& texture_slice, CpuAccessMode cpu_access_mode, uint64_t* row_pitch)
    {
        if (
            row_pitch == nullptr ||
            texture_slice.x != 0 || 
            texture_slice.y != 0 || 
            cpu_access_mode == CpuAccessMode::None
        )
        {
            LOG_ERROR("Invalid staging texture slice.");
            return nullptr;
        } 

        DX12SliceRegion slice_region = get_slice_region(texture_slice.mip_level, texture_slice.array_slice);

        *row_pitch = slice_region.d3d12_foot_print.Footprint.RowPitch;

        if (cpu_access_mode == CpuAccessMode::Read)
        {
            _d3d12_mapped_range = { slice_region.offset, slice_region.offset + slice_region.size };
        }
        else if ( cpu_access_mode == CpuAccessMode::Write)
        {
            _d3d12_mapped_range = { 0, 0 };
        }

        uint8_t* mapped_address = nullptr;
        if (
            FAILED(check_cast<DX12Buffer*>(_buffer.get())->d3d12_resource->Map(
                0, 
                &_d3d12_mapped_range, 
                reinterpret_cast<void**>(&mapped_address)
            ))
        )
        {
            LOG_ERROR("Staging texture map failed.");
            return nullptr;
        }

 
        return mapped_address + slice_region.offset;
    }

    void DX12StagingTexture::unmap()
    {
        assert(_d3d12_mapped_range.End - _d3d12_mapped_range.Begin != 0);

        check_cast<DX12Buffer*>(_buffer.get())->d3d12_resource->Unmap(0, &_d3d12_mapped_range);

        _d3d12_mapped_range = { INVALID_SIZE_64, INVALID_SIZE_64 };
    }


    DX12Sampler::DX12Sampler(const DX12Context* context, const SamplerDesc& desc_) :
        _context(context), desc(desc_), d3d12_sampler_desc(convert_sampler_desc(desc_))
    {
    }

    bool DX12Sampler::initialize()
    {
        return true;
    }

    void DX12Sampler::create_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE d3d12_cpu_descriptor) const
    {
        _context->device->CreateSampler(&d3d12_sampler_desc, d3d12_cpu_descriptor);
    }
}