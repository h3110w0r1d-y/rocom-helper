#!/usr/bin/env bash
set -euo pipefail

# 备份脚本：静态编译 macOS 依赖。
# 用法：
#   ./scripts/build_macos_static_dependencies.sh qt
#   ./scripts/build_macos_static_dependencies.sh protobuf
#   ./scripts/build_macos_static_dependencies.sh pcapplusplus
#   ./scripts/build_macos_static_dependencies.sh all
#
# 生成后的项目 CMAKE_PREFIX_PATH：
#   /Users/yuyang/CLionProjects/libs/qt-6.11.1-macos-static;/Users/yuyang/CLionProjects/libs/protobuf-33.6-macos-static;/Users/yuyang/CLionProjects/libs/PcapPlusPlus-25.05-macos-static

LIBS_ROOT="${LIBS_ROOT:-/Users/yuyang/CLionProjects/libs}"
BUILD_ROOT="${BUILD_ROOT:-${LIBS_ROOT}/build}"

QT_SRC="${QT_SRC:-${LIBS_ROOT}/qt-6.11.1}"
QT_BUILD="${QT_BUILD:-${BUILD_ROOT}/qt-6.11.1-macos-static}"
QT_INSTALL="${QT_INSTALL:-${LIBS_ROOT}/qt-6.11.1-macos-static}"

PROTOBUF_SRC="${PROTOBUF_SRC:-${LIBS_ROOT}/protobuf-33.6}"
PROTOBUF_BUILD="${PROTOBUF_BUILD:-${BUILD_ROOT}/protobuf-33.6-macos-static}"
PROTOBUF_INSTALL="${PROTOBUF_INSTALL:-${LIBS_ROOT}/protobuf-33.6-macos-static}"

PCAPPP_SRC="${PCAPPP_SRC:-${LIBS_ROOT}/PcapPlusPlus-25.05}"
PCAPPP_BUILD="${PCAPPP_BUILD:-${BUILD_ROOT}/PcapPlusPlus-25.05-macos-static}"
PCAPPP_INSTALL="${PCAPPP_INSTALL:-${LIBS_ROOT}/PcapPlusPlus-25.05-macos-static}"

JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

build_qt() {
    rm -rf "${QT_BUILD}" "${QT_INSTALL}"
    mkdir -p "${QT_BUILD}"

    (
        cd "${QT_BUILD}"
        "${QT_SRC}/configure" \
            -prefix "${QT_INSTALL}" \
            -static \
            -release \
            -opensource \
            -confirm-license \
            -nomake examples \
            -nomake tests \
            -securetransport \
            -no-openssl \
            -qt-zlib \
            -qt-pcre \
            -qt-libpng \
            -qt-libjpeg \
            -qt-freetype \
            -qt-harfbuzz \
            -qt-sqlite \
            -skip qtwebengine \
            -skip qtmultimedia \
            -skip qtquick3d \
            -skip qt3d \
            -skip qtcharts \
            -skip qtlocation \
            -skip qtpositioning \
            -skip qtconnectivity \
            -skip qtsensors \
            -skip qtserialport \
            -skip qtserialbus \
            -skip qtwayland \
            -skip qtquick3dphysics \
            -skip qtspeech \
            -skip qtgrpc \
            -skip qtdoc \
            -skip qtmqtt \
            -skip qtnetworkauth \
            -skip qtwebview \
            -skip qtvirtualkeyboard \
            -skip qt5compat \
            -skip qtdeclarative \
            -skip qtimageformats \
            -skip qtlanguageserver \
            -skip qtshadertools \
            -skip qtactiveqt \
            -skip qtcanvaspainter \
            -skip qtcoap \
            -skip qtdatavis3d \
            -skip qtgraphs \
            -skip qttasktree \
            -skip qtlottie \
            -skip qtopcua \
            -skip qtopenapi \
            -skip qtquickeffectmaker \
            -skip qtquicktimeline \
            -skip qtremoteobjects \
            -skip qtscxml \
            -skip qtwebchannel \
            -skip qttools \
            -skip qtsvg \
            -skip qttranslations
    )

    cmake --build "${QT_BUILD}" --parallel "${JOBS}"
    cmake --install "${QT_BUILD}"
}

build_protobuf() {
    rm -rf "${PROTOBUF_BUILD}" "${PROTOBUF_INSTALL}"

    cmake -S "${PROTOBUF_SRC}" -B "${PROTOBUF_BUILD}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${PROTOBUF_INSTALL}" \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DCMAKE_CXX_EXTENSIONS=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -Dprotobuf_BUILD_SHARED_LIBS=OFF \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_BUILD_PROTOC_BINARIES=ON \
        -Dprotobuf_BUILD_LIBPROTOC=ON \
        -Dprotobuf_INSTALL=ON \
        -Dprotobuf_FORCE_FETCH_DEPENDENCIES=ON \
        -DABSL_PROPAGATE_CXX_STD=ON

    cmake --build "${PROTOBUF_BUILD}" --parallel "${JOBS}"
    cmake --install "${PROTOBUF_BUILD}"
}

build_pcapplusplus() {
    rm -rf "${PCAPPP_BUILD}" "${PCAPPP_INSTALL}"

    cmake -S "${PCAPPP_SRC}" -B "${PCAPPP_BUILD}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${PCAPPP_INSTALL}" \
        -DBUILD_SHARED_LIBS=OFF \
        -DPCAPPP_BUILD_EXAMPLES=OFF \
        -DPCAPPP_BUILD_TESTS=OFF \
        -DPCAPPP_BUILD_TUTORIALS=OFF \
        -DPCAPPP_INSTALL=ON

    cmake --build "${PCAPPP_BUILD}" --parallel "${JOBS}"
    cmake --install "${PCAPPP_BUILD}"
}

print_prefix_path() {
    echo
    echo "CMAKE_PREFIX_PATH=\"${QT_INSTALL};${PROTOBUF_INSTALL};${PCAPPP_INSTALL}\""
}

case "${1:-}" in
    qt)
        build_qt
        print_prefix_path
        ;;
    protobuf)
        build_protobuf
        print_prefix_path
        ;;
    pcapplusplus)
        build_pcapplusplus
        print_prefix_path
        ;;
    all)
        build_qt
        build_protobuf
        build_pcapplusplus
        print_prefix_path
        ;;
    *)
        sed -n '2,12p' "$0"
        exit 2
        ;;
esac
