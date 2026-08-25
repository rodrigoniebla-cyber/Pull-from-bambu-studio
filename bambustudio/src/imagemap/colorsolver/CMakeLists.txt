# Vendored into Bambu Studio from OrcaSlicer-ImageMap.
# Origin repo: https://github.com/sentientstardust-dev/OrcaSlicer-ImageMap
# Origin file: deps_src/colorsolver/CMakeLists.txt (release v1.0.22, commit 92548381056dbf72836b0a1bdc455f238218dbfb)
# License: AGPLv3 (see the license headers in ColorSolver.hpp/.cpp).
cmake_minimum_required(VERSION 3.13)
project(colorsolver)

add_library(colorsolver STATIC
    ColorSolver.cpp
    ColorSolver.hpp
)

target_include_directories(colorsolver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(colorsolver PUBLIC cxx_std_17)
target_link_libraries(colorsolver PUBLIC pigment_painter prusa_fdm_mixer)
