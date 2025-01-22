#include "matrix.h"
#include <cassert>

namespace fantasy 
{    
    float4x4 translate(const float3& delta)
    {
        return float4x4{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            delta.x, delta.y, delta.z, 1.0f
        };
    }

    float4x4 scale(const float3& scale)
    {
        return float4x4{
            scale.x,  0.0f,       0.0f,       0,
            0.0f,       scale.y,  0.0f,       0,
            0.0f,       0.0f,       scale.z,  0,
            0.0f,       0.0f,       0.0f,       1.0f
        };
    }

    float4x4 rotate_x(float theta)
    {
        const float sin_theta = std::sin(radians(theta));
        const float cos_theta = std::cos(radians(theta));
        return float4x4(
            1, 0,           0,          0,
            0, cos_theta,   sin_theta, 0,
            0, -sin_theta,   cos_theta,  0,
            0, 0,           0,          1
        );
    }

    float4x4 rotate_y(float theta)
    {
        const float sin_theta = std::sin(radians(theta));
        const float cos_theta = std::cos(radians(theta));
        return float4x4(
            cos_theta,  0,          -sin_theta,  0,
            0,          1,          0,          0,
            sin_theta, 0,          cos_theta,  0,
            0,          0,          0,          1
        );
    }

    float4x4 rotate_z(float theta)
    {
        const float sin_theta = std::sin(radians(theta));
        const float cos_theta = std::cos(radians(theta));
        return float4x4(
            cos_theta,  sin_theta, 0, 0,
            -sin_theta,  cos_theta,  0, 0,
            0,          0,          1, 0,
            0,          0,          0, 1
        );
    }


    float4x4 rotate(float theta, const float3& axis)
    {
        float3 v = normalize(axis);
        float sin_theta = std::sin(radians(theta));
        float cos_theta = std::cos(radians(theta));

        float4x4 ret;

        ret[0][0] = v.x * v.x + (1 - v.x * v.x) * cos_theta;
        ret[1][0] = v.x * v.y * (1 - cos_theta) - v.z * sin_theta;
        ret[2][0] = v.x * v.z * (1 - cos_theta) + v.y * sin_theta;
        ret[3][0] = 0;

        ret[0][1] = v.x * v.y * (1 - cos_theta) + v.z * sin_theta;
        ret[1][1] = v.y * v.y + (1 - v.y * v.y) * cos_theta;
        ret[2][1] = v.y * v.z * (1 - cos_theta) - v.x * sin_theta;
        ret[3][1] = 0;

        ret[0][2] = v.x * v.z * (1 - cos_theta) - v.y * sin_theta;
        ret[1][2] = v.y * v.z * (1 - cos_theta) + v.x * sin_theta;
        ret[2][2] = v.z * v.z + (1 - v.z * v.z) * cos_theta;
        ret[3][2] = 0;

        return ret;
    }

	float4x4 rotate(const float3& rotation)
	{
        return mul(rotate_x(rotation.x), mul(rotate_y(rotation.y), rotate_z(rotation.z)));
	}

	float4x4 orthographic_left_hand(float width, float height, float near_z, float far_z)
	{
		return float4x4(
            2.0f / width, 0.0f,           0.0f,                       0.0f,
			0.0f,          2.0f / height, 0.0f,                       0.0f,
			0.0f,          0.0f,           1.0f / (far_z - near_z),    0.0f,
			0.0f,          0.0f,           near_z / (near_z - far_z),  1
        );
	}

    float4x4 perspective_left_hand(float fov_angle_y, float aspect_ratio, float near_z, float far_z)
    {
        float fInvTanAng = 1.0f / std::tan(radians(fov_angle_y) / 2.0f);
        return float4x4(
            fInvTanAng / aspect_ratio, 0,          0,                         0, 
            0,                         fInvTanAng, 0,                         0, 
            0,                         0,          far_z / (far_z - near_z),  1.0f,
            0,                         0,          -far_z * near_z / (far_z - near_z),                      0
        );

        // mul(scale(float3(fInvTanAng / fAspectRatio, fInvTanAng, 1.0f)), normalizeDepthMatrix);
    }

    float4x4 PerspectiveLeftHandinverseDepth(float fov_angle_y, float aspect_ratio, float near_z, float far_z)
    {
        float fInvTanAng = 1.0f / std::tan(radians(fov_angle_y) / 2.0f);
        return float4x4(
            fInvTanAng / aspect_ratio, 0,          0,                         0, 
            0,                         fInvTanAng, 0,                         0, 
            0,                         0,          -near_z / (far_z - near_z),  1.0f,
            0,                         0,          far_z * near_z / (far_z - near_z),                      0
        );

        // mul(scale(float3(fInvTanAng / fAspectRatio, fInvTanAng, 1.0f)), normalizeDepthMatrix);
    }

	float3x3 create_orthogonal_basis_from_z(const float3& Z)
	{
		float3 z = normalize(Z);
		float3 y;
		if (std::abs(std::abs(dot(Z, float3(1.0f, 0.0f, 0.0f)) - 1.0f)) > 0.1f)
		{
			y = cross(z, float3(1.0f, 0.0f, 0.0f));
		}
		else
		{
			y = cross(z, float3(0.0f, 1.0f, 0.0f));
		}
        float3x3 ret(normalize(cross(y, z)), normalize(y), z);
		return ret;
	}

	float4x4 look_at_left_hand(const float3& pos, const float3& look, const float3& up)
    {
		auto L = normalize(look - pos);
		auto R = normalize(cross(up, L));
		auto U = cross(L, R);

        return inverse(float4x4(
            R.x,     R.y,     R.z,     0.0f,
            U.x,     U.y,     U.z,     0.0f,
            L.x,     L.y,     L.z,     0.0f,
            pos.x, pos.y, pos.z, 1.0f
        ));
    }
}
