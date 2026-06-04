cmake_minimum_required(VERSION 3.27)

project(roco_helper LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql HttpServer Network)
find_package(OpenSSL REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(PcapPlusPlus REQUIRED CONFIG)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

file(GLOB_RECURSE RWTD_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.cpp"
)
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(FILTER APP_SOURCES EXCLUDE REGEX ".*/src/rwtd/.*")

file(GLOB_RECURSE APP_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/*"
)

add_library(rwtd STATIC
        ${RWTD_SOURCES}
)

target_include_directories(rwtd PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/proto"
)

target_link_libraries(rwtd
    PUBLIC
        Qt6::Core
        protobuf::libprotobuf
    PRIVATE
        OpenSSL::Crypto
        PcapPlusPlus::Pcap++
)

target_compile_features(rwtd PUBLIC cxx_std_17)

qt_add_executable(roco_helper
        src/main.cpp
        ${APP_SOURCES}
)

qt_add_resources(roco_helper "app_resources"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        FILES ${APP_RESOURCE_FILES}
)

target_include_directories(roco_helper PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(roco_helper PRIVATE
        Qt6::Widgets
        Qt6::Sql
        Qt6::HttpServer
        Qt6::Network
        rwtd
)

target_compile_options(roco_helper PRIVATE
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
        $<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-O2>
)
target_compile_features(roco_helper PRIVATE cxx_std_17)

if(APPLE)
    target_link_options(roco_helper PRIVATE
            $<$<CONFIG:Release>:-Wl,-dead_strip>
    )
elseif(WIN32)
    if(MSVC)
        target_link_options(roco_helper PRIVATE
                $<$<CONFIG:Release>:/OPT:REF>
                $<$<CONFIG:Release>:/OPT:ICF>
        )
    else()
        target_link_options(roco_helper PRIVATE
                $<$<CONFIG:Release>:-Wl,--gc-sections>
                $<$<CONFIG:Release>:-s>
        )
    endif()
endif()
