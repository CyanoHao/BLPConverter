#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <BS_thread_pool.hpp>
#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <nowide/args.hpp>
#include <nowide/cstdio.hpp>

#include "blp.h"

#include "FIfix.h"

using blp::Header;
using blp::Pixel;
using std::atomic;
using std::string;
using std::vector;
using std::filesystem::path;
using std::filesystem::u8path;

namespace fs = std::filesystem;

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

void showInfos(const path &inPath, const Header &header)
{
    fmt::print("Infos about `{}`:\n"
               "  - Version:    BLP2\n"
               "  - Format:     {}\n"
               "  - Dimensions: {}x{}\n"
               "  - Mip levels: {}\n",
               inPath.u8string(),
               header.friendlyFormat(),
               header.width(),
               header.height(),
               header.mipLevels());
}

namespace options
{
bool bInfos = false;
string strFormat = "png";
uint32_t mipLevel = 0;
uint32_t jobs = std::thread::hardware_concurrency();
} // namespace options

atomic<uint32_t> nbImagesConverted = 0;

void convert(const path &inPath, const path &outPath)
{
    using namespace options;

    FILE_ptr pFile(inPath.u8string().c_str(), "rb");
    if (!pFile)
    {
        fmt::println(stderr, "Failed to open the file `{}`", inPath.u8string());
        return;
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
            showInfos(inPath.filename(), header);
        }
        else
        {
            auto mipmap = header.getMipmap(data);

            FIBITMAP_ptr pImage(width, height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000);

            for (uint32_t y = 0; y < height; ++y)
            {
                Pixel *data = mipmap.data() + width * (height - y - 1);
                uint8_t *pLine = freeimage::GetScanLine(pImage, y);
                memcpy(pLine, data, width * sizeof(Pixel));
            }

            if (freeimage::Save(
                    (strFormat == "tga" ? freeimage::Format::TARGA : freeimage::Format::PNG),
                    pImage,
                    outPath.c_str(),
                    0))
            {
                fmt::println(stderr, "{}: OK", inPath.u8string());
                ++nbImagesConverted;
            }
            else
            {
                fmt::println(stderr, "{}: Failed to save the image", inPath.u8string());
            }
        }
    }
    catch (const blp::BLPError &e)
    {
        fmt::println(stderr, "{}: {}", inPath.u8string(), e.what());
    }
}

int main(int argc, char **argv)
{
    using namespace options;

    nowide::args _(argc, argv);

    CLI::App app{"Convert BLP image files to PNG or TGA format", "BLPConverter"};

    string u8OutputDirName = "./";
    vector<string> filenames;

    app.add_flag(
        "-i,--infos", bInfos, "Display informations about the BLP file(s) (no conversion)");
    app.add_option(
           "-o,--dest", u8OutputDirName, "Folder where the converted image(s) must be written to")
        ->capture_default_str();
    app.add_option("-f,--format", strFormat, "`png` or `tga`")->capture_default_str();
    app.add_option("-m,--miplevel", mipLevel, "The specific mip level to convert")
        ->capture_default_str();
    app.add_option("-j,--jobs", jobs, "Number of parallel jobs")->capture_default_str();
    app.add_option("files", filenames)->expected(1, -1)->required();

    CLI11_PARSE(app, argc, argv);

    path outputPath = u8path(u8OutputDirName);

    if (jobs == 0)
        jobs = std::thread::hardware_concurrency();

    freeimage::Initialise(true);

    BS::thread_pool pool(jobs);

    uint32_t nbExpected = 0;

    for (const auto &filename : filenames)
    {
        try
        {
            path filePath = u8path(filename);
            fs::directory_entry fileEntry(u8path(filename));

            if (!fileEntry.exists())
            {
                fmt::println(stderr, "{}: Not found", filename);
                continue;
            }

            if (fileEntry.is_directory())
            {
                path groupInDirPath = fileEntry.path();

                // remove the trailing slash
                if (!groupInDirPath.has_filename())
                    groupInDirPath = groupInDirPath.parent_path();

                path groupOutDirPath = outputPath / groupInDirPath.filename();
                fs::create_directories(groupOutDirPath);

                fs::recursive_directory_iterator it(fileEntry), end;
                for (; it != end; ++it)
                {
                    if (!it->is_regular_file())
                        continue;

                    string extension = it->path().extension().u8string();
                    if (extension == ".blp" || extension == ".BLP")
                    {
                        path fullInPath = it->path();
                        path itemInPath = fs::relative(it->path(), fileEntry.path());
                        path itemOutPath = path{itemInPath}.replace_extension(strFormat);
                        path fullOutPath = groupOutDirPath / itemOutPath;
                        fs::create_directories(fullOutPath.parent_path());

                        nbExpected++;
                        pool.detach_task([fullInPath, fullOutPath]
                                         { convert(fullInPath, fullOutPath); });
                    }
                }
            }
            else if (fileEntry.is_regular_file())
            {
                fs::create_directories(outputPath);

                path filePath = u8path(filename);
                path itemOutPath = filePath.filename().replace_extension(strFormat);
                path fullOutPath = outputPath / itemOutPath;

                nbExpected++;
                pool.detach_task([filePath, fullOutPath] { convert(filePath, fullOutPath); });
            }
            else
            {
                nbExpected++;
                fmt::println(
                    stderr, "{}: Not a directory or a regular file", fileEntry.path().u8string());
                continue;
            }
        }
        catch (const fs::filesystem_error &e)
        {
            fmt::println(stderr, "{}: {}", filename, e.what());
            continue;
        }
    }

    pool.wait();

    freeimage::DeInitialise();

    if (nbImagesConverted < nbExpected)
    {
        fmt::println(stderr, "Failed to convert {} image(s)", nbExpected - nbImagesConverted);
        return 1;
    }
    else
        return 0;
}
