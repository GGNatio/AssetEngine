
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

class BinaryWriter
{
public:

    // Ecrit un entier sur 1 octet
    void WriteU8(std::uint8_t value)
    {
        buffer.push_back(static_cast<std::byte>(value));
    }

    // Ecrit un entier sur 4 octets (Little Endian)
    void WriteU32(std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            WriteU8(static_cast<std::uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }

    // Ecrit un entier sur 8 octets (Little Endian)
    void WriteU64(std::uint64_t value)
    {
        for (int i = 0; i < 8; ++i)
        {
            WriteU8(static_cast<std::uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }

    // Ecrit un entier signe sur 4 octets
    void WriteI32(std::int32_t value)
    {
        WriteU32(std::bit_cast<std::uint32_t>(value));
    }

    // Ecrit un booleen
    void WriteBool(bool value)
    {
        WriteU8(value ? 1 : 0);
    }

    // Ecrit un float
    void WriteFloat(float value)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        WriteU32(std::bit_cast<std::uint32_t>(value));
    }

    // Ecrit un double
    void WriteDouble(double value)
    {
        static_assert(sizeof(double) == sizeof(std::uint64_t));
        WriteU64(std::bit_cast<std::uint64_t>(value));
    }

    // Ecrit une chaine avec sa longueur
    void WriteString(std::string_view value)
    {
        if (value.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("String too large");

        WriteU32(static_cast<std::uint32_t>(value.size()));

        for (unsigned char c : value)
        {
            WriteU8(c);
        }
    }

    // Ecrit un tableau d'entiers
    void WriteInt32Vector(const std::vector<std::int32_t>& values)
    {
        if (values.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Vector too large");

        WriteU32(static_cast<std::uint32_t>(values.size()));

        for (std::int32_t value : values)
        {
            WriteI32(value);
        }
    }

    // Recupere les donnees binaires
    const std::vector<std::byte>& GetBuffer() const
    {
        return buffer;
    }

    // Recupere la taille des donnees
    std::size_t GetSize() const
    {
        return buffer.size();
    }

private:

    std::vector<std::byte> buffer;
};
