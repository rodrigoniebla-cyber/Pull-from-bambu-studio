// Bambu Studio port of the "one tool-change per layer" image/texture color
// rotation from OrcaSlicer-ImageMap (Phase 1: solver + outer-wall line-width
// modulation + tool-change collapsing, driven by existing 3MF filament
// painting; no texture/image import).
//
// Ported from:
//   Origin repo:  https://github.com/sentientstardust-dev/OrcaSlicer-ImageMap
//                 (release v1.0.22, commit 92548381056dbf72836b0a1bdc455f238218dbfb)
//   Origin files: src/libslic3r/TextureMapping.cpp
//                   (safe_mod, build_balanced_component_sequence,
//                    TextureMappingManager::resolve_zone_component, parse_hex_color)
//                 src/libslic3r/TextureMappingOffset.cpp
//                   (component_weights_for_sample -> generic solver path)
//                 src/libslic3r/GCode.cpp
//                   (outer-wall gradient line-width clamping formulas)
//   Original author: sentientstardust. License: AGPLv3 (same as Bambu Studio).
//
// The vendored solver dependencies live in src/imagemap/ (see its README.md).

#ifndef slic3r_ImageMapPerLayerColor_hpp_
#define slic3r_ImageMapPerLayerColor_hpp_

#include <array>
#include <map>
#include <string>
#include <vector>

#include "ColorSolver.hpp"

namespace Slic3r {

class Print;
class PrintConfig;

namespace ImageMapPerLayer {

// True when the master toggle "image_map_per_layer_color_rotation" is enabled.
// Everything in this module must be behind this check; with the toggle off
// (the default) no caller may alter its behavior in any way.
bool enabled(const PrintConfig &config);

// 0-based filament ids used by object-body print regions (wall / solid infill /
// sparse infill filaments — i.e. what per-object assignment and 3MF mmu
// painting produce), excluding filaments used for support or support
// interface anywhere in the print, sorted ascending.
//
// This is the fixed rotation set the print cycles through, one filament per
// layer. It is computed identically (and deterministically) by ToolOrdering
// (tool-change collapsing) and GCode (outer-wall width modulation) so the two
// stay consistent.
//
// LOW-CONFIDENCE MAPPING NOTE: OrcaSlicer-ImageMap derives this set from its
// own TextureMappingZone component ids (an explicit per-zone filament list).
// Bambu Studio has no zones in Phase 1, so the set is derived from the print
// regions the existing painting pipeline creates. Filaments also used as
// support/support-interface are conservatively excluded so the support
// tool-change path is never rerouted.
std::vector<unsigned int> rotation_filaments(const Print &print);

// Balanced per-layer sequence over the rotation set.
// Port of build_balanced_component_sequence() (TextureMapping.cpp); Phase 1
// uses equal weights, which yields a plain cycle (e.g. C->M->Y->K->C->...).
std::vector<unsigned int> rotation_sequence(const std::vector<unsigned int> &rotation,
                                            const std::vector<int>          &weights = {});

// Active filament for a layer. Port of the tail of
// TextureMappingManager::resolve_zone_component(): sequence[layer % size].
unsigned int active_filament_for_layer(const std::vector<unsigned int> &sequence, int layer_index);

// Parse "#RRGGBB" (extra characters such as an alpha suffix are ignored) into
// linear 0..1 sRGB components. Port of parse_hex_color() (TextureMapping.cpp).
bool parse_filament_color(const std::string &hex, std::array<float, 3> &rgb_out);

// Generic color solver over the rotation set: converts the color of a painted
// target filament into per-rotation-filament deposition weights.
// Port of the generic-solver branch of component_weights_for_sample()
// (TextureMappingOffset.cpp), backed by the vendored ColorSolver library.
class Solver
{
public:
    Solver() = default;

    // Reads filament_colour and the image_map_generic_solver_* options.
    // Returns false (and leaves the solver invalid) when colors are missing.
    bool init(const PrintConfig &config, const std::vector<unsigned int> &rotation);

    bool valid() const { return m_valid; }
    bool in_rotation(unsigned int filament_0based) const;

    // Weight in [0,1] of `active_filament` when approximating the color of
    // `target_filament`; both 0-based. Returns a negative value when either
    // filament is unknown / outside the rotation. Results are cached.
    float weight_for(unsigned int target_filament, unsigned int active_filament);

private:
    const std::vector<float> &weights_for_target(unsigned int target_filament);

    bool                                m_valid { false };
    std::vector<unsigned int>           m_rotation;
    std::vector<std::array<float, 3>>   m_component_colors;
    std::vector<std::array<float, 3>>   m_filament_colors;
    ColorSolverCandidateCache           m_candidate_cache;
    const ColorSolverCandidateSet      *m_candidates { nullptr };
    ColorSolverLookupMode               m_lookup_mode { ColorSolverLookupMode::ClosestMix };
    ColorSolverMode                     m_solver_mode { ColorSolverMode::OklabSoftCap4Dark4 };
    ColorSolverMixModel                 m_mix_model { ColorSolverMixModel::PigmentPainter };
    std::map<unsigned int, std::vector<float>> m_weights_by_target;
    std::vector<float>                  m_empty_weights;
};

// Map a solver weight to an outer-wall line width. Port of the non-vertex
// ("offset gradient") clamping formulas of GCode::_extrude() in origin
// GCode.cpp (lines ~11118-11144): the wall keeps its nominal width where the
// active filament fully matches the target color (weight 1) and narrows down
// toward the configured/safe minimum where it does not (weight 0), so the
// surrounding layers' colors show through.
//   base_outer_width_mm     nominal width of the path being modulated
//   config_min_width_mm     texture_mapping_outer_wall_gradient_min_line_width
//   config_max_width_mm     texture_mapping_outer_wall_gradient_max_line_width (upper cap)
//   global_strength_pct     texture_mapping_outer_wall_gradient_global_strength
//   layer_height_mm         used for the positive-spacing lower bound
float modulated_outer_wall_width(float weight,
                                 float base_outer_width_mm,
                                 float config_min_width_mm,
                                 float config_max_width_mm,
                                 float global_strength_pct,
                                 float layer_height_mm);

// Volumetric flow (mm^3/mm) scale factor when an extrusion of rounded-rectangle
// cross-section (width x height) is re-issued at new_width. Returns 1 when the
// inputs are degenerate.
double flow_scale_for_width_change(float old_width_mm, float new_width_mm, float height_mm);

} // namespace ImageMapPerLayer
} // namespace Slic3r

#endif // slic3r_ImageMapPerLayerColor_hpp_
