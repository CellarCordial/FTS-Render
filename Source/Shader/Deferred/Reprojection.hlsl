#define THREAD_GROUP_NUM_X 0
#define THREAD_GROUP_NUM_Y 0



#if defined(THREAD_GROUP_NUM_X) && defined(THREAD_GROUP_NUM_Y)

[numthreads(THREAD_GROUP_NUM_X, THREAD_GROUP_NUM_Y, 1)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    
}


#endif