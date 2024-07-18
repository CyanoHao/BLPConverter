add_rules("mode.debug", "mode.release")
set_languages("cxx17")

add_requires(
    "fmt ^10.2.1",
    "freeimage ^3.18.0",
    "libsquish ^1.15")

target("BLPConverter")
    set_kind("binary")
    add_packages("fmt", "freeimage")

    add_files("src/*.cpp")
    add_deps("blp", "simpleopt")

includes("lib/blp")
includes("lib/simpleopt")
