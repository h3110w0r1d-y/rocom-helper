# MinGW fully-static executable linking (Windows only).
#
# Problem:
#   - Global -static is undone when Qt6Sql appends -Wl,-Bdynamic late in the link line.
#   - Qt FindWrapZSTD prefers libzstd_shared (libzstd.dll.a) over libzstd_static.
#   - MSYS2 libgcc_s.a is an auto-import stub; linking it still pulls libgcc_s_seh-1.dll.
#
# Fix:
#   1. Predefine WrapZSTD::WrapZSTD with libzstd_static before find_package(Qt6).
#   2. Global -static / -static-libgcc / -static-libstdc++ / gc-sections / strip.
#   3. Re-pin static runtime at link tail (libgcc_eh, not libgcc_s) after <LINK_LIBRARIES>.

function(roco_setup_mingw_static_linking)
    if(NOT MINGW)
        return()
    endif()

    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" PARENT_SCOPE)
    set(PKG_CONFIG_ARGN "--static" PARENT_SCOPE)

    find_package(zstd CONFIG REQUIRED)
    if(NOT TARGET WrapZSTD::WrapZSTD)
        add_library(WrapZSTD::WrapZSTD INTERFACE IMPORTED)
        set_target_properties(WrapZSTD::WrapZSTD PROPERTIES
                INTERFACE_LINK_LIBRARIES "zstd::libzstd_static")
    endif()

    set(CMAKE_CXX_LINK_EXECUTABLE
            "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES> -Wl,-Bstatic -lgcc_eh -lgcc -lstdc++ -lwinpthread"
            PARENT_SCOPE)

    add_link_options(
            -static
            -static-libgcc
            -static-libstdc++
            $<$<CONFIG:Release>:-Wl,--gc-sections>
            $<$<CONFIG:Release>:-s>)
endfunction()
