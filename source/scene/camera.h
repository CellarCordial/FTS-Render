#ifndef SCENE_CAMERA_H
#define SCENE_CAMERA_H

#include <math.h>
#include "../core/math/matrix.h"
#include <glfw3.h>

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
        void set_position(const Vector3F& position);
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
        
        float get_frustum_near_width() const;
        float get_frustum_far_width() const;

        Matrix4x4 get_view_proj() const { return mul(view_matrix, proj_matrix); }
        Vector2F get_project_constants_ab() const;
        Vector2F cursor_cycle(float x, float y);


        struct FrustumDirections
        {
            // 左上, 右上, 左下, 右下.
            Vector3F A, B, C, D;
        };  

        Camera::FrustumDirections get_frustum_directions();


        Matrix4x4 view_matrix;
        Matrix4x4 proj_matrix;
        Matrix4x4 prev_view_matrix;
        Vector3F position = { 4.087f, 3.6999f, 3.957f };
        Vector3F direction = { 0.0f, 0.0f, 1.0f };
        Vector3F up = { 0.0f, 1.0f, 0.0f };

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

        Vector2F _mouse_position;

        GLFWwindow* _window;
    };


}



#endif