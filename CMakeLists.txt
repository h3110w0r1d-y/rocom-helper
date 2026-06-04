cmake_minimum_required(VERSION 3.27)

project(roco_helper LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql HttpServer Network)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PROTOBUF_PC REQUIRED IMPORTED_TARGET protobuf)

find_package(PcapPlusPlus REQUIRED CONFIG)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE RWTD_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.cpp"
)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/events/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/events/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/traffic/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/traffic/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/storage/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/storage/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/http/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/http/*.cpp"
)

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
        PkgConfig::PROTOBUF_PC
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

target_compile_features(roco_helper PRIVATE cxx_std_17)

target_link_options(roco_helper PRIVATE -Wl,-dead_strip)
