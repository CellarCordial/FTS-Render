#ifndef DYNAMIC_RHI_BINDING_H
#define DYNAMIC_RHI_BINDING_H

#include "../core/tools/stack_array.h"
#include "format.h"
#include "resource.h"
#include "shader.h"
#include <memory>


namespace fantasy
{
    enum class DescriptorHeapType : uint8_t
    {
        RenderTargetView,
        DepthStencilView,
        ShaderResourceView,
        Sampler
    };

    enum class ResourceViewType : uint16_t
    {
        None,

        Texture_RTV,
        Texture_DSV,
        Texture_SRV,
        Texture_UAV,

        TypedBuffer_SRV,
        TypedBuffer_UAV,
        StructuredBuffer_SRV,
        StructuredBuffer_UAV,
        RawBuffer_SRV,
        RawBuffer_UAV,
        
        ConstantBuffer,
        VolatileConstantBuffer,

        AccelStruct,

        Sampler,
        PushConstants,

        Count
    };

    struct BindingLayoutItem
    {
        union
        {
            uint32_t slot; 
            uint32_t register_space;
        };

        ResourceViewType type   : 16;
        uint16_t size           : 16;

        static BindingLayoutItem create_bindless_texture_srv(uint32_t register_space = 0)
        {
            BindingLayoutItem ret;
            ret.register_space = register_space;
            ret.type = ResourceViewType::Texture_SRV;
            return ret;
        }

        static BindingLayoutItem create_texture_srv(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::Texture_SRV;
            return ret;
        }

        static BindingLayoutItem create_texture_uav(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::Texture_UAV;
            return ret;
        }

        static BindingLayoutItem create_typed_buffer_srv(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::TypedBuffer_SRV;
            return ret;
        }

        static BindingLayoutItem create_typed_buffer_uav(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::TypedBuffer_UAV;
            return ret;
        }

        static BindingLayoutItem create_constant_buffer(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::ConstantBuffer;
            return ret;
        }

        static BindingLayoutItem create_volatile_constant_buffer(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::VolatileConstantBuffer;
            return ret;
        }

        static BindingLayoutItem create_sampler(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::Sampler;
            return ret;
        }

        static BindingLayoutItem create_structured_buffer_srv(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::StructuredBuffer_SRV;
            return ret;
        }

        static BindingLayoutItem create_structured_buffer_uav(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::StructuredBuffer_UAV;
            return ret;
        }

        static BindingLayoutItem create_raw_buffer_srv(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::RawBuffer_SRV;
            return ret;
        }

        static BindingLayoutItem create_raw_buffer_uav(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::RawBuffer_UAV;
            return ret;
        }

        static BindingLayoutItem create_push_constants(uint32_t slot, uint16_t size)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::PushConstants;
            ret.size = size;
            return ret;
        }

        static BindingLayoutItem create_accel_struct(uint32_t slot)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::AccelStruct;
            return ret;
        }
    };

    using BindingLayoutItemArray = StackArray<BindingLayoutItem, MAX_BINDINGS_PER_LAYOUT>;

    struct BindingLayoutDesc
    {
        ShaderType shader_visibility = ShaderType::All;
        
        uint32_t register_space = 0;
        BindingLayoutItemArray binding_layout_items;
    };

    struct BindlessLayoutDesc
    {
        ShaderType shader_visibility = ShaderType::All;
        
        uint32_t first_slot = 0;
        BindingLayoutItemArray binding_layout_items;
    };

    
    struct BindingLayoutInterface
    {
        virtual const BindingLayoutDesc& get_binding_desc() const = 0;
        virtual const BindlessLayoutDesc& get_bindless_desc() const = 0;
        virtual bool is_binding_less() const = 0;

		virtual ~BindingLayoutInterface() = default;
    };


    struct BindingSetItem
    {
        std::shared_ptr<ResourceInterface> resource;

        uint32_t slot = 0;
        Format format = Format::UNKNOWN;
        ResourceViewType type = ResourceViewType::None;

        union 
        {
            TextureSubresourceSet subresource;
            BufferRange range = { 0, 0 };
        };

        static BindingSetItem create_texture_srv(
            uint32_t slot,
            std::shared_ptr<TextureInterface> texture,
            TextureSubresourceSet subresource = TextureSubresourceSet{},
            Format format = Format::UNKNOWN
        )
        {
            BindingSetItem ret;
            ret.resource = texture;
            ret.type = ResourceViewType::Texture_SRV;
            ret.slot = slot;
            ret.subresource = subresource;
            ret.format = format;
            return ret;
        }

        static BindingSetItem create_texture_uav(
            uint32_t slot,
            std::shared_ptr<TextureInterface> texture,
            TextureSubresourceSet subresource = TextureSubresourceSet{},
            Format format = Format::UNKNOWN
        )
        {
			BindingSetItem ret;
            ret.resource = texture;
            ret.type = ResourceViewType::Texture_UAV;
            ret.slot = slot;
            ret.subresource = subresource;
            ret.format = format;
            return ret;
        }

        static BindingSetItem create_typed_buffer_srv(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer,
            Format format = Format::UNKNOWN
        )
        {
            BindingSetItem ret;
            ret.resource = buffer;
            ret.type = ResourceViewType::TypedBuffer_SRV;
            ret.slot = slot;
            ret.range = BufferRange{
                .byte_offset = 0,
                .byte_size = buffer->get_desc().byte_size
            };
            ret.format = format;
            return ret;
        }

        static BindingSetItem create_typed_buffer_uav(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer,
            Format format = Format::UNKNOWN
        )
        {
			BindingSetItem ret;
            ret.resource = buffer;
            ret.type = ResourceViewType::TypedBuffer_UAV;
            ret.slot = slot;
            ret.range = BufferRange{
                .byte_offset = 0,
                .byte_size = buffer->get_desc().byte_size
            };
            ret.format = format;
            return ret;
        }

        static BindingSetItem create_constant_buffer(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            BindingSetItem ret;
            ret.resource = buffer;
            ret.type = ResourceViewType::ConstantBuffer;
            ret.slot = slot;
            ret.range = BufferRange{
                .byte_offset = 0,
                .byte_size = buffer->get_desc().byte_size
            };
            return ret;
        }

        static BindingSetItem create_volatile_constant_buffer(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            BindingSetItem ret;
            ret.resource = buffer;
            ret.type = ResourceViewType::VolatileConstantBuffer;
            ret.slot = slot;
            ret.range = BufferRange{
                .byte_offset = 0,
                .byte_size = buffer->get_desc().byte_size
            };
            return ret;
        }

        static BindingSetItem create_structured_buffer_srv(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            BindingSetItem ret;
            ret.resource = buffer;
            ret.type = ResourceViewType::StructuredBuffer_SRV;
            ret.slot = slot;
            ret.range = BufferRange{
                .byte_offset = 0,
                .byte_size = buffer->get_desc().byte_size
            };
            return ret;
        }

        static BindingSetItem create_structured_buffer_uav(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            BindingSetItem ret;
            ret.resource = buffer;
            ret.type = ResourceViewType::StructuredBuffer_UAV;
            ret.slot = slot;
            ret.range = BufferRange{
                .byte_offset = 0,
                .byte_size = buffer->get_desc().byte_size
            };
            return ret;
        }

        static BindingSetItem create_raw_buffer_srv(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            BindingSetItem ret;
            ret.resource = buffer;
            ret.type = ResourceViewType::RawBuffer_SRV;
            ret.slot = slot;
            ret.range = BufferRange{
                .byte_offset = 0,
                .byte_size = buffer->get_desc().byte_size
            };
            return ret;
        }

        static BindingSetItem create_raw_buffer_uav(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            BindingSetItem ret;
            ret.resource = buffer;
            ret.type = ResourceViewType::RawBuffer_UAV;
            ret.slot = slot;
            ret.range = BufferRange{
                .byte_offset = 0,
                .byte_size = buffer->get_desc().byte_size
            };
            return ret;
        }

        static BindingSetItem create_push_constants(
            uint32_t slot,
            uint32_t size
        )
        {
            BindingSetItem ret;
            ret.resource = nullptr;
            ret.type = ResourceViewType::PushConstants;
            ret.slot = slot;
            ret.range = { 0, size };
            return ret;
        }

        static BindingSetItem create_sampler(
            uint32_t slot,
            std::shared_ptr<SamplerInterface> sampler
        )
        {
            BindingSetItem ret;
            ret.resource = sampler;
            ret.type = ResourceViewType::Sampler;
            ret.slot = slot;
            return ret;
        }
    };

    using BindingSetItemArray = StackArray<BindingSetItem, MAX_BINDINGS_PER_LAYOUT>;


    struct BindingSetDesc
    {
        BindingSetItemArray binding_items;
    };

    struct BindingSetInterface : public ResourceInterface
    {
        virtual const BindingSetDesc& get_desc() const = 0;
        virtual BindingLayoutInterface* get_layout() const = 0;
        virtual bool is_bindless() const = 0;

		virtual ~BindingSetInterface() = default;
    };

    struct BindlessSetInterface : public BindingSetInterface
    {
        virtual uint32_t get_capacity() const = 0;
		virtual bool resize(uint32_t new_size, bool keep_contents) = 0;
		virtual bool set_slot(const BindingSetItem& item) = 0;

		virtual ~BindlessSetInterface() = default;
    };
    
}







#endif