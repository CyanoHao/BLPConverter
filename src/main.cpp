#include <cstdint>
#include <memory.h>
#include <memory>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <FreeImage.h>
#include <fmt/core.h>
#include <nowide/args.hpp>
#include <nowide/cstdio.hpp>

#include "blp.h"

using blp::Header;
using blp::Pixel;
using std::string;
using std::vector;

struct FILE_ptr : public std::unique_ptr<FILE, int (*)(FILE *)>
{
    FILE_ptr(const char *u8Filename, const char *mode)
        : std::unique_ptr<FILE, int (*)(FILE *)>(nowide::fopen(u8Filename, mode), &fclose)
    {
    }

    operator FILE *() const
    {
        return get();
    }
};

struct FIBITMAP_ptr : public std::unique_ptr<FIBITMAP, void (*)(FIBITMAP *)>
{
    FIBITMAP_ptr(int width,
                 int height,
                 int bpp,
                 unsigned red_mask = 0,
                 unsigned green_mask = 0,
                 unsigned blue_mask = 0)
        : std::unique_ptr<FIBITMAP, void (*)(FIBITMAP *)>(
              FreeImage_Allocate(width, height, bpp, red_mask, green_mask, blue_mask),
              &FreeImage_Unload)
    {
    }

    operator FIBITMAP *() const
    {
        return get();
    }
};

void showInfos(const std::string &u8Filename, const Header &header)
{
    fmt::print("Infos about `{}`:\n"
               "  - Version:    BLP2\n"
               "  - Format:     {}\n"
               "  - Dimensions: {}x{}\n"
               "  - Mip levels: {}\n",
               u8Filename,
               header.friendlyFormat(),
               header.width(),
               header.height(),
               header.mipLevels());
}

int main(int argc, char **argv)
{
    nowide::args _(argc, argv);

    CLI::App app{"Convert BLP image files to PNG or TGA format", "BLPConverter"};

    bool bInfos = false;
    string u8OutputFolder = "./";
    string strFormat = "png";
    uint32_t mipLevel = 0;
    vector<string> filenames;

    app.add_flag(
        "-i,--infos", bInfos, "Display informations about the BLP file(s) (no conversion)");
    app.add_option(
           "-o,--dest", u8OutputFolder, "Folder where the converted image(s) must be written to")
        ->capture_default_str();
    app.add_option("-f,--format", strFormat, "`png` or `tga`")->capture_default_str();
    app.add_option("-m,--miplevel", mipLevel, "The specific mip level to convert")
        ->capture_default_str();
    app.add_option("files", filenames)->expected(1, -1)->required();

    CLI11_PARSE(app, argc, argv);

    if (!u8OutputFolder.empty() && u8OutputFolder.back() != '/')
        u8OutputFolder += '/';

    unsigned int nbImagesConverted = 0;

    // Initialise FreeImage
    FreeImage_Initialise(true);

    // Process the files
    for (auto &u8InFileName : filenames)
    {
        string u8OutFileName = u8InFileName.substr(0, u8InFileName.size() - 3) + strFormat;

        size_t offset = u8OutFileName.find_last_of("/\\");
        if (offset != string::npos)
            u8OutFileName = u8OutFileName.substr(offset + 1);

        FILE_ptr pFile(u8InFileName.c_str(), "rb");
        if (!pFile)
        {
            fmt::println(stderr, "Failed to open the file `{}`", u8InFileName);
            continue;
        }
        string data;
        {
            fseek(pFile, 0, SEEK_END);
            size_t size = ftell(pFile);
            data.resize(size);
            fseek(pFile, 0, SEEK_SET);
            fread(data.data(), 1, size, pFile);
        }

        try
        {
            Header header = Header::fromBinary(data);

            uint32_t width = header.width(mipLevel);
            uint32_t height = header.height(mipLevel);

            if (bInfos)
            {
                showInfos(u8OutFileName, header);
            }
            else
            {
                auto mipmap = header.getMipmap(data);

                FIBITMAP_ptr pImage(width, height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000);

                for (uint32_t y = 0; y < height; ++y)
                {
                    Pixel *data = mipmap.data() + width * (height - y - 1);
                    BYTE *pLine = FreeImage_GetScanLine(pImage, y);
                    memcpy(pLine, data, width * sizeof(Pixel));
                }

                if (FreeImage_Save((strFormat == "tga" ? FIF_TARGA : FIF_PNG),
                                   pImage,
                                   (u8OutputFolder + u8OutFileName).c_str(),
                                   0))
                {
                    fmt::println(stderr, "{}: OK", u8InFileName);
                    ++nbImagesConverted;
                }
                else
                {
                    fmt::println(stderr, "{}: Failed to save the image", u8InFileName);
                }
            }
        }
        catch (const blp::BLPError &e)
        {
            fmt::println(stderr, "Failed to parse file `{}`: {}", u8InFileName, e.what());
            continue;
        }
    }

    // Cleanup
    FreeImage_DeInitialise();

    return 0;
}
