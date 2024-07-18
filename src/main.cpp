#include <iostream>
#include <memory.h>
#include <memory>
#include <stdio.h>
#include <string>

#include <FreeImage.h>
#include <SimpleOpt.h>
#include <fmt/core.h>

#include "blp.h"

using namespace std;
using blp::Header;
using blp::Pixel;

struct FILE_ptr : public std::unique_ptr<FILE, int (*)(FILE *)>
{
    FILE_ptr(const char *filename, const char *mode)
        : std::unique_ptr<FILE, int (*)(FILE *)>(fopen(filename, mode), &fclose)
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

void showUsage(const std::string& strApplicationName)
{
    cout << "BLPConverter" << endl
         << endl
         << "Usage: " << strApplicationName << " [options] <blp_filename> [<blp_filename> ... <blp_filename>]" << endl
         << endl
         << "Options:" << endl
         << "  --help, -h:      Display this help" << endl
         << "  --infos, -i:     Display informations about the BLP file(s) (no conversion)" << endl
         << "  --dest, -o:      Folder where the converted image(s) must be written to (default: './')" << endl
         << "  --format, -f:    'png' or 'tga' (default: png)" << endl
         << "  --miplevel, -m:  The specific mip level to convert (default: 0, the bigger one)" << endl
         << endl;
}

void showInfos(const std::string &strFilename, const Header &header)
{
    fmt::print("Infos about `{}`:\n"
               "  - Version:    BLP2\n"
               "  - Format:     {}\n"
               "  - Dimensions: {}x{}\n"
               "  - Mip levels: {}\n",
               strFilename,
               header.friendlyFormat(),
               header.width(),
               header.height(),
               header.mipLevels());
}

int main(int argc, char** argv)
{
    bool         bInfos             = false;
    string       strOutputFolder    = "./";
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
                    strOutputFolder = args.OptionArg();
                    if (strOutputFolder.at(strOutputFolder.size() - 1) != '/')
                        strOutputFolder += "/";
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
            cerr << "Invalid argument: " << args.OptionText() << endl;
            return -1;
        }
    }

    if (args.FileCount() == 0)
    {
        cerr << "No BLP file specified" << endl;
        return -1;
    }


    // Initialise FreeImage
    FreeImage_Initialise(true);


    // Process the files
    for (unsigned int i = 0; i < args.FileCount(); ++i)
    {
        ++nbImagesTotal;

        string strInFileName = args.File(i);
        string strOutFileName = strInFileName.substr(0, strInFileName.size() - 3) + strFormat;

        size_t offset = strOutFileName.find_last_of("/\\");
        if (offset != string::npos)
            strOutFileName = strOutFileName.substr(offset + 1);

        FILE_ptr pFile(strInFileName.c_str(), "rb");
        if (!pFile)
        {
            cerr << "Failed to open the file '" << strInFileName << "'" << endl;
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
                                   (strOutputFolder + strOutFileName).c_str(),
                                   0))
                {
                    cerr << strInFileName << ": OK" << endl;
                    ++nbImagesConverted;
                }
                else
                {
                    cerr << strInFileName << ": Failed to save the image" << endl;
                }
            }
        }
        catch (const blp::BLPError &e)
        {
            fmt::println("Failed to parse file `{}`: {}", strInFileName, e.what());
            continue;
        }
    }

    // Cleanup
    FreeImage_DeInitialise();

    return 0;
}
