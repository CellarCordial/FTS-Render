#include "render_resource_cache.h"
#include <string>
#include "../core/tools/check_cast.h"

namespace fantasy
{
    bool RenderResourceCache::initialize() 
    {
        ReturnIfFalse(_world != nullptr);
        
        return true;
    }

    
    void RenderResourceCache::collect(const std::shared_ptr<ResourceInterface>& resource, ResourceType type)
    {
        switch (type)
        {
            case ResourceType::Texture: 
                _resource_names[check_cast<TextureInterface>(resource)->get_desc().name] = ResourceData{ .resource = resource };
                break;
            case ResourceType::Buffer:
                _resource_names[check_cast<BufferInterface>(resource)->get_desc().name] = ResourceData{ .resource = resource };
                break;
            case ResourceType::Sampler:
                _resource_names[check_cast<SamplerInterface>(resource)->get_desc().name] = ResourceData{ .resource = resource };
                break;
            default:
                assert(false && "invalid render resource type.");
        }
    }

    std::shared_ptr<ResourceInterface> RenderResourceCache::require(const char* name)
    {
        auto iter = _resource_names.find(std::string(name));
        if (iter != _resource_names.end())
        {
            return iter->second.resource;
        }

        std::string str = "There is no resource named ";
        LOG_ERROR(str + std::string(name));
        return nullptr;
    }
    
	bool RenderResourceCache::collect_constants(const char* name, void* data, uint64_t element_num)
	{
		if (data == nullptr || name == nullptr)
		{
			LOG_ERROR("collect constants has a null pointer.");
			return true;
		}

		if (_data_names.find(std::string(name)) == _data_names.end())
		{
			_data_names[std::string(name)] = std::make_pair(data, element_num);
			return true;
		}

		LOG_ERROR("Render resource cache already has the same constant data.");
		return false;
	}

	bool RenderResourceCache::require_constants(const char* name, void** out_data, uint64_t* out_element_num)
	{
		if (name == nullptr || out_data == nullptr)
		{
			LOG_ERROR("collect constants has a null pointer.");
			return false;
		}

		auto iter = _data_names.find(std::string(name));

		if (iter == _data_names.end())
		{
			std::string str = "Render resource cache doesn't have the constant data called ";
			str += name;
			LOG_ERROR(str);
			return false;
		}

        *out_data = iter->second.first;
        if (out_element_num) *out_element_num = iter->second.second;

        return true;
	}
}