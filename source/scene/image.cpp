#include "image.h"
#include <string>
#include "../core/tools/log.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>


namespace fantasy 
{
    Image Image::LoadImageFromFile(const char* file_name)
    {
        int32_t width = 0, height = 0;
        auto data = stbi_load(file_name, &width, &height, 0, STBI_rgb_alpha);

        if (!data)
        {
            LOG_ERROR("Failed to load Image.");
            return Image{};
        }

        LOG_INFO("Loaded Image: " + std::string(file_name));

        Image ret;
        ret.size = width * height * STBI_rgb_alpha;
            
        ret.width = width;
        ret.height = height;
        ret.format = Format::RGBA8_UNORM;
        ret.data = std::make_shared<uint8_t[]>(ret.size);

        std::vector<uint8_t> tmp(ret.size);
        for (uint32_t ix = 0; ix < ret.size; ++ix)
        {
            tmp[ix] = data[ix];
        }

        memcpy(ret.data.get(), data, ret.size);

        stbi_image_free(data);
        
        return ret;
    }
}

