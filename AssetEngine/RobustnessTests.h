
#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <functional>

#include "Archive.h"
#include "BinaryWriter.h"
#include "BinaryReader.h"
#include "Serializer.h"

namespace RobustnessTests
{
    inline int passed = 0;
    inline int failed = 0;

    inline void RunTest(
        const std::string& name,
        const std::function<void()>& test)
    {
        std::cout << "[TEST] " << name << " ... ";

        try
        {
            test();
            ++passed;
            std::cout << "PASSED\n";
        }
        catch (const std::exception& e)
        {
            ++failed;
            std::cout << "FAILED (" << e.what() << ")\n";
        }
    }

    inline void Require(bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    inline void ExpectException(const std::function<void()>& action)
    {
        bool exceptionThrown = false;

        try
        {
            action();
        }
        catch (const std::exception&)
        {
            exceptionThrown = true;
        }

        Require(exceptionThrown, "Expected an exception");
    }

    inline void RunAll()
    {
        passed = 0;
        failed = 0;

        std::cout << "\n====================================\n";
        std::cout << "       ROBUSTNESS TESTS\n";
        std::cout << "====================================\n\n";

        // 1. Empty archive
        RunTest("Empty archive", []()
            {
                AssetArchive archive;
                archive.Save("test_empty.gca");

                AssetArchive loaded =
                    AssetArchive::Load("test_empty.gca");

                Require(
                    loaded.GetEntries().empty(),
                    "Archive should contain no entries");
            });

        // 2. Empty collection
        RunTest("Empty collection serialization", []()
            {
                AssetData::QuestAsset quest;
                quest.title = "Empty Quest";
                quest.description = "No objectives";

                BinaryWriter writer;
                Serializer<AssetData::QuestAsset>::Write(writer, quest);

                BinaryReader reader(writer.GetBuffer());

                auto loaded =
                    Serializer<AssetData::QuestAsset>::Read(reader);

                Require(loaded.title == quest.title, "Title mismatch");
                Require(
                    loaded.objectives.empty(),
                    "Collection should be empty");
            });

        // 3. Missing asset ID
        RunTest("Missing asset ID", []()
            {
                AssetArchive archive;
                archive.AddAsset(100, 1, { std::byte{ 1 } });
                archive.Save("test_missing_id.gca");

                AssetArchive loaded =
                    AssetArchive::Load("test_missing_id.gca");

                ExpectException([&]()
                    {
                        loaded.GetAsset(999, 1);
                    });
            });

        // 4. Wrong asset type
        RunTest("Wrong asset type", []()
            {
                AssetArchive archive;
                archive.AddAsset(100, 1, { std::byte{ 1 } });
                archive.Save("test_wrong_type.gca");

                AssetArchive loaded =
                    AssetArchive::Load("test_wrong_type.gca");

                ExpectException([&]()
                    {
                        loaded.GetAsset(100, 2);
                    });
            });

        // 5. Truncated archive
        RunTest("Truncated archive", []()
            {
                AssetArchive archive;
                archive.AddAsset(100, 1, { std::byte{ 1 }, std::byte{ 2 } });
                archive.Save("test_truncated_source.gca");

                std::ifstream input(
                    "test_truncated_source.gca",
                    std::ios::binary);

                Require(input.good(), "Cannot open source archive");

                std::vector<char> bytes(
                    (std::istreambuf_iterator<char>(input)),
                    std::istreambuf_iterator<char>());

                Require(bytes.size() > 1, "Source archive is too small");

                bytes.pop_back();

                std::ofstream output(
                    "test_truncated.gca",
                    std::ios::binary);

                Require(output.good(), "Cannot create truncated archive");

                output.write(bytes.data(),
                    static_cast<std::streamsize>(bytes.size()));

                output.close();

                ExpectException([]()
                    {
                        AssetArchive::Load("test_truncated.gca");
                    });
            });

        // 6. Modified byte / invalid CRC
        RunTest("Modified archive byte", []()
            {
                AssetArchive archive;
                archive.AddAsset(100, 1, { std::byte{ 10 }, std::byte{ 20 } });
                archive.Save("test_corrupted.gca");

                std::fstream file(
                    "test_corrupted.gca",
                    std::ios::binary | std::ios::in | std::ios::out);

                Require(file.good(), "Cannot open archive");

                // Modify one byte in the archive header.
                file.seekg(4);
                char value = 0;
                file.read(&value, 1);

                Require(file.good(), "Cannot read archive byte");

                value ^= 0x01;

                file.seekp(4);
                file.write(&value, 1);
                file.close();

                ExpectException([]()
                    {
                        AssetArchive::Load("test_corrupted.gca");
                    });
            });

        // 7. Excessive string size
        RunTest("Excessive string size", []()
            {
                BinaryWriter writer;

                // Claim that the string contains 4 GB of data,
                // without actually providing that data.
                writer.WriteU32(0xFFFFFFFF);

                BinaryReader reader(writer.GetBuffer());

                ExpectException([&]()
                    {
                        reader.ReadString();
                    });
            });

        // 8. Invalid boolean
        RunTest("Invalid boolean value", []()
            {
                BinaryWriter writer;
                writer.WriteU8(5);

                BinaryReader reader(writer.GetBuffer());

                ExpectException([&]()
                    {
                        reader.ReadBool();
                    });
            });

        // 9. Write failure
        RunTest("Write failure", []()
            {
                AssetArchive archive;
                archive.AddAsset(100, 1, { std::byte{ 1 } });

                ExpectException([&]()
                    {
                        archive.Save(
                            "invalid_directory_that_does_not_exist/test.gca");
                    });
            });

        // 10. Invalid asset data
        RunTest("Truncated serialized data", []()
            {
                BinaryWriter writer;

                // PlayerProfile V2, but without its fields.
                writer.WriteU32(2);

                BinaryReader reader(writer.GetBuffer());

                ExpectException([&]()
                    {
                        Serializer<AssetData::PlayerProfile>::Read(reader);
                    });
            });

        std::cout << "\n====================================\n";
        std::cout << "RESULTS\n";
        std::cout << "Passed: " << passed << '\n';
        std::cout << "Failed: " << failed << '\n';
        std::cout << "====================================\n";

        if (failed != 0)
        {
            throw std::runtime_error(
                "Some robustness tests failed");
        }
    }
}
