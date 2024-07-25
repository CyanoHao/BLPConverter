#pragma once

#include <memory>

struct FIBITMAP;

struct FIBITMAP_ptr : public std::unique_ptr<FIBITMAP, void (*)(FIBITMAP *)>
{
    FIBITMAP_ptr(int width,
                 int height,
                 int bpp,
                 unsigned red_mask = 0,
                 unsigned green_mask = 0,
                 unsigned blue_mask = 0);

    operator FIBITMAP *() const
    {
        return get();
    }
};

namespace freeimage
{

enum class Format
{
    PNG,
    TARGA,
};

void Initialise(bool load_local_plugins_only = false);

void DeInitialise();

uint8_t *GetScanLine(FIBITMAP *dib, int scanline);

bool Save(Format fif, FIBITMAP *dib, const char *filename, int flags = 0);

} // namespace freeimage
