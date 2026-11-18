
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

struct ArchiveEntry
{
    std::uint64_t id;
    std::uint32_t type;
    std::uint64_t offset;
    std::uint64_t size;
};

class AssetArchive
{
public:

    // Ajoute un asset a l'archive
    void AddAsset(
        std::uint64_t id,
        std::uint32_t type,
        const std::vector<std::byte>& data
    );

    // Enregistre l'archive dans un fichier
    void Save(const std::string& path) const;

    // Charge une archive existante
    static AssetArchive Load(const std::string& path);

    // Recupere un asset a partir de son ID
    std::vector<std::byte> GetAsset(
        std::uint64_t id,
        std::uint32_t expectedType
    ) const;

    // Retourne les entrees du repertoire
    const std::vector<ArchiveEntry>& GetEntries() const;

private:

    struct StoredAsset
    {
        std::uint64_t id;
        std::uint32_t type;
        std::vector<std::byte> data;
    };

    std::vector<StoredAsset> assets;
    std::vector<ArchiveEntry> entries;

    // Donnees originales d'une archive chargee
    std::vector<std::byte> fileData;

    bool loadedFromFile = false;
};
