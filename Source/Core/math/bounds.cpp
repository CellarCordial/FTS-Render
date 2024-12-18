#include "bounds.h"
#include <algorithm>
#include <vector>

namespace fantasy 
{
    Bounds3F create_aabb(const std::vector<Vector3F>& position)
    {
        const auto XMinMax = std::minmax_element(
            position.begin(),
            position.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.x < rhs.x;    
            }
        );
        const auto YMinMax = std::minmax_element(
            position.begin(),
            position.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.y < rhs.y;    
            }
        );
        const auto ZMinMax = std::minmax_element(
            position.begin(),
            position.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.z < rhs.z;    
            }
        );
        
        const Vector3F min(
            XMinMax.first->x,
            YMinMax.first->y,
            ZMinMax.first->z
        );
        
        // max - Center相当于(min + max) / 2
        const Vector3F max(
            XMinMax.second->x,
            YMinMax.second->y,
            ZMinMax.second->z
        );    

        return Bounds3F{ min, max };
    }

}