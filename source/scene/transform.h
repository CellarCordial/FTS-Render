#ifndef SCENE_TRANSFORM_H
#define SCENE_TRANSFORM_H

#include "../core/math/matrix.h"
#include "../core/tools/ecs.h"


namespace fantasy 
{

	struct Transform
	{
		float3 position = { 0.0f, 0.0f, 0.0f };
		float3 rotation = { 0.0f, 0.0f, 0.0f };
		float3 scale = { 1.0f, 1.0f, 1.0f };

		float4x4 world_matrix() const
		{
			return float4x4(mul(translate(position), mul(rotate(rotation), ::fantasy::scale(scale))));
		}
	};
    
    namespace event
	{
		struct OnModelTransform
		{
			Entity* entity = nullptr;
			Transform transform;
		};
	};
}


















#endif