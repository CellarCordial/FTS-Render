#include "camera.h"
#include <wtypesbase.h>

namespace fantasy 
{
    Camera::Camera(GLFWwindow* window) : 
        _window(window)
    {
        set_lens(60.0f, 1.0f * CLIENT_WIDTH / CLIENT_HEIGHT, 0.1f, 100.0f);
        update_view_matrix();
        prev_view_matrix = view_matrix;
    }

    void Camera::handle_input(float delta_time)
    {
        double new_x, new_y;
        glfwGetCursorPos(_window, &new_x, &new_y);
        float2 new_pos(new_x, new_y);
        auto cusor_state = glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_RIGHT);
        if (cusor_state == GLFW_PRESS)
        {
             //glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

            pitch(static_cast<int32_t>(new_y));
            yall(static_cast<int32_t>(new_x));
            update_view_matrix();

            new_pos = cursor_cycle(new_pos.x, new_pos.y);
        }
        else if (cusor_state == GLFW_RELEASE)
        {
             //glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        _mouse_position = new_pos;

        handle_keyboard_input(delta_time);
    }

    void Camera::update_view_matrix()
    {
        if(_view_need_update)
        {
            prev_view_matrix = view_matrix;
            view_matrix = look_at_left_hand(position, position + direction, up);
            _view_need_update = false;
        }
    }

    Camera::FrustumDirections Camera::get_frustum_directions()
    {
        float4x4 inv_view_proj = inverse(mul(view_matrix, proj_matrix));
        const float4 A0 = mul(float4(-1, 1, 0.2f, 1.0f), inv_view_proj);
        const float4 A1 = mul(float4(-1, 1, 0.5f, 1.0f), inv_view_proj);

        const float4 B0 = mul(float4(1, 1, 0.2f, 1.0f), inv_view_proj);
        const float4 B1 = mul(float4(1, 1, 0.5f, 1.0f), inv_view_proj);

        const float4 C0 = mul(float4(-1, -1, 0.2f, 1.0f), inv_view_proj);
        const float4 C1 = mul(float4(-1, -1, 0.5f, 1.0f), inv_view_proj);
        
        const float4 D0 = mul(float4(1, -1, 0.2f, 1.0f), inv_view_proj);
        const float4 D1 = mul(float4(1, -1, 0.5f, 1.0f), inv_view_proj);

        FrustumDirections directions;
        directions.A = normalize(float3(A1) / A1.w - float3(A0) / A0.w);
        directions.B = normalize(float3(B1) / B1.w - float3(B0) / B0.w);
        directions.C = normalize(float3(C1) / C1.w - float3(C0) / C0.w);
        directions.D = normalize(float3(D1) / D1.w - float3(D0) / D0.w);

        return directions;
    }


    void Camera::handle_keyboard_input(float delta_time)
    {
        auto action = glfwGetKey(_window, GLFW_KEY_W);
        if (glfwGetKey(_window, GLFW_KEY_W) == GLFW_PRESS) walk(3.0f * delta_time);
        if (glfwGetKey(_window, GLFW_KEY_S) == GLFW_PRESS) walk(-3.0f * delta_time);
        if (glfwGetKey(_window, GLFW_KEY_D) == GLFW_PRESS) strafe(3.0f * delta_time);
        if (glfwGetKey(_window, GLFW_KEY_A) == GLFW_PRESS) strafe(-3.0f * delta_time);
        if (glfwGetKey(_window, GLFW_KEY_E) == GLFW_PRESS) vertical(3.0f * delta_time);
        if (glfwGetKey(_window, GLFW_KEY_Q) == GLFW_PRESS) vertical(-3.0f * delta_time);

        update_view_matrix();
    }

    void Camera::set_lens(float fov, float aspect, float near_z, float far_z)
    {
        _fov_y = fov; _aspect = aspect; _near_z = near_z; _far_z = far_z;
        _near_window_height = _near_z * static_cast<float>(std::tan(_fov_y)) * 2.0f;
        _far_window_height = _far_z * static_cast<float>(std::tan(_fov_y)) * 2.0f;

        proj_matrix = perspective_left_hand(_fov_y, _aspect, _near_z, _far_z);    // LH is left hand
    }

    void Camera::set_position(const float3& pos)
    {
        position = pos;
        _view_need_update = true;
    }

	void Camera::set_direction(float vert, float horz)
	{
		_vert_radians = radians(vert);
		_horz_radians = radians(horz);
        direction = float3(
            std::cos(_vert_radians) * std::cos(_horz_radians),
            std::sin(_vert_radians),
            std::cos(_vert_radians) * std::sin(_horz_radians)
        );

	}

	void Camera::walk(float size)
    {
        size *= speed;

        position = size * direction + position;

        _view_need_update = true;
    }

    void Camera::strafe(float size)
    {
        size *= speed;

        float3 right = cross(up, direction);
        position = size * right + position;

        _view_need_update = true;
    }

    void Camera::vertical(float size)
    {
        size *= speed;

        position = size * up + position;

        _view_need_update = true;
    }

    void Camera::pitch(float angle)
    {
        _vert_radians -= radians(angle);
		direction = float3(
			std::cos(_vert_radians) * std::cos(_horz_radians),
			std::sin(_vert_radians),
			std::cos(_vert_radians) * std::sin(_horz_radians)
		);

        _view_need_update = true;
    }

    void Camera::pitch(int32_t Y)
    {
        pitch(0.2f * (static_cast<float>(Y) - _mouse_position.y));
    }

    void Camera::yall(float angle)
    {
        _horz_radians -= radians(angle);
		direction = float3(
			std::cos(_vert_radians) * std::cos(_horz_radians),
			std::sin(_vert_radians),
			std::cos(_vert_radians) * std::sin(_horz_radians)
		);

        _view_need_update = true;
    }

    void Camera::yall(int32_t X)
    {
        yall(0.2f * (static_cast<float>(X) - _mouse_position.x));
    }

    float Camera::get_fov_x() const
    {
        return 2.0f * static_cast<float>(std::atan((0.5f * get_frustum_near_width()) / _near_z));
    }

    float Camera::get_fov_y() const
    {
        return _fov_y;
    }

    float Camera::get_frustum_near_width() const
    {
        return _aspect * _near_window_height;
    }

    float Camera::get_frustum_far_width() const
    {
        return _aspect * _far_window_height;
    }

    float2 Camera::cursor_cycle(float x, float y)
    {
        if (x > CLIENT_WIDTH)     x = 0.0f;
        if (x < 0.0f)               x = static_cast<float>(CLIENT_WIDTH);
        if (y > CLIENT_HEIGHT)    y = 0.0f;
        if (y < 0.0f)               y = static_cast<float>(CLIENT_HEIGHT);
        glfwSetCursorPos(_window, static_cast<double>(x), static_cast<double>(y));

        return float2(x, y);
    }

    float2 Camera::get_project_constants_ab() const
    {
        float2 constants;
        constants.x = _far_z / (_far_z - _near_z);
        constants.y = -(_near_z * _far_z) / (_far_z - _near_z);
        return constants;
    }


}