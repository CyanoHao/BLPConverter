# BLP Converter

Forked from [Kanma/BLPConverter](https://github.com/Kanma/BLPConverter).

## Summary

A command-line tool to convert BLP image files to PNG or TGA format. The BLP
images are used by Blizzard games.

Supports the following BLP2 formats:

- Uncompressed without alpha channel
- Uncompressed with alpha channel (1-, 4-, and 8-bits)
- Uncompressed raw BGRA (known as RAW3)
- DXT1 without alpha channel
- DXT1 with alpha channel (1-bit)
- DXT3 with alpha channel (4- and 8-bits)
- DXT5 with alpha channel (8-bit)

Works on Windows and Linux.

## Compilation

```bash
xmake build
xmake install -o dist
```

The executable will be put in `dist/bin`.

## Usage

(Copied from ./BLPConverter --help)

```
Usage: ./BLPConverter [options] <blp_filename> [<blp_filename> ... <blp_filename>]

Options:
  --help, -h:      Display this help
  --infos, -i:     Display informations about the BLP file(s) (no conversion)
  --dest, -o:      Folder where the converted image(s) must be written to (default: './')
  --format, -f:    'png' or 'tga' (default: png)
  --miplevel, -m:  The specific mip level to convert (default: 0, the bigger one)
```

## Extras

The Python script 'extra/convert_all.py' can be used to convert recursively in-place
all the BLP files in a hierarchy of folders.

## Dependencies

Dependencies are [managed by xmake](./xmake.lua). `xmake build` will automatically download and install the dependencies.

## License

BLPConverter is is made available under the MIT License. The text of the license is
in the file `LICENSE`.

Under the MIT License you may use BLPConverter for any purpose you wish, without
warranty, and modify it if you require, subject to one condition:

> The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software.

In practice this means that whenever you distribute your application, whether as
binary or as source code, you must include somewhere in your distribution the
text in the file `LICENSE`. This might be in the printed documentation, as a
file on delivered media, or even on the credits / acknowledgements of the
runtime application itself; any of those would satisfy the requirement.

Even if the license doesn't require it, please consider to contribute your
modifications back to the community.
