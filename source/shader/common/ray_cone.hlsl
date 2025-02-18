#ifndef SHADER_RAY_TRCING_HELPER_RAY_CONE_SLANG
#define SHADER_RAY_TRCING_HELPER_RAY_CONE_SLANG

// 2019-ray-tracing-gems-chapter-20-akenine-moller-et-al.
struct RayCone
{
    float _width;
    float _spread_angle_sin;

    
    RayCone propagate(float surface_spread_angle_sin, float hit_time)
    {
        RayCone ret;
        ret._width = _width + _spread_angle_sin * hit_time;
        ret._spread_angle_sin = _spread_angle_sin + surface_spread_angle_sin;
        return ret;
    }

    float get_width(float hit_time)
    {
        return _width + _spread_angle_sin * hit_time;
    }

};
















#endif