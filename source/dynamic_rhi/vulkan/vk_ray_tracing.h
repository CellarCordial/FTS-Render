#ifndef DYNAMIC_RHI_VULKAN_RAY_TRACING_H
#define DYNAMIC_RHI_VULKAN_RAY_TRACING_H

#include "../pipeline.h"
#include "vk_forward.h"

namespace fantasy 
{
        
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

    class VKAccelStruct : public ray_tracing::AccelStructInterface
    {
    public:
        explicit VKAccelStruct(const VKContext* context)
            : m_Context(context)
        { }

        ~VKAccelStruct() override;

        const ray_tracing::AccelStructDesc& get_desc() const override;
        MemoryRequirements get_memory_requirements() override;
        bool bind_memory(HeapInterface* heap, uint64_t offset = 0) override;
        BufferInterface* get_buffer() const override;

    public:
        std::shared_ptr<BufferInterface> data_buffer;
        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        vk::AccelerationStructureKHR vk_accel_struct;
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