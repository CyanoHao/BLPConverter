#pragma once

#include <stdexcept>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace blp
{

template <typename T>
constexpr bool is_pod_v = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

enum tBLPEncoding
{
    BLP_ENCODING_UNCOMPRESSED = 1,
    BLP_ENCODING_DXT = 2,
    BLP_ENCODING_UNCOMPRESSED_RAW_BGRA = 3,
};

enum tBLPAlphaDepth
{
    BLP_ALPHA_DEPTH_0 = 0,
    BLP_ALPHA_DEPTH_1 = 1,
    BLP_ALPHA_DEPTH_4 = 4,
    BLP_ALPHA_DEPTH_8 = 8,
};

enum tBLPAlphaEncoding
{
    BLP_ALPHA_ENCODING_DXT1 = 0,
    BLP_ALPHA_ENCODING_DXT3 = 1,
    BLP_ALPHA_ENCODING_DXT5 = 7,
};

enum tBLPFormat
{
    BLP_FORMAT_JPEG = 0,

    BLP_FORMAT_PALETTED_NO_ALPHA = (BLP_ENCODING_UNCOMPRESSED << 16) | (BLP_ALPHA_DEPTH_0 << 8),
    BLP_FORMAT_PALETTED_ALPHA_1 = (BLP_ENCODING_UNCOMPRESSED << 16) | (BLP_ALPHA_DEPTH_1 << 8),
    BLP_FORMAT_PALETTED_ALPHA_4 = (BLP_ENCODING_UNCOMPRESSED << 16) | (BLP_ALPHA_DEPTH_4 << 8),
    BLP_FORMAT_PALETTED_ALPHA_8 = (BLP_ENCODING_UNCOMPRESSED << 16) | (BLP_ALPHA_DEPTH_8 << 8),

    BLP_FORMAT_RAW_BGRA = (BLP_ENCODING_UNCOMPRESSED_RAW_BGRA << 16),

    BLP_FORMAT_DXT1_NO_ALPHA =
        (BLP_ENCODING_DXT << 16) | (BLP_ALPHA_DEPTH_0 << 8) | BLP_ALPHA_ENCODING_DXT1,
    BLP_FORMAT_DXT1_ALPHA_1 =
        (BLP_ENCODING_DXT << 16) | (BLP_ALPHA_DEPTH_1 << 8) | BLP_ALPHA_ENCODING_DXT1,
    BLP_FORMAT_DXT3_ALPHA_4 =
        (BLP_ENCODING_DXT << 16) | (BLP_ALPHA_DEPTH_4 << 8) | BLP_ALPHA_ENCODING_DXT3,
    BLP_FORMAT_DXT3_ALPHA_8 =
        (BLP_ENCODING_DXT << 16) | (BLP_ALPHA_DEPTH_8 << 8) | BLP_ALPHA_ENCODING_DXT3,
    BLP_FORMAT_DXT5_ALPHA_8 =
        (BLP_ENCODING_DXT << 16) | (BLP_ALPHA_DEPTH_8 << 8) | BLP_ALPHA_ENCODING_DXT5,
};

class BLPError : public std::runtime_error
{
  public:
    template <typename T>
    BLPError(T &&message)
        : std::runtime_error(std::forward<T>(message))
    {
    }
};

struct Pixel
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
};

// A description of the BLP2 format can be found on Wikipedia: http://en.wikipedia.org/wiki/.BLP
struct Header
{
    uint8_t magic[4];      // Always 'BLP2'
    uint32_t type;         // 0: JPEG, 1: see encoding
    uint8_t encoding;      // 1: Uncompressed, 2: DXT compression, 3: Uncompressed BGRA
    uint8_t alphaDepth;    // 0, 1, 4 or 8 bits
    uint8_t alphaEncoding; // 0: DXT1, 1: DXT3, 7: DXT5

    union
    {
        uint8_t hasMipLevels; // In BLP file: 0 or 1
        uint8_t nbMipLevels;  // For convenience, replaced with the number of mip levels
    };

    uint32_t width_; // In pixels, power-of-two
    uint32_t height_;
    uint32_t offsets[16];
    uint32_t lengths[16];
    Pixel palette[256]; // 256 BGRA colors

  public:
    tBLPFormat format() const;

    uint32_t width(uint32_t mipLevel = 0) const;
    uint32_t height(uint32_t mipLevel = 0) const;
    uint32_t mipLevels() const;

    std::string friendlyFormat() const;

    std::vector<Pixel> getMipmap(std::string_view data, uint32_t mipLevel = 0) const;

  public:
    static Header fromBinary(std::string_view data);
    static std::string friendlyFormat(tBLPFormat format);

  private:
    static std::vector<Pixel> convertPalettedNoAlpha(std::string_view mipmap,
                                                     const Header &header,
                                                     unsigned int width,
                                                     unsigned int height);
    static std::vector<Pixel> convertPalettedAlpha1(std::string_view mipmap,
                                                    const Header &header,
                                                    unsigned int width,
                                                    unsigned int height);
    static std::vector<Pixel> convertPalettedAlpha4(std::string_view mipmap,
                                                    const Header &header,
                                                    unsigned int width,
                                                    unsigned int height);
    static std::vector<Pixel> convertPalettedAlpha8(std::string_view mipmap,
                                                    const Header &header,
                                                    unsigned int width,
                                                    unsigned int height);
    static std::vector<Pixel> convertRawBgra(std::string_view mipmap,
                                             const Header &header,
                                             unsigned int width,
                                             unsigned int height);
    static std::vector<Pixel> convertDxt(std::string_view mipmap,
                                         const Header &header,
                                         unsigned int width,
                                         unsigned int height,
                                         int flags);
};

static_assert(is_pod_v<Header>);

} // namespace blp
