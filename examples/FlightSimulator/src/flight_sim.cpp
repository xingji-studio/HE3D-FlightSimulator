/*
 * HE3D Flight Simulator - simulation-specific mesh data.
 */
#include "flight_sim.hpp"

using namespace HE3D;

const int FLIGHT_FALLBACK_AIRCRAFT_VERTEX_COUNT = 39;

const float3 FLIGHT_FALLBACK_AIRCRAFT_VERTICES[] = {
    { 0.07f, 0.0f, 0.45f}, { 0.0f, 0.07f, 0.45f}, { 0.07f, 0.0f, -0.45f},
    { 0.0f, 0.07f, 0.45f}, { 0.0f, 0.07f, -0.45f}, { 0.07f, 0.0f, -0.45f},
    { 0.0f, 0.07f, 0.45f}, {-0.07f, 0.0f, 0.45f}, { 0.0f, 0.07f, -0.45f},
    {-0.07f, 0.0f, 0.45f}, {-0.07f, 0.0f, -0.45f}, { 0.0f, 0.07f, -0.45f},
    {-0.07f, 0.0f, 0.45f}, { 0.0f,-0.07f, 0.45f}, {-0.07f, 0.0f, -0.45f},
    { 0.0f,-0.07f, 0.45f}, { 0.0f,-0.07f, -0.45f}, {-0.07f, 0.0f, -0.45f},
    { 0.0f,-0.07f, 0.45f}, { 0.07f, 0.0f, 0.45f}, { 0.0f,-0.07f, -0.45f},
    { 0.07f, 0.0f, 0.45f}, { 0.07f, 0.0f, -0.45f}, { 0.0f,-0.07f, -0.45f},

    {-0.35f, 0.15f,-0.06f}, { 0.35f, 0.15f,-0.06f}, {-0.35f, 0.15f, 0.06f},
    { 0.35f, 0.15f,-0.06f}, { 0.35f, 0.15f, 0.06f}, {-0.35f, 0.15f, 0.06f},

    {-0.315f,-0.15f,-0.06f}, { 0.315f,-0.15f,-0.06f}, {-0.315f,-0.15f, 0.06f},
    { 0.315f,-0.15f,-0.06f}, { 0.315f,-0.15f, 0.06f}, {-0.315f,-0.15f, 0.06f},

    { 0.0f, 0.0f,-0.50f}, { 0.0f, 0.1f,-0.60f}, { 0.0f, 0.0f,-0.60f}
};

const float2 FLIGHT_FALLBACK_AIRCRAFT_UVS[] = {
    {0,0}, {1,0}, {0,1}, {1,0}, {1,1}, {0,1},
    {0,0}, {1,0}, {0,1}, {1,0}, {1,1}, {0,1},
    {0,0}, {1,0}, {0,1}, {1,0}, {1,1}, {0,1},
    {0,0}, {1,0}, {0,1}, {1,0}, {1,1}, {0,1},
    {0,0}, {1,0}, {0,1}, {1,0}, {1,1}, {0,1},
    {0,0}, {1,0}, {0,1}, {1,0}, {1,1}, {0,1},
    {0,0}, {1,0}, {0.5f,1}
};


static float FlightNoise(int x, int z)
{
    int n = x + z * 57;
    n     = (n << 13) ^ n;
    int mixed = n * (n * n * 15731 + 789221) + 1376312589;
    return 1.0f - (float)(mixed & 0x7fffffff) / 1073741824.0f;
}

static float FlightTerrainHeight(float x, float z)
{
    float large = FlightNoise((int)(x * 0.05f), (int)(z * 0.05f)) * 8.0f;
    float small = FlightNoise((int)(x * 0.2f),  (int)(z * 0.2f))  * 1.5f;
    return large + small;
}

Mesh *CreatePlane(int gridCount, float step, float worldX, float worldZ)
{
    int triCount  = (gridCount - 1) * (gridCount - 1) * 2;
    int vertCount = triCount * 3;

    Mesh *mesh = Mesh::Create(vertCount);
    if (!mesh)
    {
        return nullptr;
    }

    if (!UpdatePlaneMesh(mesh, gridCount, step, worldX, worldZ))
    {
        delete mesh;
        return nullptr;
    }
    return mesh;
}

bool UpdatePlaneMesh(Mesh *mesh, int gridCount, float step, float worldX, float worldZ)
{
    if (!mesh || !mesh->vertices || !mesh->uvs || gridCount < 2)
    {
        return false;
    }

    int triCount  = (gridCount - 1) * (gridCount - 1) * 2;
    int vertCount = triCount * 3;
    if (mesh->capacity < vertCount)
    {
        return false;
    }

    float half = (float)(gridCount - 1) * step * 0.5f;
    const int MAX_CACHED_GRID = 32;
    float heightCache[MAX_CACHED_GRID * MAX_CACHED_GRID];
    bool useHeightCache = gridCount <= MAX_CACHED_GRID;
    if (useHeightCache)
    {
        for (int z = 0; z < gridCount; z++)
        {
            float lz = z * step - half;
            float wz = lz + worldZ;
            for (int x = 0; x < gridCount; x++)
            {
                float lx = x * step - half;
                heightCache[z * gridCount + x] = FlightTerrainHeight(lx + worldX, wz);
            }
        }
    }

    mesh->vertCount = 0;
    for (int z = 0; z < gridCount - 1; z++)
    {
        for (int x = 0; x < gridCount - 1; x++)
        {
            float lx0 = x * step - half;
            float lz0 = z * step - half;
            float lx1 = (x + 1) * step - half;
            float lz1 = (z + 1) * step - half;
            float wx0 = lx0 + worldX;
            float wz0 = lz0 + worldZ;
            float wx1 = lx1 + worldX;
            float wz1 = lz1 + worldZ;

            float h1 = useHeightCache ? heightCache[z * gridCount + x] : FlightTerrainHeight(wx0, wz0);
            float h2 = useHeightCache ? heightCache[(z + 1) * gridCount + x] : FlightTerrainHeight(wx0, wz1);
            float h3 = useHeightCache ? heightCache[z * gridCount + x + 1] : FlightTerrainHeight(wx1, wz0);
            float h4 = useHeightCache ? heightCache[(z + 1) * gridCount + x + 1] : FlightTerrainHeight(wx1, wz1);

            float3 v1 = {lx0, h1, lz0};
            float3 v2 = {lx0, h2, lz1};
            float3 v3 = {lx1, h3, lz0};
            float3 v4 = {lx1, h4, lz1};

            int i = mesh->vertCount;
            if (i + 6 > mesh->capacity)
            {
                return false;
            }

            mesh->vertices[i]     = v1;
            mesh->vertices[i + 1] = v2;
            mesh->vertices[i + 2] = v3;
            mesh->uvs[i]          = {0,0};
            mesh->uvs[i + 1]      = {0,1};
            mesh->uvs[i + 2]      = {1,0};

            mesh->vertices[i + 3] = v3;
            mesh->vertices[i + 4] = v2;
            mesh->vertices[i + 5] = v4;
            mesh->uvs[i + 3]      = {1,0};
            mesh->uvs[i + 4]      = {0,1};
            mesh->uvs[i + 5]      = {1,1};
            mesh->vertCount      += 6;
        }
    }
    mesh->RecalculateTriangleNormals();
    return mesh->vertCount == vertCount;
}
