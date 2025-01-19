#ifndef SCENE_IMAGE_H
#define SCENE_IMAGE_H

#include "../dynamic_rhi/format.h"
#include <memory>


namespace fantasy 
{
    struct Image
    {
        uint32_t width = 0;
        uint32_t height = 0;
        Format format = Format::UNKNOWN;
        uint64_t size = 0;
        std::shared_ptr<uint8_t[]> data;

        bool is_valid() const { return data != nullptr; }

        bool operator==(const Image& other) const
        {
            return  width == other.width &&
                    height == other.height &&
                    format == other.format &&
                    size == other.size &&
                    data == other.data;
        }

        bool operator!=(const Image& other) const
        {
            return !((*this) == other);
        }
        
        static Image load_image_from_file(const char* file_name);
    };
}


















#endif