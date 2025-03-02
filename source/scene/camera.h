#ifndef SCENE_CAMERA_H
#define SCENE_CAMERA_H

#include <math.h>
#include "../core/math/matrix.h"
#include <GLFW/glfw3.h>

namespace fantasy 
{
    class Camera
    {
    public:
        Camera(GLFWwindow* window);

    public:
        void update_view_matrix();

        void handle_input(float delta_time);
        void handle_keyboard_input(float delta_time);

        void set_lens(float fov, float aspect, float near_z, float far_z);
        void set_position(const float3& position);
        void set_direction(float vert, float horz);
        
        void strafe(float size);      // 前后移动
        void walk(float size);        // 左右平移
        void vertical(float size);    // 上下移动  

        void pitch(float angle);  // 上下俯仰
        void yall(float angle);   // 左右转向
        void pitch(int32_t X);  // 上下俯仰
        void yall(int32_t Y);   // 左右转向

        float get_fov_x() const;
        float get_fov_y() const;

        
        float get_near_z() const { return _near_z; }
        float get_far_z() const { return _far_z; }
        
        float get_frustum_near_width() const;
        float get_frustum_far_width() const;

        float4x4 get_view_proj() const { return mul(view_matrix, proj_matrix); }
        float4x4 get_reverse_z_view_proj() const { return mul(view_matrix, reverse_z_proj_matrix); }
        float2 get_project_constants_ab() const;
        float2 cursor_cycle(float x, float y);


        struct FrustumDirections
        {
            // 左上, 右上, 左下, 右下.
            float3 A, B, C, D;
        };  

        Camera::FrustumDirections get_frustum_directions();


        float4x4 view_matrix;
        float4x4 prev_view_matrix;
        float4x4 proj_matrix;
        float4x4 reverse_z_proj_matrix;
        
        float3 position = { 0.0f, 0.1f, -5.0f };
        float3 direction = { 0.0f, 0.0f, 1.0f };
        float3 up = { 0.0f, 1.0f, 0.0f };
        int32_t speed = 5;

    private:
		float _vert_radians = 0.0f;
		float _horz_radians = 0.0f;

        float _near_z = 0.0f;
        float _far_z = 0.0f;
        
        float _aspect = 0.0f;
        float _fov_y = 0.0f;
        
        float _near_window_height = 0.0f;
        float _far_window_height = 0.0f;

        bool _view_need_update = true;

        float2 _mouse_position;

        GLFWwindow* _window;
    };


}



#endif