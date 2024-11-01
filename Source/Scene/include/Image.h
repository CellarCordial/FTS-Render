#ifndef SCENE_IMAGE_H
#define SCENE_IMAGE_H

#include "../../DynamicRHI/include/Format.h"
#include <memory>


namespace FTS 
{
    struct FImage
    {
        UINT32 Width = 0;
        UINT32 Height = 0;
        EFormat Format = EFormat::UNKNOWN;
        UINT64 stSize = 0;
        std::shared_ptr<UINT8[]> Data;

        BOOL IsValid() const { return Data != nullptr; }

        BOOL operator==(const FImage& crOther) const
        {
            return  Width == crOther.Width &&
                    Height == crOther.Height &&
                    Format == crOther.Format &&
                    stSize == crOther.stSize &&
                    Data == crOther.Data;
        }

        BOOL operator!=(const FImage& crOther) const
        {
            return !((*this) == crOther);
        }
    };

    namespace Image 
    {
        FImage LoadImageFromFile(const char* InFileName);

        void Reset();
    }

}


















#endif