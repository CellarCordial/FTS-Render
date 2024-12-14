#ifndef SHADER_RAY_TRCING_HELPER_RAY_CONE_SLANG
#define SHADER_RAY_TRCING_HELPER_RAY_CONE_SLANG

// 2019-ray-tracing-gems-chapter-20-akenine-moller-et-al.
struct RayCone
{
    __init(float spread_angle)
    {
        _width = 0.0f;
        _spread_angle_sin = spread_angle;
    }

    RayCone propagate(float surface_spread_angle_sin, float hit_time)
    {
        RayCone ret;
        ret._width = this._width + this._spread_angle_sin * hit_time;
        ret._spread_angle_sin = this._spread_angle_sin + surface_spread_angle_sin;
        return ret;
    }

    float get_width(float hit_time)
    {
        return _width + _spread_angle_sin * hit_time;
    }

    float _width;
    float _spread_angle_sin;
};
















#endif