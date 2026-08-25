// Vendored into Bambu Studio from OrcaSlicer-ImageMap (Pigment Painter dependency).
// Origin repo:   https://github.com/sentientstardust-dev/OrcaSlicer-ImageMap
// Origin file:   deps_src/pigment-painter/pigment_painter_mixer.hpp (release v1.0.22, commit 92548381056dbf72836b0a1bdc455f238218dbfb)
// License:       GNU General Public License v3 (GPLv3) — see COPYING in this directory.

#ifndef PIGMENT_PAINTER_MIXER_HPP
#define PIGMENT_PAINTER_MIXER_HPP

#include <array>
#include <vector>

namespace pigment_painter {

std::array<float, 3> mix_srgb(const std::vector<std::array<float, 3>> &colors,
                              const std::vector<float>                &weights);

std::array<float, 3> mix_srgb(const std::vector<std::array<float, 3>> &colors,
                              const std::vector<int>                  &weights);

} // namespace pigment_painter

#endif
