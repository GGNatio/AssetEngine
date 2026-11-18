
#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <unordered_set>
#include <cmath>

#include "BinaryWriter.h"
#include "BinaryReader.h"
#include "Serializer.h"
// ============================================================
// Basic physics structures
// ============================================================

namespace Physics
{
    struct Vector3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct Quaternion
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;
    };

    enum class ColliderType : std::uint8_t
    {
        Sphere = 0,
        Box = 1
    };

    struct Collider
    {
        ColliderType type = ColliderType::Sphere;

        // Sphere: x = radius
        // Box: x, y, z = half-extents
        Vector3 dimensions;
    };

    struct RigidBody
    {
        std::uint64_t id = 0;

        bool isStatic = false;

        Vector3 position;
        Quaternion orientation;

        float mass = 1.0f;
        Vector3 inertia;

        Vector3 linearVelocity;
        Vector3 angularVelocity;

        Collider collider;
    };

    struct PhysicsScene
    {
        Vector3 gravity = { 0.0f, -9.81f, 0.0f };

        float fixedTimeStep = 1.0f / 60.0f;

        std::vector<RigidBody> bodies;
    };
}

// ============================================================
// Physics adapter serializer
// ============================================================

template<>
struct Serializer<Physics::PhysicsScene>
{
    static constexpr std::uint32_t VERSION = 1;
    static constexpr std::uint32_t MAX_BODIES = 100000;

    static void WriteVector3(
        BinaryWriter& writer,
        const Physics::Vector3& value)
    {
        writer.WriteFloat(value.x);
        writer.WriteFloat(value.y);
        writer.WriteFloat(value.z);
    }

    static Physics::Vector3 ReadVector3(BinaryReader& reader)
    {
        Physics::Vector3 value;

        value.x = reader.ReadFloat();
        value.y = reader.ReadFloat();
        value.z = reader.ReadFloat();

        ValidateVector3(value);

        return value;
    }

    static void WriteQuaternion(
        BinaryWriter& writer,
        const Physics::Quaternion& value)
    {
        writer.WriteFloat(value.x);
        writer.WriteFloat(value.y);
        writer.WriteFloat(value.z);
        writer.WriteFloat(value.w);
    }

    static Physics::Quaternion ReadQuaternion(BinaryReader& reader)
    {
        Physics::Quaternion value;

        value.x = reader.ReadFloat();
        value.y = reader.ReadFloat();
        value.z = reader.ReadFloat();
        value.w = reader.ReadFloat();

        if (!std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z) ||
            !std::isfinite(value.w))
        {
            throw std::runtime_error("Invalid quaternion");
        }

        return value;
    }

    static void ValidateVector3(const Physics::Vector3& value)
    {
        if (!std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z))
        {
            throw std::runtime_error("Invalid Vector3 value");
        }
    }

    static void Write(
        BinaryWriter& writer,
        const Physics::PhysicsScene& scene)
    {
        if (scene.bodies.size() > MAX_BODIES)
            throw std::length_error("Too many physics bodies");

        if (!std::isfinite(scene.fixedTimeStep) ||
            scene.fixedTimeStep <= 0.0f)
        {
            throw std::runtime_error("Invalid fixed timestep");
        }

        // Scene header
        writer.WriteU32(VERSION);

        WriteVector3(writer, scene.gravity);
        writer.WriteFloat(scene.fixedTimeStep);

        writer.WriteU32(
            static_cast<std::uint32_t>(scene.bodies.size()));

        std::unordered_set<std::uint64_t> ids;

        for (const Physics::RigidBody& body : scene.bodies)
        {
            if (!ids.insert(body.id).second)
                throw std::runtime_error("Duplicate physics body ID");

            ValidateVector3(body.position);
            ValidateVector3(body.inertia);
            ValidateVector3(body.linearVelocity);
            ValidateVector3(body.angularVelocity);
            ValidateVector3(body.collider.dimensions);

            if (!std::isfinite(body.mass) || body.mass < 0.0f)
                throw std::runtime_error("Invalid body mass");

            if (body.isStatic && body.mass != 0.0f)
                throw std::runtime_error(
                    "Static bodies must have zero mass");

            if (!body.isStatic && body.mass <= 0.0f)
                throw std::runtime_error(
                    "Dynamic bodies must have positive mass");

            if (body.collider.type != Physics::ColliderType::Sphere &&
                body.collider.type != Physics::ColliderType::Box)
            {
                throw std::runtime_error("Invalid collider type");
            }

            if (body.collider.type == Physics::ColliderType::Sphere &&
                body.collider.dimensions.x <= 0.0f)
            {
                throw std::runtime_error("Invalid sphere radius");
            }

            if (body.collider.type == Physics::ColliderType::Box &&
                (body.collider.dimensions.x <= 0.0f ||
                    body.collider.dimensions.y <= 0.0f ||
                    body.collider.dimensions.z <= 0.0f))
            {
                throw std::runtime_error("Invalid box dimensions");
            }

            // Body ID and type
            writer.WriteU64(body.id);
            writer.WriteBool(body.isStatic);

            // Transform
            WriteVector3(writer, body.position);
            WriteQuaternion(writer, body.orientation);

            // Physical properties
            writer.WriteFloat(body.mass);
            WriteVector3(writer, body.inertia);

            // Velocities
            WriteVector3(writer, body.linearVelocity);
            WriteVector3(writer, body.angularVelocity);

            // Collider
            writer.WriteU8(
                static_cast<std::uint8_t>(body.collider.type));

            WriteVector3(writer, body.collider.dimensions);
        }
    }

    static Physics::PhysicsScene Read(BinaryReader& reader)
    {
        const std::uint32_t version = reader.ReadU32();

        if (version != VERSION)
            throw std::runtime_error(
                "Unsupported PhysicsScene version");

        Physics::PhysicsScene scene;

        scene.gravity = ReadVector3(reader);

        scene.fixedTimeStep = reader.ReadFloat();

        if (!std::isfinite(scene.fixedTimeStep) ||
            scene.fixedTimeStep <= 0.0f)
        {
            throw std::runtime_error("Invalid fixed timestep");
        }

        const std::uint32_t count = reader.ReadU32();

        // Each body occupies at least 90 bytes in this format.
        constexpr std::size_t BODY_SIZE = 90;

        if (count > MAX_BODIES ||
            count > reader.GetRemaining() / BODY_SIZE)
        {
            throw std::runtime_error("Invalid physics body count");
        }

        scene.bodies.reserve(count);

        std::unordered_set<std::uint64_t> ids;

        for (std::uint32_t i = 0; i < count; ++i)
        {
            Physics::RigidBody body;

            body.id = reader.ReadU64();

            if (!ids.insert(body.id).second)
                throw std::runtime_error(
                    "Duplicate physics body ID");

            body.isStatic = reader.ReadBool();

            body.position = ReadVector3(reader);
            body.orientation = ReadQuaternion(reader);

            body.mass = reader.ReadFloat();
            body.inertia = ReadVector3(reader);

            body.linearVelocity = ReadVector3(reader);
            body.angularVelocity = ReadVector3(reader);

            if (!std::isfinite(body.mass) || body.mass < 0.0f)
                throw std::runtime_error("Invalid body mass");

            if (body.isStatic && body.mass != 0.0f)
                throw std::runtime_error("Invalid static body mass");

            if (!body.isStatic && body.mass <= 0.0f)
                throw std::runtime_error("Invalid dynamic body mass");

            const std::uint8_t colliderType = reader.ReadU8();

            if (colliderType >
                static_cast<std::uint8_t>(Physics::ColliderType::Box))
            {
                throw std::runtime_error("Invalid collider type");
            }

            body.collider.type =
                static_cast<Physics::ColliderType>(colliderType);

            body.collider.dimensions = ReadVector3(reader);

            if (body.collider.type == Physics::ColliderType::Sphere &&
                body.collider.dimensions.x <= 0.0f)
            {
                throw std::runtime_error("Invalid sphere radius");
            }

            if (body.collider.type == Physics::ColliderType::Box &&
                (body.collider.dimensions.x <= 0.0f ||
                    body.collider.dimensions.y <= 0.0f ||
                    body.collider.dimensions.z <= 0.0f))
            {
                throw std::runtime_error("Invalid box dimensions");
            }

            scene.bodies.push_back(body);
        }

        if (reader.GetRemaining() != 0)
            throw std::runtime_error(
                "Unexpected data at the end of PhysicsScene");

        return scene;
    }
};

// ============================================================
// Snapshot and restoration helpers
// ============================================================

namespace Physics
{
    // Call at the end of a simulation step.
    inline std::vector<std::byte> CaptureSnapshot(
        const PhysicsScene& world)
    {
        BinaryWriter writer;

        Serializer<PhysicsScene>::Write(writer, world);

        return writer.GetBuffer();
    }

    // Builds a new world. The caller can replace its current
    // world only after this function succeeds.
    inline PhysicsScene RestoreSnapshot(
        const std::vector<std::byte>& data)
    {
        BinaryReader reader(data);

        return Serializer<PhysicsScene>::Read(reader);
    }

    // Replaces an existing world only after successful decoding.
    inline void ReplaceWorld(
        PhysicsScene& currentWorld,
        const std::vector<std::byte>& data)
    {
        PhysicsScene restoredWorld = RestoreSnapshot(data);

        currentWorld = std::move(restoredWorld);
    }
}
