# Global Release optimizations (all targets).
# Compile: section splitting so link-time gc can drop unused sections.
# Link:    strip / dead_strip / gc-sections apply at final executable link.

add_compile_options(
        "$<$<CONFIG:Release>:-ffunction-sections>"
        "$<$<CONFIG:Release>:-fdata-sections>")

if(MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif()

if(APPLE)
    add_link_options(
            $<$<CONFIG:Release>:-Wl,-dead_strip>
            $<$<CONFIG:Release>:-Wl,-x>
            $<$<CONFIG:Release>:-Wl,-S>)
elseif(UNIX)
    add_link_options(
            $<$<CONFIG:Release>:-Wl,--gc-sections>
            $<$<CONFIG:Release>:-Wl,-s>)
endif()
