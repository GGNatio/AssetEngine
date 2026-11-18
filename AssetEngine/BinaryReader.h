
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

class BinaryReader
{
public:

    explicit BinaryReader(const std::vector<std::byte>& data)
        : buffer(data)
    {
    }

    // Lit un entier sur 1 octet
    std::uint8_t ReadU8()
    {
        CheckSize(1);

        return std::to_integer<std::uint8_t>(buffer[position++]);
    }

    // Lit un entier sur 4 octets (Little Endian)
    std::uint32_t ReadU32()
    {
        CheckSize(4);

        std::uint32_t value = 0;

        for (int i = 0; i < 4; ++i)
        {
            value |= static_cast<std::uint32_t>(ReadU8()) << (i * 8);
        }

        return value;
    }

    // Lit un entier sur 8 octets (Little Endian)
    std::uint64_t ReadU64()
    {
        CheckSize(8);

        std::uint64_t value = 0;

        for (int i = 0; i < 8; ++i)
        {
            value |= static_cast<std::uint64_t>(ReadU8()) << (i * 8);
        }

        return value;
    }

    // Lit un entier signe sur 4 octets
    std::int32_t ReadI32()
    {
        return std::bit_cast<std::int32_t>(ReadU32());
    }

    // Lit un booleen
    bool ReadBool()
    {
        std::uint8_t value = ReadU8();

        if (value > 1)
            throw std::runtime_error("Invalid boolean value");

        return value == 1;
    }

    // Lit un float
    float ReadFloat()
    {
        return std::bit_cast<float>(ReadU32());
    }

    // Lit un double
    double ReadDouble()
    {
        return std::bit_cast<double>(ReadU64());
    }

    // Lit une chaine
    std::string ReadString()
    {
        std::uint32_t size = ReadU32();

        CheckSize(size);

        std::string value;
        value.reserve(size);

        for (std::uint32_t i = 0; i < size; ++i)
        {
            value.push_back(static_cast<char>(ReadU8()));
        }

        return value;
    }

    // Lit un tableau d'entiers
    std::vector<std::int32_t> ReadInt32Vector()
    {
        std::uint32_t count = ReadU32();

        // Chaque entier occupe 4 octets.
        if (count > (buffer.size() - position) / 4)
            throw std::runtime_error("Invalid vector size");

        std::vector<std::int32_t> values;
        values.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i)
        {
            values.push_back(ReadI32());
        }

        return values;
    }

    // Nombre d'octets restants
    std::size_t GetRemaining() const
    {
        return buffer.size() - position;
    }

private:

    const std::vector<std::byte>& buffer;
    std::size_t position = 0;

    // Verifie qu'il reste assez de donnees
    void CheckSize(std::size_t size) const
    {
        if (position > buffer.size() ||
            size > buffer.size() - position)
        {
            throw std::runtime_error("Unexpected end of binary data");
        }
    }
};
