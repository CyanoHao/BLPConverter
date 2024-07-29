#include "FIfix.h"

#include <FreeImage.h>

FIBITMAP_ptr::FIBITMAP_ptr(
    int width, int height, int bpp, unsigned red_mask, unsigned green_mask, unsigned blue_mask)
    : std::unique_ptr<FIBITMAP, void (*)(FIBITMAP *)>(
          FreeImage_Allocate(width, height, bpp, red_mask, green_mask, blue_mask),
          &FreeImage_Unload)
{
}

namespace freeimage
{

void Initialise(bool load_local_plugins_only)
{
    FreeImage_Initialise(load_local_plugins_only);
}

void DeInitialise()
{
    FreeImage_DeInitialise();
}

uint8_t *GetScanLine(FIBITMAP *dib, int scanline)
{
    return FreeImage_GetScanLine(dib, scanline);
}

template <typename Char>
bool SaveImpl(Format format, FIBITMAP *dib, const Char *filename, int flags)
{
    FREE_IMAGE_FORMAT fif = FIF_PNG;
    switch (format)
    {
    case Format::PNG:
        fif = FIF_PNG;
        break;
    case Format::TARGA:
        fif = FIF_TARGA;
        break;
    }
    if constexpr (std::is_same_v<Char, wchar_t>)
        return FreeImage_SaveU(fif, dib, filename, flags);
    else
        return FreeImage_Save(fif, dib, filename, flags);
}

bool Save(Format format, FIBITMAP *dib, const char *filename, int flags)
{
    return SaveImpl(format, dib, filename, flags);
}

bool Save(Format format, FIBITMAP *dib, const wchar_t *filename, int flags)
{
    return SaveImpl(format, dib, filename, flags);
}

} // namespace freeimage
