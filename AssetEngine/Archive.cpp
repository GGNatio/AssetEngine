
#include "Archive.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace
{
    constexpr std::uint32_t ARCHIVE_VERSION = 1;
    constexpr std::size_t MAX_ARCHIVE_SIZE = 256 * 1024 * 1024;
    constexpr std::uint32_t MAX_ASSET_COUNT = 100000;

    constexpr std::size_t HEADER_SIZE = 12;
    constexpr std::size_t ENTRY_SIZE = 28;
    constexpr std::size_t CHECKSUM_SIZE = 4;

    // Ajoute un octet
    void WriteByte(std::vector<std::byte>& output, std::uint8_t value)
    {
        output.push_back(static_cast<std::byte>(value));
    }

    // Ecrit un entier en Little Endian
    void WriteU32(std::vector<std::byte>& output, std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            WriteByte(output, static_cast<std::uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }

    void WriteU64(std::vector<std::byte>& output, std::uint64_t value)
    {
        for (int i = 0; i < 8; ++i)
        {
            WriteByte(output, static_cast<std::uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }

    // Lit un entier en Little Endian
    std::uint32_t ReadU32(
        const std::vector<std::byte>& data,
        std::size_t& position)
    {
        if (position > data.size() || data.size() - position < 4)
            throw std::runtime_error("Unexpected end of archive");

        std::uint32_t value = 0;

        for (int i = 0; i < 4; ++i)
        {
            value |= static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>(data[position++])
                ) << (i * 8);
        }

        return value;
    }

    std::uint64_t ReadU64(
        const std::vector<std::byte>& data,
        std::size_t& position)
    {
        if (position > data.size() || data.size() - position < 8)
            throw std::runtime_error("Unexpected end of archive");

        std::uint64_t value = 0;

        for (int i = 0; i < 8; ++i)
        {
            value |= static_cast<std::uint64_t>(
                std::to_integer<std::uint8_t>(data[position++])
                ) << (i * 8);
        }

        return value;
    }

    // CRC-32 standard (polynome IEEE 0xEDB88320)
    std::uint32_t CalculateCRC32(
        const std::vector<std::byte>& data,
        std::size_t size)
    {
        std::uint32_t crc = 0xFFFFFFFF;

        for (std::size_t i = 0; i < size; ++i)
        {
            crc ^= std::to_integer<std::uint8_t>(data[i]);

            for (int bit = 0; bit < 8; ++bit)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320;
                else
                    crc >>= 1;
            }
        }

        return crc ^ 0xFFFFFFFF;
    }
}

// --------------------------------------------------
// Ajout d'un asset
// --------------------------------------------------

void AssetArchive::AddAsset(
    std::uint64_t id,
    std::uint32_t type,
    const std::vector<std::byte>& data)
{
    if (loadedFromFile)
        throw std::runtime_error("Cannot modify a loaded archive");

    if (id == 0)
        throw std::runtime_error("Asset ID cannot be zero");

    if (type == 0)
        throw std::runtime_error("Asset type cannot be zero");

    for (const auto& asset : assets)
    {
        if (asset.id == id)
            throw std::runtime_error("Duplicate asset ID");
    }

    if (assets.size() >= MAX_ASSET_COUNT)
        throw std::runtime_error("Too many assets");

    assets.push_back({ id, type, data });
}

// --------------------------------------------------
// Sauvegarde de l'archive
// --------------------------------------------------

void AssetArchive::Save(const std::string& path) const
{
    if (loadedFromFile)
        throw std::runtime_error("Cannot save a loaded archive");

    if (assets.size() > MAX_ASSET_COUNT)
        throw std::runtime_error("Too many assets");

    std::uint64_t directorySize =
        static_cast<std::uint64_t>(assets.size()) * ENTRY_SIZE;

    std::uint64_t dataOffset = HEADER_SIZE + directorySize;

    std::uint64_t totalSize = dataOffset + CHECKSUM_SIZE;

    for (const auto& asset : assets)
    {
        if (asset.data.size() > MAX_ARCHIVE_SIZE)
            throw std::runtime_error("Asset too large");

        if (asset.data.size() > MAX_ARCHIVE_SIZE - totalSize)
            throw std::runtime_error("Archive too large");

        totalSize += asset.data.size();
    }

    if (totalSize > MAX_ARCHIVE_SIZE)
        throw std::runtime_error("Archive too large");

    std::vector<std::byte> output;
    output.reserve(static_cast<std::size_t>(totalSize));

    // Header : magic number, version, nombre d'assets
    WriteByte(output, 'G');
    WriteByte(output, 'C');
    WriteByte(output, 'B');
    WriteByte(output, 'A');

    WriteU32(output, ARCHIVE_VERSION);
    WriteU32(output, static_cast<std::uint32_t>(assets.size()));

    // Repertoire des assets
    std::uint64_t currentOffset = dataOffset;

    for (const auto& asset : assets)
    {
        WriteU64(output, asset.id);
        WriteU32(output, asset.type);
        WriteU64(output, currentOffset);
        WriteU64(output, asset.data.size());

        currentOffset += asset.data.size();
    }

    // Donnees des assets
    for (const auto& asset : assets)
    {
        output.insert(
            output.end(),
            asset.data.begin(),
            asset.data.end()
        );
    }

    // Checksum de tout le contenu precedent
    std::uint32_t checksum = CalculateCRC32(output, output.size());
    WriteU32(output, checksum);

    // Ecriture du fichier
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    if (!file)
        throw std::runtime_error("Cannot open archive for writing");

    file.write(
        reinterpret_cast<const char*>(output.data()),
        static_cast<std::streamsize>(output.size())
    );

    if (!file)
        throw std::runtime_error("Failed to write archive");
}

// --------------------------------------------------
// Chargement d'une archive
// --------------------------------------------------

AssetArchive AssetArchive::Load(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file)
        throw std::runtime_error("Cannot open archive");

    std::streamsize fileSize = file.tellg();

    if (fileSize < static_cast<std::streamsize>(
        HEADER_SIZE + CHECKSUM_SIZE))
    {
        throw std::runtime_error("Archive is too small");
    }

    if (fileSize > static_cast<std::streamsize>(MAX_ARCHIVE_SIZE))
        throw std::runtime_error("Archive exceeds maximum size");

    file.seekg(0, std::ios::beg);

    std::vector<std::byte> data(static_cast<std::size_t>(fileSize));

    if (!file.read(
        reinterpret_cast<char*>(data.data()),
        fileSize))
    {
        throw std::runtime_error("Failed to read archive");
    }

    // Verification du checksum avant d'utiliser le repertoire
    std::size_t checksumPosition = data.size() - CHECKSUM_SIZE;
    std::size_t checksumReadPosition = checksumPosition;

    std::uint32_t storedChecksum =
        ReadU32(data, checksumReadPosition);

    std::uint32_t calculatedChecksum =
        CalculateCRC32(data, checksumPosition);

    if (storedChecksum != calculatedChecksum)
        throw std::runtime_error("Archive checksum mismatch");

    // Verification du magic number
    if (std::to_integer<char>(data[0]) != 'G' ||
        std::to_integer<char>(data[1]) != 'C' ||
        std::to_integer<char>(data[2]) != 'B' ||
        std::to_integer<char>(data[3]) != 'A')
    {
        throw std::runtime_error("Invalid archive magic number");
    }

    std::size_t position = 4;

    std::uint32_t version = ReadU32(data, position);

    if (version != ARCHIVE_VERSION)
        throw std::runtime_error("Unsupported archive version");

    std::uint32_t count = ReadU32(data, position);

    if (count > MAX_ASSET_COUNT)
        throw std::runtime_error("Too many archive entries");

    // Verification de la taille du repertoire
    if (count > (checksumPosition - HEADER_SIZE) / ENTRY_SIZE)
        throw std::runtime_error("Invalid archive directory size");

    std::size_t directoryEnd =
        HEADER_SIZE + static_cast<std::size_t>(count) * ENTRY_SIZE;

    AssetArchive archive;

    std::unordered_set<std::uint64_t> ids;

    // Lecture du repertoire
    for (std::uint32_t i = 0; i < count; ++i)
    {
        ArchiveEntry entry;

        entry.id = ReadU64(data, position);
        entry.type = ReadU32(data, position);
        entry.offset = ReadU64(data, position);
        entry.size = ReadU64(data, position);

        if (entry.id == 0 || entry.type == 0)
            throw std::runtime_error("Invalid asset metadata");

        if (!ids.insert(entry.id).second)
            throw std::runtime_error("Duplicate asset ID");

        // L'asset doit commencer apres le repertoire
        if (entry.offset < directoryEnd)
            throw std::runtime_error("Invalid asset offset");

        // Verification sans addition susceptible de deborder
        if (entry.offset > checksumPosition ||
            entry.size > checksumPosition - entry.offset)
        {
            throw std::runtime_error("Asset outside archive boundaries");
        }

        archive.entries.push_back(entry);
    }

    // Verification des chevauchements entre les assets
    std::vector<ArchiveEntry> sortedEntries = archive.entries;

    std::sort(
        sortedEntries.begin(),
        sortedEntries.end(),
        [](const ArchiveEntry& a, const ArchiveEntry& b)
        {
            return a.offset < b.offset;
        }
    );

    for (std::size_t i = 1; i < sortedEntries.size(); ++i)
    {
        const auto& previous = sortedEntries[i - 1];
        const auto& current = sortedEntries[i];

        if (current.offset < previous.offset + previous.size)
            throw std::runtime_error("Overlapping asset data");
    }

    archive.fileData = std::move(data);
    archive.loadedFromFile = true;

    return archive;
}

// --------------------------------------------------
// Recuperation d'un asset
// --------------------------------------------------

std::vector<std::byte> AssetArchive::GetAsset(
    std::uint64_t id,
    std::uint32_t expectedType) const
{
    if (loadedFromFile)
    {
        for (const auto& entry : entries)
        {
            if (entry.id == id)
            {
                if (entry.type != expectedType)
                    throw std::runtime_error("Asset type mismatch");

                auto begin = fileData.begin() +
                    static_cast<std::ptrdiff_t>(entry.offset);

                auto end = begin +
                    static_cast<std::ptrdiff_t>(entry.size);

                return std::vector<std::byte>(begin, end);
            }
        }

        throw std::runtime_error("Asset ID not found");
    }

    for (const auto& asset : assets)
    {
        if (asset.id == id)
        {
            if (asset.type != expectedType)
                throw std::runtime_error("Asset type mismatch");

            return asset.data;
        }
    }

    throw std::runtime_error("Asset ID not found");
}

// --------------------------------------------------
// Liste des assets
// --------------------------------------------------

const std::vector<ArchiveEntry>& AssetArchive::GetEntries() const
{
    return entries;
}
