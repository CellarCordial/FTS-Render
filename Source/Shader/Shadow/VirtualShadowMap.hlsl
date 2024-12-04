// 输入数据结构
struct PSInput
{
    float4 pos : SV_POSITION;  // 屏幕空间位置
    float3 worldPos : TEXCOORD0;  // 世界空间位置
    float depth : TEXCOORD1;  // 片元深度
};

// UAV纹理的结构（物理Tile的深度图像）
RWTexture2D<float> g_physicalTileDepth : register(u0);  // 存储物理 Tile 的深度

// 深度阈值
cbuffer DepthThreshold : register(b0)
{
    float depthThreshold;  // 深度阈值
};

[numthreads(16, 16, 1)]  // 根据需要设置工作组大小
void main(PSInput input)
{
    // 获取当前片元的位置和深度
    float3 worldPos = input.worldPos;
    float currentDepth = input.depth;

    // 计算当前片元所在的物理 Tile 坐标（假设你已经有了一个函数获取物理 Tile 坐标）
    int2 tileCoord = CalculateTileCoordinate(worldPos);  // 计算物理 Tile 坐标

    // 使用原子操作进行深度比较和更新
    // 先读取当前物理 Tile 的深度
    float existingDepth = g_physicalTileDepth[tileCoord];

    // 使用原子比较更新深度（如果当前深度更小，则写入）
    if (currentDepth < existingDepth - depthThreshold)
    {
        // 使用原子比较，只有当前深度小于物理 Tile 中的深度时才更新
        InterlockedCompareExchange(g_physicalTileDepth[tileCoord], currentDepth, existingDepth);
    }
}

// 计算物理 Tile 坐标的辅助函数
int2 CalculateTileCoordinate(float3 worldPos)
{
    // 假设有一些规则可以根据世界坐标计算出物理 Tile 坐标
    // 这里只是一个简单的示例，实际的计算方式会根据实际情况来
    int tileSize = 256;  // 物理 Tile 大小
    int2 tileCoord = int2(worldPos.x / tileSize, worldPos.y / tileSize);
    return tileCoord;
}
