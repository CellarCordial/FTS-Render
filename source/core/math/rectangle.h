#ifndef CORE_MATH_RECTANGLE_H
#define CORE_MATH_RECTANGLE_H


namespace fantasy 
{

    struct Rectangle
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        Rectangle(float in_width, float in_height, float in_x = 0.0f, float in_y = 0.0f) :
            width(in_width), height(in_height), x(in_x), y(in_y)
        {
        }

        bool contain(float _x, float _y) const
        {
            return _x >= x && _x < x + width &&
                   _y >= y && _x < x + width;
        }
    };

}






#endif