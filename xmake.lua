add_rules("mode.debug", "mode.release")

add_requires("freeimage", "libsquish")

target("BLPConverter")
    set_kind("binary")
    add_packages("freeimage")

    add_files("src/*.cpp")
    add_deps("blp", "simpleopt")

includes("lib/blp")
includes("lib/simpleopt")
