#include "../include/Bounds.h"
#include <algorithm>
#include <vector>

namespace FTS 
{
    FBounds3F CreateAABB(const std::vector<FVector3F>& InPosition)
    {
        const auto XMinMax = std::minmax_element(
            InPosition.begin(),
            InPosition.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.x < rhs.x;    
            }
        );
        const auto YMinMax = std::minmax_element(
            InPosition.begin(),
            InPosition.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.y < rhs.y;    
            }
        );
        const auto ZMinMax = std::minmax_element(
            InPosition.begin(),
            InPosition.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.z < rhs.z;    
            }
        );
        
        const FVector3F Min(
            XMinMax.first->x,
            YMinMax.first->y,
            ZMinMax.first->z
        );
        
        // Max - Center相当于(Min + Max) / 2
        const FVector3F Max(
            XMinMax.second->x,
            YMinMax.second->y,
            ZMinMax.second->z
        );    

        return FBounds3F{ Min, Max };
    }

}