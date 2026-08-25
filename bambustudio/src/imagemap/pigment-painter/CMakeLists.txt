# Vendored into Bambu Studio from OrcaSlicer-ImageMap.
# Origin repo: https://github.com/sentientstardust-dev/OrcaSlicer-ImageMap
# Origin file: deps_src/pigment-painter/CMakeLists.txt (release v1.0.22, commit 92548381056dbf72836b0a1bdc455f238218dbfb)
# License: GPLv3 (see COPYING in this directory).
cmake_minimum_required(VERSION 3.13)
project(pigment_painter)

if(NOT TARGET PNG::PNG)
    find_package(PNG REQUIRED)
endif()

add_library(pigment_painter STATIC
    lut_wide.png.c
    pigment_painter_mixer.cpp
    pigment_painter_mixer.hpp
)

target_include_directories(pigment_painter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pigment_painter PUBLIC cxx_std_17)
target_link_libraries(pigment_painter PRIVATE PNG::PNG)
