
#include <iostream>
#include <exception>
#include <cmath>

#include "RobustnessTests.h"
#include "PhysicsAdapter.h"

void TestPhysicsAdapter()
{
    std::cout << "\n====================================\n";
    std::cout << "       PHYSICS ADAPTER TEST\n";
    std::cout << "====================================\n";

    // Create an example physics scene
    Physics::PhysicsScene originalScene;

    originalScene.gravity = { 0.0f, -9.81f, 0.0f };
    originalScene.fixedTimeStep = 1.0f / 60.0f;

    // Static floor
    Physics::RigidBody floor;

    floor.id = 1001;
    floor.isStatic = true;
    floor.position = { 0.0f, -2.0f, 0.0f };
    floor.mass = 0.0f;
    floor.collider.type = Physics::ColliderType::Box;
    floor.collider.dimensions = { 10.0f, 1.0f, 10.0f };

    // Dynamic box
    Physics::RigidBody box;

    box.id = 1002;
    box.isStatic = false;
    box.position = { 0.0f, 5.0f, 0.0f };
    box.mass = 2.0f;
    box.inertia = { 1.0f, 1.0f, 1.0f };
    box.linearVelocity = { 0.0f, -1.5f, 0.0f };
    box.angularVelocity = { 0.0f, 0.2f, 0.0f };
    box.collider.type = Physics::ColliderType::Sphere;
    box.collider.dimensions = { 0.5f, 0.0f, 0.0f };

    originalScene.bodies.push_back(floor);
    originalScene.bodies.push_back(box);

    std::cout << "Original scene bodies: "
        << originalScene.bodies.size() << '\n';

    // Capture the snapshot
    std::vector<std::byte> snapshot =
        Physics::CaptureSnapshot(originalScene);

    std::cout << "Snapshot size: "
        << snapshot.size() << " bytes\n";

    // Save snapshot as an archive asset
    AssetArchive archive;

    archive.AddAsset(
        5001,
        3,
        snapshot);

    archive.Save("physics_scene.gca");

    std::cout << "Physics archive saved.\n";

    // Load archive
    AssetArchive loadedArchive =
        AssetArchive::Load("physics_scene.gca");

    auto physicsData =
        loadedArchive.GetAsset(5001, 3);

    // Restore the physics scene
    Physics::PhysicsScene restoredScene =
        Physics::RestoreSnapshot(physicsData);

    std::cout << "\nRestored scene:\n";

    std::cout << "Gravity Y: "
        << restoredScene.gravity.y << '\n';

    std::cout << "Fixed timestep: "
        << restoredScene.fixedTimeStep << '\n';

    std::cout << "Bodies: "
        << restoredScene.bodies.size() << '\n';

    for (const Physics::RigidBody& body : restoredScene.bodies)
    {
        std::cout << "\nBody ID: " << body.id << '\n';

        std::cout << "Static: "
            << (body.isStatic ? "true" : "false") << '\n';

        std::cout << "Position: "
            << body.position.x << ", "
            << body.position.y << ", "
            << body.position.z << '\n';

        std::cout << "Mass: " << body.mass << '\n';

        std::cout << "Linear velocity: "
            << body.linearVelocity.x << ", "
            << body.linearVelocity.y << ", "
            << body.linearVelocity.z << '\n';
    }

    // Verify restoration
    if (restoredScene.bodies.size() != 2)
        throw std::runtime_error("Physics body count mismatch");

    if (restoredScene.bodies[0].id != 1001 ||
        restoredScene.bodies[1].id != 1002)
    {
        throw std::runtime_error("Physics body ID mismatch");
    }

    if (std::abs(restoredScene.bodies[1].position.y - 5.0f) > 0.001f)
        throw std::runtime_error("Physics position mismatch");

    if (std::abs(restoredScene.bodies[1].mass - 2.0f) > 0.001f)
        throw std::runtime_error("Physics mass mismatch");

    // Test replacing an existing world
    Physics::PhysicsScene existingWorld;

    Physics::ReplaceWorld(existingWorld, physicsData);

    if (existingWorld.bodies.size() != 2)
        throw std::runtime_error("World replacement failed");

    std::cout << "\nPhysics snapshot: SUCCESS\n";
    std::cout << "World restoration: SUCCESS\n";
}

int main()
{
    try
    {
        RobustnessTests::RunAll();

        TestPhysicsAdapter();

        std::cout << "\n====================================\n";
        std::cout << "       ALL TESTS PASSED\n";
        std::cout << "====================================\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "\nERROR: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
