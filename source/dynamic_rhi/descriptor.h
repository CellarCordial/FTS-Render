#ifndef RHI_DESCRIPTOR_H
#define RHI_DESCRIPTOR_H

#include "../core/tools/stack_array.h"
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

    enum class ResourceViewType : uint8_t
    {
        None,

        Texture_SRV,
        Texture_UAV,
        TypedBuffer_SRV,
        TypedBuffer_UAV,
        StructuredBuffer_SRV,
        StructuredBuffer_UAV,
        RawBuffer_SRV,
        RawBuffer_UAV,
        ConstantBuffer,         // static
        VolatileConstantBuffer, // volatile

        Sampler,
        PushConstants,

        Count
    };

    // 确保 BindingLayoutItem 大小为 8 字节
    struct BindingLayoutItem
    {
        union
        {
            uint32_t slot; 
            uint32_t register_space;     /**< For bindless layout. */
        };

        ResourceViewType type  : 8;
        uint8_t pad         : 8;
        uint16_t size        : 16;   /**< For push constants. */


        static BindingLayoutItem create_bindless_srv(UINT32 dwRegisterSpace = 0)
        {
			BindingLayoutItem Ret;
            // 只要是个 SRV 就行.
			Ret.type = ResourceViewType::Texture_SRV;
            Ret.register_space = dwRegisterSpace;
			return Ret;
        }

		static BindingLayoutItem create_bindless_uav(UINT32 dwRegisterSpace = 0)
		{
			BindingLayoutItem Ret;
			// 只要是个 UAV 就行.
			Ret.type = ResourceViewType::Texture_UAV;
			Ret.register_space = dwRegisterSpace;
			return Ret;
		}

		static BindingLayoutItem create_bindless_cbv(UINT32 dwRegisterSpace = 0)
		{
			BindingLayoutItem Ret;
			Ret.type = ResourceViewType::ConstantBuffer;
			Ret.register_space = dwRegisterSpace;
			return Ret;
		}

		static BindingLayoutItem create_bindless_sampler(UINT32 dwRegisterSpace = 0)
		{
			BindingLayoutItem Ret;
			Ret.type = ResourceViewType::ConstantBuffer;
			Ret.register_space = dwRegisterSpace;
			return Ret;
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

        static BindingLayoutItem create_constant_buffer(uint32_t slot, bool is_volatile = true)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = is_volatile ? ResourceViewType::VolatileConstantBuffer : ResourceViewType::ConstantBuffer;
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

        static BindingLayoutItem create_push_constants(uint32_t slot, uint16_t dwByteSize)
        {
            BindingLayoutItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::PushConstants;
            ret.size = dwByteSize;
            return ret;
        }
    };

    using BindingLayoutItemArray = StackArray<BindingLayoutItem, MAX_BINDINGS_PER_LAYOUT>;

    struct BindingLayoutDesc
    {
        ShaderType shader_visibility = ShaderType::All;
        uint32_t register_space = 0;

        // 相同类型的 item 需放在一块
        BindingLayoutItemArray binding_layout_items;
    };

    struct BindlessLayoutDesc
    {
        // 不允许 PushConstants 和 VolatileConstantBuffer
        BindingLayoutItemArray binding_layout_items;
		
        uint32_t first_slot = 0;
        ShaderType shader_visibility = ShaderType::All;
    };

    struct BindingLayoutInterface
    {
        virtual const BindingLayoutDesc& get_binding_desc() const = 0;
        virtual const BindlessLayoutDesc& get_bindless_desc() const = 0;
        virtual bool is_binding_less() const = 0;

		virtual ~BindingLayoutInterface() = default;
    };



    // 确保其为 32 字节
    struct BindingSetItem
    {
        std::shared_ptr<ResourceInterface> resource;

        uint32_t slot = 0u;     // 对 bindless set 来说, 这是 slot offset.

        ResourceViewType type          : 8 = ResourceViewType::None;
        TextureDimension dimension : 8 = TextureDimension::Unknown; // 供 Texture_SRV, Texture_UAV 使用
        Format format              : 8 = Format::UNKNOWN; // 供 Texture_SRV, Texture_UAV, Buffer_SRV, Buffer_UAV 使用
        uint8_t pad                 : 8 = 0;

        union 
        {
            TextureSubresourceSet subresource; // 供 valid for Texture_SRV, Texture_UAV 使用，且必须得为 16 字节
            BufferRange range;                 // 供 Buffer_SRV, Buffer_UAV, ConstantBuffer 使用，且必须得为 16 字节
            uint64_t pads[2] = { 0, 0 };
        };


        static BindingSetItem create_texture_srv(
            uint32_t slot,
            std::shared_ptr<TextureInterface> texture,
            TextureSubresourceSet subresource = entire_subresource_set
        )
        {
            assert(texture != nullptr);
            TextureDesc desc = texture->get_desc();

            BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::Texture_SRV;
            ret.resource = texture;
            ret.format = desc.format;
            ret.dimension = desc.dimension;
            ret.subresource = subresource;
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_texture_uav(
            uint32_t slot,
            std::shared_ptr<TextureInterface> texture,
            TextureSubresourceSet subresource = TextureSubresourceSet{}
        )
        {
            assert(texture != nullptr);
			TextureDesc desc = texture->get_desc();

			BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::Texture_UAV;
            ret.resource = texture;
            ret.format = desc.format;
            ret.dimension = desc.dimension;
            ret.subresource = subresource;
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_typed_buffer_srv(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
			assert(buffer != nullptr);
			BufferDesc desc = buffer->get_desc();

            BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::TypedBuffer_SRV;
            ret.resource = buffer;
            ret.format = desc.format;
            ret.dimension = TextureDimension::Unknown;
            ret.range = BufferRange{ .byte_size = desc.byte_size };
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_typed_buffer_uav(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            assert(buffer != nullptr);
			BufferDesc desc = buffer->get_desc();

			BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::TypedBuffer_UAV;
            ret.resource = buffer;
            ret.format = desc.format;
            ret.dimension = TextureDimension::Unknown;
            ret.range = BufferRange{ .byte_size = desc.byte_size };
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_constant_buffer(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            assert(buffer != nullptr);

            BufferDesc desc = buffer->get_desc();

            const bool is_volatile = buffer && desc.is_volatile;

            BindingSetItem ret;
            ret.slot = slot;
            ret.type = is_volatile ? ResourceViewType::VolatileConstantBuffer : ResourceViewType::ConstantBuffer;
            ret.resource = buffer;
            ret.format = Format::UNKNOWN;
            ret.dimension = TextureDimension::Unknown;
            ret.range = BufferRange{ .byte_size = desc.byte_size };
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_sampler(
            uint32_t slot,
            std::shared_ptr<SamplerInterface> sampler
        )
        {
            assert(sampler != nullptr);
            BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::Sampler;
            ret.resource = sampler;
            ret.format = Format::UNKNOWN;
            ret.dimension = TextureDimension::Unknown;
            ret.pads[0] = 0;
            ret.pads[1] = 0;
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_structured_buffer_srv(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            assert(buffer != nullptr);
            BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::StructuredBuffer_SRV;
            ret.resource = buffer;
            ret.format = Format::UNKNOWN;
            ret.dimension = TextureDimension::Unknown;
            ret.range = BufferRange{ .byte_size = buffer->get_desc().byte_size };
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_structured_buffer_uav(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            assert(buffer != nullptr);
            BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::StructuredBuffer_UAV;
            ret.resource = buffer;
            ret.format = Format::UNKNOWN;
            ret.dimension = TextureDimension::Unknown;
            ret.range = BufferRange{ .byte_size = buffer->get_desc().byte_size };
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_raw_buffer_srv(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            assert(buffer != nullptr);
            BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::RawBuffer_SRV;
            ret.resource = buffer;
            ret.format = Format::UNKNOWN;
            ret.dimension = TextureDimension::Unknown;
            ret.range = BufferRange{ .byte_size = buffer->get_desc().byte_size };
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_raw_buffer_uav(
            uint32_t slot,
            std::shared_ptr<BufferInterface> buffer
        )
        {
            assert(buffer != nullptr);
            BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::RawBuffer_UAV;
            ret.resource = buffer;
            ret.format = Format::UNKNOWN;
            ret.dimension = TextureDimension::Unknown;
            ret.range = BufferRange{ .byte_size = buffer->get_desc().byte_size };
            ret.pad = 0;
            return ret;
        }

        static BindingSetItem create_push_constants(
            uint32_t slot,
            uint32_t dwByteSize
        )
        {
            BindingSetItem ret;
            ret.slot = slot;
            ret.type = ResourceViewType::PushConstants;
            ret.resource = nullptr;
            ret.format = Format::UNKNOWN;
            ret.dimension = TextureDimension::Unknown;
            ret.range.byte_offset = 0;
            ret.range.byte_size = dwByteSize;
            ret.pad = 0;
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
		virtual void resize(uint32_t new_size, bool keep_contents) = 0;
		virtual bool set_slot(const BindingSetItem& item) = 0;

		virtual ~BindlessSetInterface() = default;
    };
    
}







#endif