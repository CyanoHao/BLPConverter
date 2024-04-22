target("blp")
    set_kind("static")
    add_packages("freeimage", "libsquish")

    add_files("src/*.cpp")
    add_headerfiles("include/*.h")
    add_includedirs("include", {public = true})
