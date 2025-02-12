#include "global_render.h"

int main()
{
    fantasy::GlobalRender render;
    return render.Init() && render.run();
}