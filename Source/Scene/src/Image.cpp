#include "../include/Image.h"
#include "../../Core/include/ComIntf.h"
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>


namespace FTS 
{
    namespace Image 
    {
        FImage LoadImageFromFile(const CHAR* strFileName)
        {
            INT32 Width = 0, Height = 0;
            auto Data = stbi_load(strFileName, &Width, &Height, 0, STBI_rgb_alpha);

            if (!Data)
            {
                LOG_ERROR("Failed to load Image.");
                return FImage{};
            }

            LOG_INFO("Loaded Image: " + std::string(strFileName));

            FImage Ret;
            Ret.stSize = Width * Height * STBI_rgb_alpha;
                
            Ret.Width = Width;
            Ret.Height = Height;
            Ret.Format = EFormat::RGBA8_UNORM;
            Ret.Data = std::make_shared<UINT8[]>(Ret.stSize);

            std::vector<UINT8> Tmp(Ret.stSize);
            for (UINT32 ix = 0; ix < Ret.stSize; ++ix)
            {
                Tmp[ix] = Data[ix];
            }

            memcpy(Ret.Data.get(), Data, Ret.stSize);

            stbi_image_free(Data);
            
            return Ret;
        }
    }
}

