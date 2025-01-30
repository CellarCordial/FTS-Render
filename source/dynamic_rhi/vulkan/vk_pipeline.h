#ifndef DYNAMIC_RHI_VULKAN_PIPELINE_H
#define DYNAMIC_RHI_VULKAN_PIPELINE_H


#include "../pipeline.h"
#include "vk_forward.h"
#include <memory>


namespace fantasy 
{
    class VKInputLayout : public InputLayoutInterface
    {
    public:
        VKInputLayout() = default;
		~VKInputLayout() override = default;

        const VertexAttributeDesc& get_attribute_desc(uint32_t attribute_index) const override;
        uint32_t get_attributes_num() const override;

    public:
        std::vector<VertexAttributeDesc> input_desc;
        std::vector<vk::VertexInputBindingDescription> binding_desc;
        std::vector<vk::VertexInputAttributeDescription> attribute_desc;
    };

    class VKBindingLayout : public BindingLayoutInterface
    {
    public:
        VKBindingLayout(const VKContext* context, const BindingLayoutDesc& desc);
        VKBindingLayout(const VKContext* context, const BindlessLayoutDesc& desc);
        ~VKBindingLayout() override;

        const BindlessLayoutDesc& get_bindless_desc() const override;
        const BindingLayoutDesc& get_binding_desc() const override;
        bool is_binding_less() const override;

        // generate the descriptor set layout
        vk::Result bake();

    public:
        BindingLayoutDesc desc;
        BindlessLayoutDesc bindless_desc;
        bool is_bindless;

        std::vector<vk::DescriptorSetLayoutBinding> vulkan_layout_bindings;

        vk::DescriptorSetLayout descriptor_set_layout;

        std::vector<vk::DescriptorPoolSize> descriptor_pool_size_info;

    private:
        const VKContext* _context;
    };

    class VKBindingSet : public BindingSetInterface
    {
    public:
        explicit VKBindingSet(const VKContext* context)
            : m_context(context)
        { }
		~VKBindingSet() override = default;

        BindingLayoutInterface* get_layout() const override;
        const BindingSetDesc& get_desc() const override;
        bool is_bindless() const override;

    
    public:
        BindingSetDesc desc;
        std::shared_ptr<BindingLayoutInterface> layout;

        vk::DescriptorPool descriptor_pool;
        vk::DescriptorSet descriptor_set;

        std::vector<std::shared_ptr<ResourceInterface>> resources;
        StackArray<BufferInterface*, MAX_VOLATILE_CONSTANT_BUFFERS_PER_LAYOUT> volatile_constant_buffers;

        std::vector<uint16_t> bindings_need_transitions;

    private:
        const VKContext* m_context;
    };

    class VKBindlessSet : public BindlessSetInterface
    {
    public:
        explicit VKBindlessSet(const VKContext* context) : _context(context) {}
        ~VKBindlessSet() override;

		void resize(uint32_t new_size, bool keep_contents) override;
		bool set_slot(const BindingSetItem& item) override;
        uint32_t get_capacity() const override;

    public:
        std::shared_ptr<BindingLayoutInterface> layout;
        uint32_t capacity = 0;

        vk::DescriptorPool descriptor_pool;
        vk::DescriptorSet descriptor_set;

    private:
        const VKContext* _context;
    };


    class VKGraphicsPipeline : public GraphicsPipelineInterface
    {
    public:
        explicit VKGraphicsPipeline(const VKContext* context)  : _context(context) {}
        ~VKGraphicsPipeline() override;

        const FrameBufferInfo& get_frame_buffer_info() const override;
        const GraphicsPipelineDesc& get_desc() const override;
        void* get_native_object() override;

    public:
        GraphicsPipelineDesc desc;
        FrameBufferInfo frame_buffer_info;
        ShaderType shader_type = ShaderType::None;
        StackArray<std::shared_ptr<BindingLayoutInterface>, MAX_BINDING_LAYOUTS> pipeline_binding_layouts;
        StackArray<uint32_t, MAX_BINDING_LAYOUTS> descriptor_set_index_to_binding_index;
        vk::PipelineLayout pipeline_layout;
        vk::Pipeline pipeline;
        vk::ShaderStageFlags push_constant_visibility;
        bool uses_blend_constants = false;
    
    private:
        const VKContext* _context;
    };

    class VKComputePipeline : public ComputePipelineInterface
    {
    public:
        explicit VKComputePipeline(const VKContext* context)  : m_Context(context) {}
        ~VKComputePipeline() override;

        const ComputePipelineDesc& get_desc() const override;
        void* get_native_object() override;
    
    public:
        ComputePipelineDesc desc;

        StackArray<std::shared_ptr<BindingLayoutInterface>, MAX_BINDING_LAYOUTS> pipeline_binding_layouts;
        StackArray<uint32_t, MAX_BINDING_LAYOUTS> descriptor_set_index_to_binding_index;
        vk::PipelineLayout pipeline_layout;
        vk::Pipeline pipeline;
        vk::ShaderStageFlags push_constant_visibility;

    private:
        const VKContext* m_Context;
    };
    
    class RayTracingPipeline : public ray_tracing::PipelineInterface
    {
    public:
        explicit RayTracingPipeline(const VKContext* context)  : _context(context) {}
        ~RayTracingPipeline() override;

        const ray_tracing::PipelineDesc& get_desc() const override;
        ray_tracing::ShaderTableInterface* create_shader_table() override;
        void* get_native_object() override;

        int findShaderGroup(const std::string& name);

    public:
        ray_tracing::PipelineDesc desc;
        StackArray<std::shared_ptr<BindingLayoutInterface>, MAX_BINDING_LAYOUTS> pipeline_binding_layouts;
        StackArray<uint32_t, MAX_BINDING_LAYOUTS> descriptor_set_index_to_binding_index;
        vk::PipelineLayout pipeline_layout;
        vk::Pipeline pipeline;
        vk::ShaderStageFlags push_constant_visibility;

        std::unordered_map<std::string, uint32_t> shader_groups;
        std::vector<uint8_t> shader_group_handles;

    private:
        const VKContext* _context;
    };


    class ShaderTable : public ray_tracing::ShaderTableInterface
    {
    public:
        ShaderTable(const VKContext* context, RayTracingPipeline* _pipeline)
            : pipeline(_pipeline)
            , m_Context(context)
        { }
        ~ShaderTable() override = default;
        
        void set_raygen_shader(const char* name, BindingSetInterface* binding_set = nullptr) override;
        int32_t add_miss_shader(const char* name, BindingSetInterface* binding_set = nullptr) override;
        int32_t add_hit_group(const char* name, BindingSetInterface* binding_set = nullptr) override;
        int32_t add_callable_shader(const char* name, BindingSetInterface* binding_set = nullptr) override;
        void clear_miss_shaders() override;
        void clear_hit_groups() override;
        void clear_callable_shaders() override;
        ray_tracing::PipelineInterface* get_pipeline() const override;

        bool verify_shader_group_exists(const char* export_name, int shader_group_index) const;
    
    public:
        std::shared_ptr<RayTracingPipeline> pipeline;

        int32_t ray_generation_shader = -1;
        std::vector<uint32_t> miss_shaders;
        std::vector<uint32_t> callable_shaders;
        std::vector<uint32_t> hit_groups;

        uint32_t version = 0;

    private:
        const VKContext* m_Context;
    };

    class AccelStruct : public ray_tracing::AccelStructInterface
    {
    public:
        explicit AccelStruct(const VKContext* context)
            : m_Context(context)
        { }

        ~AccelStruct() override;

        const ray_tracing::AccelStructDesc& get_desc() const override;
        MemoryRequirements get_memory_requirements() override;
        bool bind_memory(HeapInterface* heap, uint64_t offset = 0) override;
        BufferInterface* get_buffer() const override;

    public:
        std::shared_ptr<BufferInterface> data_buffer;
        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        vk::AccelerationStructureKHR accel_struct;
        vk::DeviceAddress accel_struct_device_address = 0;
        ray_tracing::AccelStructDesc desc;
        bool allow_update = false;
        bool compacted = false;
        size_t rtxmu_id = INVALID_SIZE_64;
        vk::Buffer rtxmu_buffer;

    private:
        const VKContext* m_Context;
    };

}

#endif