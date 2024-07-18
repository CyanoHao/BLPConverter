#include "blp.h"

#include <memory.h>
#include <string.h>
#include <vector>

#include <FreeImage.h>
#include <fmt/core.h>
#include <squish.h>

using std::string;
using std::string_view;
using std::vector;
using namespace std::literals;

namespace blp
{

tBLPFormat Header::format() const
{
    if (type == 0)
        return BLP_FORMAT_JPEG;

    if (encoding == BLP_ENCODING_UNCOMPRESSED)
        return tBLPFormat((encoding << 16) | (alphaDepth << 8));

    if (encoding == BLP_ENCODING_UNCOMPRESSED_RAW_BGRA)
        return tBLPFormat((encoding << 16));

    return tBLPFormat((encoding << 16) | (alphaDepth << 8) | alphaEncoding);
}

uint32_t Header::width(uint32_t mipLevel) const
{
    if (mipLevel >= nbMipLevels)
        mipLevel = nbMipLevels - 1;

    return (width_ >> mipLevel);
}

uint32_t Header::height(uint32_t mipLevel) const
{
    if (mipLevel >= nbMipLevels)
        mipLevel = nbMipLevels - 1;

    return (height_ >> mipLevel);
}

uint32_t Header::mipLevels() const
{
    return nbMipLevels;
}

string Header::friendlyFormat() const
{
    return friendlyFormat(format());
}

std::vector<Pixel> Header::getMipmap(string_view data, uint32_t mipLevel) const
{
    if (mipLevel >= nbMipLevels)
        mipLevel = nbMipLevels - 1;

    unsigned mipWidth = width(mipLevel);
    unsigned mipHeight = height(mipLevel);

    auto offset = offsets[mipLevel];
    auto size = lengths[mipLevel];

    if (data.size() < offset + size)
        throw BLPError("Invalid BLP2 file: mipmap data is truncated");

    string_view mipmap = data.substr(offset, size);

    switch (format())
    {
    case BLP_FORMAT_PALETTED_NO_ALPHA:
        return convertPalettedNoAlpha(mipmap, *this, mipWidth, mipHeight);
    case BLP_FORMAT_PALETTED_ALPHA_1:
        return convertPalettedAlpha1(mipmap, *this, mipWidth, mipHeight);
    case BLP_FORMAT_PALETTED_ALPHA_4:
        return convertPalettedAlpha4(mipmap, *this, mipWidth, mipHeight);
    case BLP_FORMAT_PALETTED_ALPHA_8:
        return convertPalettedAlpha8(mipmap, *this, mipWidth, mipHeight);

    case BLP_FORMAT_RAW_BGRA:
        return convertRawBgra(mipmap, *this, mipWidth, mipHeight);

    case BLP_FORMAT_DXT1_NO_ALPHA:
    case BLP_FORMAT_DXT1_ALPHA_1:
        return convertDxt(mipmap, *this, mipWidth, mipHeight, squish::kDxt1);
    case BLP_FORMAT_DXT3_ALPHA_4:
    case BLP_FORMAT_DXT3_ALPHA_8:
        return convertDxt(mipmap, *this, mipWidth, mipHeight, squish::kDxt3);
    case BLP_FORMAT_DXT5_ALPHA_8:
        return convertDxt(mipmap, *this, mipWidth, mipHeight, squish::kDxt5);

    default:
        throw BLPError("Unsupported BLP2 format: " + friendlyFormat());
    }
}

Header Header::fromBinary(std::string_view data)
{
    if (data.size() < 4)
        throw BLPError("Invalid BLP file: too short to contain magic");

    string_view magic = data.substr(0, 4);
    if (magic == "BLP1"sv)
        throw BLPError("Invalid BLP file: unsupported format BLP1");
    else if (magic != "BLP2"sv)
        throw BLPError("Invalid BLP file: unknown magic");

    if (data.size() < sizeof(Header))
        throw BLPError("Invalid BLP2 file: too short to contain header");

    Header header;
    memcpy(&header, data.data(), sizeof(Header));

    uint8_t mipLevels = 0;
    while (header.offsets[mipLevels] != 0 && mipLevels < 16)
        ++mipLevels;
    header.nbMipLevels = mipLevels;

    return header;
}

std::string Header::friendlyFormat(tBLPFormat format)
{
    switch (format)
    {
    case BLP_FORMAT_JPEG:
        return "JPEG";
    case BLP_FORMAT_PALETTED_NO_ALPHA:
        return "Uncompressed paletted image, no alpha";
    case BLP_FORMAT_PALETTED_ALPHA_1:
        return "Uncompressed paletted image, 1-bit alpha";
    case BLP_FORMAT_PALETTED_ALPHA_4:
        return "Uncompressed paletted image, 4-bit alpha";
    case BLP_FORMAT_PALETTED_ALPHA_8:
        return "Uncompressed paletted image, 8-bit alpha";
    case BLP_FORMAT_RAW_BGRA:
        return "Uncompressed raw 32-bit BGRA";
    case BLP_FORMAT_DXT1_NO_ALPHA:
        return "DXT1, no alpha";
    case BLP_FORMAT_DXT1_ALPHA_1:
        return "DXT1, 1-bit alpha";
    case BLP_FORMAT_DXT3_ALPHA_4:
        return "DXT3, 4-bit alpha";
    case BLP_FORMAT_DXT3_ALPHA_8:
        return "DXT3, 8-bit alpha";
    case BLP_FORMAT_DXT5_ALPHA_8:
        return "DXT5, 8-bit alpha";
    default:
        return "Unknown";
    }
}

vector<Pixel> Header::convertPalettedNoAlpha(string_view mipmap,
                                             const Header &header,
                                             unsigned int width,
                                             unsigned int height)
{
    auto expectedLength = width * height;
    if (mipmap.size() < expectedLength)
        throw BLPError(
            fmt::format("Invalid BLP2 paletted mipmap: too short ({0} expected, {1} provided)",
                        expectedLength,
                        mipmap.size()));

    vector<Pixel> result(width * height);
    for (uint32_t idx = 0; idx < width * height; ++idx)
    {
        result[idx] = header.palette[mipmap[idx]];
        result[idx].a = 0xFF;
    }
    return result;
}

vector<Pixel> Header::convertPalettedAlpha1(string_view mipmap,
                                            const Header &header,
                                            unsigned int width,
                                            unsigned int height)
{
    auto expectedLength = width * height + (width * height + 7) / 8;
    if (mipmap.size() < expectedLength)
        throw BLPError(
            fmt::format("Invalid BLP2 paletted mipmap: too short ({0} expected, {1} provided)",
                        expectedLength,
                        mipmap.size()));

    vector<Pixel> result(width * height);
    for (uint32_t idx = 0; idx < width * height; ++idx)
    {
        auto alphaIdx = width * height + idx / 8;
        auto alphaBit = idx % 8;
        result[idx] = header.palette[mipmap[idx]];
        result[idx].a = (mipmap[alphaIdx] & (1 << alphaBit)) ? 0xFF : 0x00;
    }
    return result;
}

vector<Pixel> Header::convertPalettedAlpha4(string_view mipmap,
                                            const Header &header,
                                            unsigned int width,
                                            unsigned int height)
{
    auto expectedLength = width * height + (width * height + 1) / 2;
    if (mipmap.size() < expectedLength)
        throw BLPError(
            fmt::format("Invalid BLP2 paletted mipmap: too short ({0} expected, {1} provided)",
                        expectedLength,
                        mipmap.size()));

    vector<Pixel> result(width * height);
    for (uint32_t idx = 0; idx < width * height; ++idx)
    {
        auto alphaIdx = width * height + idx / 2;
        auto alphaBit = idx % 2 * 4;
        result[idx] = header.palette[mipmap[idx]];
        uint8_t alpha = (mipmap[alphaIdx] >> alphaBit) & 0xF;
        result[idx].a = (alpha << 4) | alpha;
    }
    return result;
}

vector<Pixel> Header::convertPalettedAlpha8(string_view mipmap,
                                            const Header &header,
                                            unsigned int width,
                                            unsigned int height)
{
    auto expectedLength = width * height * 2;
    if (mipmap.size() < expectedLength)
        throw BLPError(
            fmt::format("Invalid BLP2 paletted mipmap: too short ({0} expected, {1} provided)",
                        expectedLength,
                        mipmap.size()));

    vector<Pixel> result(width * height);
    for (uint32_t idx = 0; idx < width * height; ++idx)
    {
        auto alphaIdx = width * height + idx;
        result[idx] = header.palette[mipmap[idx]];
        result[idx].a = mipmap[alphaIdx];
    }
    return result;
}

vector<Pixel> Header::convertRawBgra(std::string_view mipmap,
                                     const Header &header,
                                     unsigned int width,
                                     unsigned int height)
{
    auto expectedLength = width * height * 4;
    if (mipmap.size() < expectedLength)
        throw BLPError(
            fmt::format("Invalid BLP2 raw mipmap: too short ({0} expected, {1} provided)",
                        expectedLength,
                        mipmap.size()));

    vector<Pixel> result(width * height);
    memcpy(result.data(), mipmap.data(), width * height * 4);
    return result;
}

vector<Pixel> Header::convertDxt(
    string_view mipmap, const Header &header, unsigned int width, unsigned int height, int flags)
{
    vector<Pixel> result(width * height);
    squish::DecompressImage(reinterpret_cast<squish::u8 *>(result.data()),
                            width,
                            height,
                            reinterpret_cast<const squish::u8 *>(mipmap.data()),
                            flags);

    for (uint32_t idx = 0; idx < width * height; ++idx)
        std::swap(result[idx].b, result[idx].r);
    return result;
}

} // namespace blp
