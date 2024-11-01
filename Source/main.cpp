#include "GlobalRender.h"

int main()
{
	FTS::FGlobalRender Render;
	if (!Render.Init() || !Render.Run())
	{
		LOG_ERROR("Render Failed.");
		return -1;
	}
	return 0;
}