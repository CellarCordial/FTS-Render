#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

#include "../common/light.hlsl"


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    
}

#endif