/*
 * HE3D Flight Simulator - simulation-specific mesh data.
 * HE3D 飞行模拟器：示例专用网格数据。
 */
#include "flight_sim.hpp"

// Vertex count for the built-in aircraft fallback mesh.
// 内置飞机备用 mesh 的顶点数量。
const HE3D::int32_t flightFallbackAircraftVertexCount = 39;

// Vertices for the built-in aircraft fallback mesh.
// 内置飞机备用 mesh 的顶点数据。
const HE3D::float3 flightFallbackAircraftVertices[] = {
    {0.07f, 0.0f, 0.45f}, {0.0f, 0.07f, 0.45f}, {0.07f, 0.0f, -0.45f},
    {0.0f, 0.07f, 0.45f}, {0.0f, 0.07f, -0.45f}, {0.07f, 0.0f, -0.45f},
    {0.0f, 0.07f, 0.45f}, {-0.07f, 0.0f, 0.45f}, {0.0f, 0.07f, -0.45f},
    {-0.07f, 0.0f, 0.45f}, {-0.07f, 0.0f, -0.45f}, {0.0f, 0.07f, -0.45f},
    {-0.07f, 0.0f, 0.45f}, {0.0f, -0.07f, 0.45f}, {-0.07f, 0.0f, -0.45f},
    {0.0f, -0.07f, 0.45f}, {0.0f, -0.07f, -0.45f}, {-0.07f, 0.0f, -0.45f},
    {0.0f, -0.07f, 0.45f}, {0.07f, 0.0f, 0.45f}, {0.0f, -0.07f, -0.45f},
    {0.07f, 0.0f, 0.45f}, {0.07f, 0.0f, -0.45f}, {0.0f, -0.07f, -0.45f},

    {-0.35f, 0.15f, -0.06f},
    {0.35f, 0.15f, -0.06f},
    {-0.35f, 0.15f, 0.06f},
    {0.35f, 0.15f, -0.06f},
    {0.35f, 0.15f, 0.06f},
    {-0.35f, 0.15f, 0.06f},

    {-0.315f, -0.15f, -0.06f},
    {0.315f, -0.15f, -0.06f},
    {-0.315f, -0.15f, 0.06f},
    {0.315f, -0.15f, -0.06f},
    {0.315f, -0.15f, 0.06f},
    {-0.315f, -0.15f, 0.06f},

    {0.0f, 0.0f, -0.50f},
    {0.0f, 0.1f, -0.60f},
    {0.0f, 0.0f, -0.60f}};

// UVs for the built-in aircraft fallback mesh.
// 内置飞机备用 mesh 的 UV 数据。
const HE3D::float2 flightFallbackAircraftUvs[] = {
    {0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1},
    {0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1},
    {0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1},
    {0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1},
    {0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1},
    {0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1},
    {0, 0}, {1, 0}, {0.5f, 1}};

// Return deterministic pseudo-random noise at an integer grid point.
// 返回整数网格点上的确定性伪随机噪声。
static float FlightNoiseGrid(HE3D::int32_t x, HE3D::int32_t z) {
    int n = x + z * 57;
    n = (n << 13) ^ n;
    int mixed = n * (n * n * 15731 + 789221) + 1376312589;
    return 1.0f - (float)(mixed & 0x7fffffff) / 1073741824.0f;
}

// Smooth interpolation weight for terrain noise.
// 计算地形噪声使用的平滑插值权重。
static float SmoothStep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// Linearly interpolate between two values.
// 在两个数值之间做线性插值。
static float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Sample smoothed value noise at a floating-point terrain coordinate.
// 在浮点地形坐标处采样平滑值噪声。
static float FlightNoise(float x, float z) {
    int ix = (int)HE3D::floorf(x);
    int iz = (int)HE3D::floorf(z);
    float fx = x - (float)ix;
    float fz = z - (float)iz;
    float sx = SmoothStep(fx);
    float sz = SmoothStep(fz);

    float n00 = FlightNoiseGrid(ix, iz);
    float n10 = FlightNoiseGrid(ix + 1, iz);
    float n01 = FlightNoiseGrid(ix, iz + 1);
    float n11 = FlightNoiseGrid(ix + 1, iz + 1);
    float nx0 = Lerp(n00, n10, sx);
    float nx1 = Lerp(n01, n11, sx);
    return Lerp(nx0, nx1, sz);
}

// Return procedural terrain height at a world-space x/z coordinate.
// 返回世界空间 x/z 坐标处的程序化地形高度。
float TerrainHeightAt(float x, float z) {
    float large = FlightNoise(x * 0.05f, z * 0.05f) * 8.0f;
    float small = FlightNoise(x * 0.2f, z * 0.2f) * 1.5f;
    return large + small;
}

// Create a terrain mesh tile centered at the given world coordinate.
// 创建以指定世界坐标为中心的地形 mesh 块。
HE3D::Mesh *CreatePlane(HE3D::int32_t gridCount, float step, float worldX, float worldZ) {
    int triCount = (gridCount - 1) * (gridCount - 1) * 2;
    int vertCount = triCount * 3;

    HE3D::Mesh *mesh = HE3D::Mesh::Create(vertCount);
    if (!mesh) {
        return nullptr;
    }

    if (!UpdatePlaneMesh(mesh, gridCount, step, worldX, worldZ)) {
        delete mesh;
        return nullptr;
    }
    return mesh;
}

// Rewrite an existing terrain mesh tile for a new world coordinate.
// 将已有地形 mesh 块改写到新的世界坐标。
bool UpdatePlaneMesh(HE3D::Mesh *mesh, HE3D::int32_t gridCount,
                     float step, float worldX, float worldZ) {
    if (!mesh || !mesh->vertices || !mesh->uvs || gridCount < 2) {
        return false;
    }

    int triCount = (gridCount - 1) * (gridCount - 1) * 2;
    int vertCount = triCount * 3;
    if (mesh->capacity < vertCount) {
        return false;
    }

    float half = (float)(gridCount - 1) * step * 0.5f;
    const int maxCachedGrid = 32;
    float heightCache[maxCachedGrid * maxCachedGrid];
    bool useHeightCache = gridCount <= maxCachedGrid;
    if (useHeightCache) {
        for (int z = 0; z < gridCount; z++) {
            float lz = z * step - half;
            float wz = lz + worldZ;
            for (int x = 0; x < gridCount; x++) {
                float lx = x * step - half;
                heightCache[z * gridCount + x] = TerrainHeightAt(lx + worldX, wz);
            }
        }
    }

    mesh->vertCount = 0;
    for (int z = 0; z < gridCount - 1; z++) {
        for (int x = 0; x < gridCount - 1; x++) {
            float lx0 = x * step - half;
            float lz0 = z * step - half;
            float lx1 = (x + 1) * step - half;
            float lz1 = (z + 1) * step - half;
            float wx0 = lx0 + worldX;
            float wz0 = lz0 + worldZ;
            float wx1 = lx1 + worldX;
            float wz1 = lz1 + worldZ;

            float h1 = useHeightCache ? heightCache[z * gridCount + x]
                                       : TerrainHeightAt(wx0, wz0);
            float h2 = useHeightCache ? heightCache[(z + 1) * gridCount + x]
                                       : TerrainHeightAt(wx0, wz1);
            float h3 = useHeightCache ? heightCache[z * gridCount + x + 1]
                                       : TerrainHeightAt(wx1, wz0);
            float h4 = useHeightCache ? heightCache[(z + 1) * gridCount + x + 1]
                                       : TerrainHeightAt(wx1, wz1);

            HE3D::float3 v1 = {lx0, h1, lz0};
            HE3D::float3 v2 = {lx0, h2, lz1};
            HE3D::float3 v3 = {lx1, h3, lz0};
            HE3D::float3 v4 = {lx1, h4, lz1};

            int i = mesh->vertCount;
            if (i + 6 > mesh->capacity) {
                return false;
            }

            mesh->vertices[i] = v1;
            mesh->vertices[i + 1] = v2;
            mesh->vertices[i + 2] = v3;
            mesh->uvs[i] = {0, 0};
            mesh->uvs[i + 1] = {0, 1};
            mesh->uvs[i + 2] = {1, 0};

            mesh->vertices[i + 3] = v3;
            mesh->vertices[i + 4] = v2;
            mesh->vertices[i + 5] = v4;
            mesh->uvs[i + 3] = {1, 0};
            mesh->uvs[i + 4] = {0, 1};
            mesh->uvs[i + 5] = {1, 1};
            mesh->vertCount += 6;
        }
    }
    mesh->RecalculateTriangleNormals();
    return mesh->vertCount == vertCount;
}
