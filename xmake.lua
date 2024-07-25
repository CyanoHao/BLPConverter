add_rules("mode.debug", "mode.release")
set_languages("cxx17")

add_requires(
    "cli11 ^2.4.2",
    "fmt ^10.2.1",
    "freeimage ^3.18.0",
    "libsquish ^1.15",
    "nowide_standalone ^11.3.0")

target("BLPConverter")
    set_kind("binary")
    add_packages("cli11", "fmt", "freeimage", "nowide_standalone")

    add_files("src/*.cpp")
    add_deps("blp")

includes("lib/blp")
