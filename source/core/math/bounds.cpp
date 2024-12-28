#include "bounds.h"
#include "vector.h"
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

    Circle merge(const Circle& circle0, const Circle& circle1)
	{
        Vector2F center0_to_1 = circle0.center - circle1.center;
        float center_distance = center0_to_1.length();
        if (circle0.radius - circle1.radius >= center_distance) return circle0;
        if (circle1.radius - circle0.radius >= center_distance) return circle1;

        float radius = (center_distance + circle0.radius + circle1.radius) * 0.5f;
        Vector2F center = circle0.center + (center0_to_1 / center_distance) * (radius - circle0.radius);
		return Circle(center, radius);
	}
	
	Sphere merge(const Sphere& sphere0, const Sphere& sphere1)
	{
        Vector3F center0_to_1 = sphere0.center - sphere1.center;
        float center_distance = center0_to_1.length();
        if (sphere0.radius - sphere1.radius >= center_distance) return sphere0;
        if (sphere1.radius - sphere0.radius >= center_distance) return sphere1;

        float radius = (center_distance + sphere0.radius + sphere1.radius) * 0.5f;
        Vector3F center = sphere0.center + (center0_to_1 / center_distance) * (radius - sphere0.radius);
		return Sphere(center, radius);
	}

}