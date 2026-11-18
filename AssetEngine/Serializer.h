
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

#include "BinaryWriter.h"
#include "BinaryReader.h"

namespace AssetData
{
    struct PlayerProfile
    {
        std::string name;
        std::int32_t level = 1;
        float rating = 0.0f;
        bool premium = false;

        // New field added in version 2
        std::uint32_t experience = 0;
    };

    struct QuestAsset
    {
        std::string title;
        std::string description;
        std::vector<std::int32_t> objectives;
        bool completed = false;
    };
}

// ============================================================
// Generic serializer
// ============================================================

template<typename T>
struct Serializer;


// ============================================================
// PlayerProfile serializer
// Supports schema versions 1 and 2
// ============================================================

template<>
struct Serializer<AssetData::PlayerProfile>
{
    static constexpr std::uint32_t VERSION_1 = 1;
    static constexpr std::uint32_t VERSION_2 = 2;
    static constexpr std::uint32_t CURRENT_VERSION = VERSION_2;

    // Writes the current version (V2)
    static void Write(
        BinaryWriter& writer,
        const AssetData::PlayerProfile& data)
    {
        writer.WriteU32(CURRENT_VERSION);

        writer.WriteString(data.name);
        writer.WriteI32(data.level);
        writer.WriteFloat(data.rating);
        writer.WriteBool(data.premium);

        // Field introduced in V2
        writer.WriteU32(data.experience);
    }

    // Writes an old V1 profile.
    // Useful for compatibility tests and legacy archives.
    static void WriteVersion1(
        BinaryWriter& writer,
        const AssetData::PlayerProfile& data)
    {
        writer.WriteU32(VERSION_1);

        writer.WriteString(data.name);
        writer.WriteI32(data.level);
        writer.WriteFloat(data.rating);
        writer.WriteBool(data.premium);

        // No experience field in V1
    }

    // Reads both V1 and V2
    static AssetData::PlayerProfile Read(BinaryReader& reader)
    {
        const std::uint32_t version = reader.ReadU32();

        if (version != VERSION_1 && version != VERSION_2)
        {
            throw std::runtime_error(
                "Unsupported PlayerProfile schema version");
        }

        AssetData::PlayerProfile data;

        // Fields shared by V1 and V2
        data.name = reader.ReadString();
        data.level = reader.ReadI32();
        data.rating = reader.ReadFloat();
        data.premium = reader.ReadBool();

        if (data.level < 0)
            throw std::runtime_error("Invalid player level");

        if (version == VERSION_1)
        {
            // New fields receive their default value
            data.experience = 0;
        }
        else if (version == VERSION_2)
        {
            data.experience = reader.ReadU32();
        }

        // Reject unexpected additional data
        if (reader.GetRemaining() != 0)
        {
            throw std::runtime_error(
                "Unexpected data at the end of PlayerProfile");
        }

        return data;
    }
};


// ============================================================
// QuestAsset serializer
// ============================================================

template<>
struct Serializer<AssetData::QuestAsset>
{
    static void Write(
        BinaryWriter& writer,
        const AssetData::QuestAsset& data)
    {
        writer.WriteString(data.title);
        writer.WriteString(data.description);
        writer.WriteInt32Vector(data.objectives);
        writer.WriteBool(data.completed);
    }

    static AssetData::QuestAsset Read(BinaryReader& reader)
    {
        AssetData::QuestAsset data;

        data.title = reader.ReadString();
        data.description = reader.ReadString();
        data.objectives = reader.ReadInt32Vector();
        data.completed = reader.ReadBool();

        if (reader.GetRemaining() != 0)
        {
            throw std::runtime_error(
                "Unexpected data at the end of QuestAsset");
        }

        return data;
    }
};
