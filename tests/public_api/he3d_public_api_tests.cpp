#include "he3d.hpp"

#include <cstdio>

static int g_failures = 0;

static void Check(bool condition, const char *name)
{
    if (!condition)
    {
        std::printf("FAIL: %s\n", name);
        g_failures++;
    }
}

static bool Near(float a, float b, float tolerance)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= tolerance;
}

static float FlatHeight(float, float, void *)
{
    return 0.0f;
}

static void MathFunctionsMatchWorkedExamples()
{
    Check(Near(HE3D::fracf(-0.25f), 0.75f, 0.0001f), "fracf wraps negative fractions into positive UV range");

    HE3D::float3 x = {1.0f, 0.0f, 0.0f};
    HE3D::float3 y = {0.0f, 1.0f, 0.0f};
    HE3D::float3 z = HE3D::float3::cross(x, y);
    Check(Near(z.x, 0.0f, 0.0001f) && Near(z.y, 0.0f, 0.0001f) && Near(z.z, 1.0f, 0.0001f),
          "float3 cross product follows right-handed basis");

    HE3D::quat yaw = HE3D::quat::FromEuler({0.0f, HE3D::HE3D_PI_DIV_2, 0.0f});
    HE3D::float3 forward = yaw.rotate({0.0f, 0.0f, 1.0f});
    Check(Near(forward.x, 1.0f, 0.01f) && Near(forward.z, 0.0f, 0.01f),
          "quat yaw rotates local forward toward +X");
}

static void PlatformStateFunctionsClampAndReportValues()
{
    HE3D::SetFrameRateLimit(144);
    Check(HE3D::GetFrameRateLimit() == 144, "frame-rate limit reports the value that was set");

    HE3D::SetFxaaEnabled(true);
    HE3D::SetTaaEnabled(false);
    HE3D::SetMsaaEnabled(true);
    Check(HE3D::IsFxaaEnabled(), "FXAA state reports enabled");
    Check(!HE3D::IsTaaEnabled(), "TAA state reports disabled");
    Check(HE3D::IsMsaaEnabled(), "MSAA state reports enabled");

    HE3D::SetSsaaScale(0);
    Check(HE3D::GetSsaaScale() == 1, "SSAA scale clamps low values to 1");
    HE3D::SetSsaaScale(99);
    Check(HE3D::GetSsaaScale() == 4, "SSAA scale clamps high values to 4");
}

static void ResourceLoadersCreateMeshFromObjFile()
{
    const char *path = "/tmp/he3d_tdd_triangle.obj";
    FILE *file = std::fopen(path, "wb");
    Check(file != nullptr, "test can create temporary OBJ file");
    if (!file)
    {
        return;
    }

    const char obj[] =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "f 1/1 2/2 3/3\n";
    std::fwrite(obj, 1, sizeof(obj) - 1, file);
    std::fclose(file);

    HE3D::Mesh *mesh = HE3D::Mesh::LoadOBJ(path);
    Check(mesh != nullptr, "Mesh::LoadOBJ loads a simple triangle OBJ");
    if (mesh)
    {
        Check(mesh->vertCount == 3, "OBJ triangle expands to three vertices");
        Check(Near(mesh->vertices[1].x, 1.0f, 0.0001f), "OBJ loader preserves vertex positions");
        Check(Near(mesh->uvs[2].y, 0.0f, 0.0001f), "OBJ loader converts OBJ V to HE3D texture-space V");
        delete mesh;
    }

    std::remove(path);
}

static void TextureSamplingWrapsUvsAndReportsInvalidTexture()
{
    HE3D::Texture invalid;
    HE3D::color3 invalidSample = invalid.Sample(0.0f, 0.0f);
    Check(Near(invalidSample.r, 1.0f, 0.0001f) &&
          Near(invalidSample.g, 0.0f, 0.0001f) &&
          Near(invalidSample.b, 1.0f, 0.0001f),
          "invalid texture samples as magenta");

    HE3D::Texture texture;
    texture.width = 2;
    texture.height = 2;
    texture.valid = true;
    texture.pixels = new HE3D::ColorA[4];
    texture.pixels[0] = {255, 0, 0, 255};
    texture.pixels[1] = {0, 255, 0, 255};
    texture.pixels[2] = {0, 0, 255, 255};
    texture.pixels[3] = {255, 255, 255, 255};

    HE3D::color3 wrapped = texture.Sample(1.25f, -0.25f);
    Check(Near(wrapped.r, 0.0f, 0.0001f) &&
          Near(wrapped.g, 0.0f, 0.0001f) &&
          Near(wrapped.b, 1.0f, 0.0001f),
          "Texture::Sample wraps UVs before nearest-neighbor lookup");
}

static void PrimitiveMeshFactoriesCreateDrawableMeshes()
{
    HE3D::Mesh *triangle = HE3D::Mesh::CreateTriangle(2.0f, 3.0f);
    HE3D::Mesh *plane = HE3D::Mesh::CreatePlane(4.0f, 5.0f);
    HE3D::Mesh *cube = HE3D::Mesh::CreateCube(2.0f, 3.0f, 4.0f);
    HE3D::Mesh *sphere = HE3D::Mesh::CreateSphere(1.0f, 3, 2);

    Check(triangle != nullptr, "Mesh::CreateTriangle creates a mesh");
    Check(plane != nullptr, "Mesh::CreatePlane creates a mesh");
    Check(cube != nullptr, "Mesh::CreateCube creates a mesh");
    Check(sphere != nullptr, "Mesh::CreateSphere creates a mesh");

    if (triangle)
    {
        Check(triangle->vertCount == 3, "triangle primitive has one triangle");
        Check(Near(triangle->vertices[0].x, -1.0f, 0.0001f) &&
              Near(triangle->vertices[1].y, 1.5f, 0.0001f),
              "triangle primitive uses requested width and height");
    }
    if (plane)
    {
        Check(plane->vertCount == 6, "plane primitive has two triangles");
        Check(Near(plane->vertices[0].x, -2.0f, 0.0001f) &&
              Near(plane->vertices[0].z, -2.5f, 0.0001f),
              "plane primitive is centered on the XZ plane");
    }
    if (cube)
    {
        Check(cube->vertCount == 36, "cube primitive has twelve triangles");
        HE3D::GameObject object;
        object.mesh = cube;
        HE3D::CollisionBox box(&object);
        Check(box.FitMesh(), "collision box fits cube primitive");
        Check(Near(box.halfExtents.x, 1.0f, 0.0001f) &&
              Near(box.halfExtents.y, 1.5f, 0.0001f) &&
              Near(box.halfExtents.z, 2.0f, 0.0001f),
              "cube primitive uses requested extents");
    }
    if (sphere)
    {
        Check(sphere->vertCount == 36, "sphere primitive uses segments * rings * 6 vertices");
    }

    delete triangle;
    delete plane;
    delete cube;
    delete sphere;

    HE3D::Mesh *invalid = HE3D::Mesh::CreateCube(1.0f, 0.0f, 1.0f);
    HE3D::Mesh *clampedSphere = HE3D::Mesh::CreateSphere(1.0f, 1, 1);
    Check(invalid == nullptr, "primitive factories reject non-positive dimensions");
    Check(clampedSphere != nullptr && clampedSphere->vertCount == 36,
          "sphere primitive clamps low segment and ring counts");
    delete clampedSphere;
}

static HE3D::Mesh *CreateTestTriangleMesh()
{
    return HE3D::Mesh::CreateTriangle(1.0f, 1.0f);
}

static HE3D::Mesh *CreateTestBoxMesh()
{
    return HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
}

static void RendererDrawsValidObjectAcrossResize()
{
    HE3D::WindowDesc desc;
    desc.width = 64;
    desc.height = 48;
    desc.title = "HE3D test";
    desc.flags = 0;

    HE3D::Window *window = HE3D::CreateWindow(&desc);
    Check(window != nullptr, "CreateWindow returns a test window");
    if (!window)
    {
        return;
    }

    HE3D::Renderer renderer(window, desc.width, desc.height);
    HE3D::Camera camera;
    camera.position = {0.0f, 0.0f, 0.0f};
    camera.fov = 70.0f;

    HE3D::GameObject object;
    object.mesh = CreateTestTriangleMesh();
    object.position = {0.0f, 0.0f, 3.0f};
    Check(object.mesh != nullptr, "renderer test mesh is created");

    if (object.mesh)
    {
        renderer.Clear({0.0f, 0.0f, 0.0f});
        renderer.DrawGameObject(object, camera, HE3D::color3(1.0f, 0.0f, 0.0f));
        renderer.Resize(80, 60);
        renderer.Clear({0.0f, 0.0f, 0.0f});
        renderer.DrawGameObject(object, camera, HE3D::color3(0.0f, 1.0f, 0.0f));
    }

    delete object.mesh;
    HE3D::DestroyWindow(window);
}

static void HeightfieldResolutionPushesBodyAboveTerrain()
{
    HE3D::float3 vertices[6] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.0f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.0f,  0.5f,  0.5f}
    };
    HE3D::float2 uvs[6] = {
        {0, 0}, {1, 0}, {0.5f, 1},
        {0, 0}, {1, 0}, {0.5f, 1}
    };

    HE3D::GameObject object;
    object.mesh = HE3D::Mesh::Create(vertices, uvs, 6);
    object.position = {0.0f, 0.4f, 0.0f};

    HE3D::CollisionBox box(&object);
    Check(object.mesh != nullptr, "mesh creation succeeds for heightfield test");
    Check(box.FitMesh(), "collision box fits triangle mesh");

    HE3D::PhysicsBody body(&object);
    body.SetEnabled(true);
    body.SetVelocity({0.0f, -3.0f, 0.0f});

    HE3D::HeightFieldCollider heightfield(FlatHeight, nullptr, 3, 1.0f);
    HE3D::PhysicsContact contact;
    bool hit = body.ResolveHeightField(box, heightfield, 0.0f, &contact);

    Check(hit, "heightfield resolution reports contact");
    Check(contact.hit, "heightfield contact result is marked as hit");
    Check(object.position.y > 0.5f, "heightfield resolution moves body above terrain");
    Check(body.velocity.y >= 0.0f, "heightfield resolution removes downward velocity");

    delete object.mesh;
}

static void CollisionResolutionRemovesVelocityIntoObstacle()
{
    HE3D::GameObject mover;
    mover.mesh = CreateTestBoxMesh();
    mover.position = {0.0f, 0.0f, 0.0f};
    HE3D::CollisionBox moverBox(&mover);
    Check(mover.mesh != nullptr, "mover mesh is created for collision test");
    Check(moverBox.FitMesh(), "mover collision box fits mesh");

    HE3D::GameObject obstacleObject;
    obstacleObject.mesh = CreateTestBoxMesh();
    obstacleObject.position = {0.80f, 0.0f, 0.0f};
    obstacleObject.orientation = HE3D::quat::FromEuler({0.0f, 0.0f, 0.40f});
    HE3D::CollisionBox obstacle(&obstacleObject);
    Check(obstacleObject.mesh != nullptr, "obstacle mesh is created for collision test");
    Check(obstacle.FitMesh(), "obstacle collision box fits mesh");

    HE3D::PhysicsBody body(&mover);
    body.SetEnabled(true);
    body.SetVelocity({2.0f, 0.0f, 0.0f});

    HE3D::PhysicsContact contact;
    bool collided = body.StepWithCollisions(0.01f, moverBox, &obstacle, 1, 1, &contact);

    Check(collided, "StepWithCollisions reports overlapping obstacle");
    Check(contact.hit, "StepWithCollisions writes contact hit");
    Check(HE3D::float3::dot(body.velocity, contact.normal) >= -0.00001f,
          "collision resolution removes velocity into obstacle");

    delete mover.mesh;
    delete obstacleObject.mesh;
}

int main()
{
    MathFunctionsMatchWorkedExamples();
    PlatformStateFunctionsClampAndReportValues();
    ResourceLoadersCreateMeshFromObjFile();
    TextureSamplingWrapsUvsAndReportsInvalidTexture();
    PrimitiveMeshFactoriesCreateDrawableMeshes();
    RendererDrawsValidObjectAcrossResize();
    HeightfieldResolutionPushesBodyAboveTerrain();
    CollisionResolutionRemovesVelocityIntoObstacle();

    if (g_failures != 0)
    {
        std::printf("%d HE3D test(s) failed\n", g_failures);
        return 1;
    }

    std::printf("HE3D public API tests passed\n");
    return 0;
}
