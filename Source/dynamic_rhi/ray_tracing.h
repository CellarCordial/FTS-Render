#ifndef RHI_RAY_TRACING
#define RHI_RAY_TRACING

#include "resource.h"
#include "pipeline.h"
#include <memory>
#if RAY_TRACING
#include "../core/math/matrix.h"
#include <vector>

namespace fantasy::ray_tracing
{
	// Forward declaration.
	struct ShaderTableInterface;


	enum class GeometryFlags : uint8_t
	{
		None = 0,
		Opaque = 1,
		NoDuplicateAnyHitInvocation = 2
	};
	ENUM_CLASS_FLAG_OPERATORS(GeometryFlags);

	enum class GeometryType : uint8_t
	{
		Triangle,
		BoundingBox
	};

	struct GeometryTriangles
	{
		std::shared_ptr<BufferInterface> index_buffer;
		std::shared_ptr<BufferInterface> vertex_buffer;
		Format index_format;
		Format vertex_format;
		uint32_t index_count = 0;
		uint64_t index_offset = 0;
		uint32_t vertex_count = 0;
		uint64_t vertex_offset = 0;
		uint32_t vertex_stride = 0;
	};

	struct GeometryBoundingBoxes
	{
		std::shared_ptr<BufferInterface> buffer;		
		std::shared_ptr<BufferInterface> unused_buffer;
		uint64_t offset = 0;
		uint32_t count = 0;
		uint32_t stride = 0;
	};


	struct GeometryDesc
	{
		GeometryFlags flags = GeometryFlags::None;
		GeometryType type = GeometryType::Triangle;

		union 
		{
			GeometryTriangles triangles;
			GeometryBoundingBoxes aabbs;
		};

		bool use_transform = false;
		Matrix3x4 affine_matrix;


		GeometryDesc(const GeometryDesc& other) :
			flags(other.flags), 
			type(other.type),
			use_transform(other.use_transform),
			affine_matrix(other.affine_matrix)
		{
			if (type == GeometryType::Triangle) triangles = other.triangles;
			else if (type == GeometryType::BoundingBox) aabbs = other.aabbs;
		}

		GeometryDesc& operator=(const GeometryDesc& other)
		{
			if (this != &other) 
			{
				flags = other.flags;
				type = other.type;
				
				use_transform = other.use_transform;
				affine_matrix = other.affine_matrix;

				if (type == GeometryType::Triangle) triangles = other.triangles;
				else if (type == GeometryType::BoundingBox) aabbs = other.aabbs;
			}

			return *this;
		}

		~GeometryDesc()
		{
			if (type == GeometryType::Triangle)
			{
				triangles.index_buffer.reset();
				triangles.vertex_buffer.reset();
			}
			else if (type == GeometryType::BoundingBox)
			{
				aabbs.buffer.reset();
				aabbs.unused_buffer.reset();
			}
		}
	};

	enum class InstanceFlags : uint8_t
	{
		None							= 0,
		TriangleCullDisable				= 1,
		TriangleFrontCounterclockwise	= 1 << 1,
		ForceOpaque						= 1 << 2,
		ForceNonOpaque					= 1 << 3,
	};
	ENUM_CLASS_FLAG_OPERATORS(InstanceFlags);

	struct AccelStructInterface;

	struct InstanceDesc
	{
		Matrix3x4 affine_matrix;

		uint32_t instance_id : 24 = 0;
		uint32_t instance_mask : 8 = 0;

		uint32_t instance_contibution_to_hit_group_index : 24 = 0;
		InstanceFlags flags : 8 = InstanceFlags::None;
		
		union 
		{
			AccelStructInterface* bottom_level_accel_struct = nullptr;
			size_t bias_device_address;
		};
	};

	enum class AccelStructBuildFlags : uint8_t
	{
		None				= 0,
		AllowUpdate			= 1,
		AllowCompaction		= 1 << 1,
		PreferFastTrace		= 1 << 2,
		PreferFastBuild		= 1 << 3,
		MinimizeMemory		= 1 << 4,
		PerformUpdate		= 1 << 5
	};
	ENUM_CLASS_FLAG_OPERATORS(AccelStructBuildFlags);

	struct AccelStructDesc
	{
		std::string name;

		bool is_virtual = false;
		bool is_top_level = false;
		uint64_t top_level_max_instance_num = 0;
		std::vector<GeometryDesc> bottom_level_geometry_descs;

		AccelStructBuildFlags flags = AccelStructBuildFlags::None;
	};

	
	struct AccelStructInterface : ResourceInterface
	{
		virtual const AccelStructDesc& get_desc() const = 0;
		virtual MemoryRequirements get_memory_requirements() = 0;
		virtual bool bind_memory(HeapInterface* heap, uint64_t offset = 0) = 0;
        virtual BufferInterface* get_buffer() const = 0;

		virtual ~AccelStructInterface() = default;
	};

	struct ShaderDesc
	{
		Shader* shader = nullptr;
		BindingLayoutInterface* binding_layout = nullptr;
	};

	struct FHitGroupDesc
	{
		std::string export_name;
		Shader* closest_hit_shader = nullptr;
		Shader* any_hit_shader = nullptr;
		Shader* intersect_shader = nullptr;
		BindingLayoutInterface* binding_layout = nullptr;
		
		bool is_procedural_primitive = false;
	};

	struct PipelineDesc
	{
		std::vector<ShaderDesc> shader_descs;
		std::vector<FHitGroupDesc> hit_group_descs;

		BindingLayoutInterfaceArray global_binding_layouts;
		uint32_t max_payload_size = 0;
		uint32_t max_attribute_size = sizeof(float) * 2;
		uint32_t max_recursion_depth = 1;
	};

	
	struct PipelineInterface : public ResourceInterface
	{
		virtual const PipelineDesc& get_desc() const = 0;
		virtual ShaderTableInterface* create_shader_table() = 0;
        virtual void* get_native_object() = 0;

		virtual ~PipelineInterface() = default;
	};

	
	struct ShaderTableInterface : public ResourceInterface
	{
		virtual void set_raygen_shader(const char* name, BindingSetInterface* binding_set = nullptr) = 0;

		virtual int32_t add_miss_shader(const char* name, BindingSetInterface* binding_set = nullptr) = 0;
		virtual int32_t add_hit_group(const char* name, BindingSetInterface* binding_set = nullptr) = 0;
		virtual int32_t add_callable_shader(const char* name, BindingSetInterface* binding_set = nullptr) = 0;
		
		virtual void clear_miss_shaders() = 0;
		virtual void clear_hit_groups() = 0;
		virtual void clear_callable_shaders() = 0;

		virtual PipelineInterface* get_pipeline() const = 0;
		
		virtual ~ShaderTableInterface() = default;
	};


	struct PipelineState
	{
		ShaderTableInterface* shader_table = nullptr;
		BindingSetItemArray binding_sets;
	};

	struct FDispatchRaysArguments
	{
		uint32_t width = 1;	
		uint32_t height = 1;
		uint32_t depth = 1;
	};
}



#endif









#endif