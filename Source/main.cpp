#include "global_render.h"

int main()
{
	fantasy::GlobalRender render;
	if (!render.Init() || !render.run())
	{
		LOG_ERROR("Render Failed.");
		return -1;
	}
	return 0;
}
