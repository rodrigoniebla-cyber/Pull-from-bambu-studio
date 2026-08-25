# Vendored into Bambu Studio from OrcaSlicer-ImageMap.
# Origin repo: https://github.com/sentientstardust-dev/OrcaSlicer-ImageMap
# Origin file: deps_src/prusa-fdm-mixer/CMakeLists.txt (release v1.0.22, commit 92548381056dbf72836b0a1bdc455f238218dbfb)
# License: MIT (see LICENSE in this directory).
cmake_minimum_required(VERSION 3.13)
project(prusa_fdm_mixer)

add_library(prusa_fdm_mixer STATIC
    prusa_fdm_mixer.cpp
    prusa_fdm_mixer.hpp
)

target_include_directories(prusa_fdm_mixer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(prusa_fdm_mixer PUBLIC cxx_std_17)
