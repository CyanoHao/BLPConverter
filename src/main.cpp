#include <memory.h>
#include <memory>
#include <string>

#include <FreeImage.h>
#include <SimpleOpt.h>
#include <fmt/core.h>
#include <nowide/args.hpp>
#include <nowide/cstdio.hpp>

#include "blp.h"

using blp::Header;
using blp::Pixel;
using std::string;

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

/**************************** COMMAND-LINE PARSING ****************************/

// The valid options
enum
{
    OPT_HELP,
    OPT_INFOS,
    OPT_DEST,
    OPT_FORMAT,
    OPT_MIP_LEVEL,
};


const CSimpleOpt::SOption COMMAND_LINE_OPTIONS[] = {
    { OPT_HELP,      "-h",         SO_NONE },
    { OPT_HELP,      "--help",     SO_NONE },
    { OPT_INFOS,     "-i",         SO_NONE },
    { OPT_INFOS,     "--infos",    SO_NONE },
    { OPT_DEST,      "-o",         SO_REQ_SEP },
    { OPT_DEST,      "--dest",     SO_REQ_SEP },
    { OPT_FORMAT,    "-f",         SO_REQ_SEP },
    { OPT_FORMAT,    "--format",   SO_REQ_SEP },
    { OPT_MIP_LEVEL, "-m",         SO_REQ_SEP },
    { OPT_MIP_LEVEL, "--miplevel", SO_REQ_SEP },

    SO_END_OF_OPTIONS
};


/********************************** FUNCTIONS *********************************/

void showUsage(const std::string &u8ApplicationName)
{
    fmt::print("BLPConverter\n"
               "\n"
               "Usage: {} [options] <blp_filename> [<blp_filename> ... <blp_filename>]\n"
               "\n"
               "Options:\n"
               "  --help, -h:      Display this help\n"
               "  --infos, -i:     Display informations about the BLP file(s) (no conversion)\n"
               "  --dest, -o:      Folder where the converted image(s) must be written to "
               "(default: './')\n"
               "  --format, -f:    'png' or 'tga' (default: png)\n"
               "  --miplevel, -m:  The specific mip level to convert (default: 0, the bigger one)\n"
               "\n",
               u8ApplicationName);
}

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

int main(int argc, char** argv)
{
    nowide::args _(argc, argv);

    bool         bInfos             = false;
    string       u8OutputFolder    = "./";
    string       strFormat          = "png";
    unsigned int mipLevel           = 0;
    unsigned int nbImagesTotal      = 0;
    unsigned int nbImagesConverted  = 0;


    // Parse the command-line parameters
    CSimpleOpt args(argc, argv, COMMAND_LINE_OPTIONS);
    while (args.Next())
    {
        if (args.LastError() == SO_SUCCESS)
        {
            switch (args.OptionId())
            {
                case OPT_HELP:
                    showUsage(argv[0]);
                    return 0;

                case OPT_INFOS:
                    bInfos = true;
                    break;

                case OPT_DEST:
                    u8OutputFolder = args.OptionArg();
                    if (u8OutputFolder.at(u8OutputFolder.size() - 1) != '/')
                        u8OutputFolder += "/";
                    break;

                case OPT_FORMAT:
                    strFormat = args.OptionArg();
                    if (strFormat != "tga")
                        strFormat = "png";
                    break;

                case OPT_MIP_LEVEL:
                    mipLevel = atoi(args.OptionArg());
                    break;
            }
        }
        else
        {
            fmt::println("Invalid argument: `{}`", args.OptionText());
            return -1;
        }
    }

    if (args.FileCount() == 0)
    {
        puts("No BLP file specified\n");
        return -1;
    }


    // Initialise FreeImage
    FreeImage_Initialise(true);


    // Process the files
    for (unsigned int i = 0; i < args.FileCount(); ++i)
    {
        ++nbImagesTotal;

        string u8InFileName = args.File(i);
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
                showInfos(args.File(i), header);
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
