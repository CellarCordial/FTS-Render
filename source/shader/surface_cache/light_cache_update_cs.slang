#define THREAD_GROUP_SIZE_X 1
#define THREAD_GROUP_SIZE_Y 1

cbuffer pass_constants : register(b0)
{
};


#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y)

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{

}

#endif