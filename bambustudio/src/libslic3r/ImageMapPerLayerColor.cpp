// Bambu Studio port of the "one tool-change per layer" image/texture color
// rotation from OrcaSlicer-ImageMap. See ImageMapPerLayerColor.hpp for the
// full provenance note.
//
//   Origin repo:  https://github.com/sentientstardust-dev/OrcaSlicer-ImageMap
//                 (release v1.0.22, commit 92548381056dbf72836b0a1bdc455f238218dbfb)
//   Origin files: src/libslic3r/TextureMapping.cpp, TextureMappingOffset.cpp, GCode.cpp
//   Original author: sentientstardust. License: AGPLv3 (same as Bambu Studio).

#include "ImageMapPerLayerColor.hpp"

#include "Print.hpp"
#include "PrintConfig.hpp"
#include "libslic3r.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>

namespace Slic3r {
namespace ImageMapPerLayer {

// ---------------------------------------------------------------------------
// Ported verbatim from OrcaSlicer-ImageMap src/libslic3r/TextureMapping.cpp
// (static helpers safe_mod / build_balanced_component_sequence).
// ---------------------------------------------------------------------------

static int safe_mod(int value, int divisor)
{
    if (divisor <= 0)
        return 0;
    int out = value % divisor;
    if (out < 0)
        out += divisor;
    return out;
}

static std::vector<unsigned int> build_balanced_component_sequence(const std::vector<unsigned int> &ids,
                                                                   const std::vector<int>          &weights)
{
    if (ids.empty())
        return {};

    std::vector<int> counts;
    counts.reserve(ids.size());
    for (size_t i = 0; i < ids.size(); ++i) {
        const int weight = i < weights.size() ? std::max(0, weights[i]) : 1;
        counts.emplace_back(weight);
    }
    if (std::all_of(counts.begin(), counts.end(), [](int v) { return v <= 0; }))
        counts.assign(ids.size(), 1);

    int total = std::accumulate(counts.begin(), counts.end(), 0);
    constexpr int MaxCycle = 64;
    if (total > MaxCycle) {
        const double scale = double(MaxCycle) / double(total);
        for (int &count : counts)
            count = count <= 0 ? 0 : std::max(1, int(std::lround(double(count) * scale)));
        total = std::accumulate(counts.begin(), counts.end(), 0);
    }
    if (total <= 0)
        return {};

    std::vector<unsigned int> sequence;
    sequence.reserve(size_t(total));
    std::vector<int> debt(ids.size(), 0);
    for (int step = 0; step < total; ++step) {
        size_t best_idx = 0;
        int best_debt = std::numeric_limits<int>::lowest();
        for (size_t idx = 0; idx < counts.size(); ++idx) {
            debt[idx] += counts[idx];
            if (debt[idx] > best_debt) {
                best_debt = debt[idx];
                best_idx = idx;
            }
        }
        sequence.emplace_back(ids[best_idx]);
        debt[best_idx] -= total;
    }
    return sequence;
}

// ---------------------------------------------------------------------------

bool enabled(const PrintConfig &config)
{
    return config.image_map_per_layer_color_rotation.value;
}

bool parse_filament_color(const std::string &hex, std::array<float, 3> &rgb_out)
{
    // Port of parse_hex_color() (TextureMapping.cpp).
    if (hex.size() < 7 || hex[0] != '#')
        return false;
    int r = 0, g = 0, b = 0;
    try {
        r = std::stoi(hex.substr(1, 2), nullptr, 16);
        g = std::stoi(hex.substr(3, 2), nullptr, 16);
        b = std::stoi(hex.substr(5, 2), nullptr, 16);
    } catch (...) {
        return false;
    }
    rgb_out = { float(r) / 255.f, float(g) / 255.f, float(b) / 255.f };
    return true;
}

std::vector<unsigned int> rotation_filaments(const Print &print)
{
    const PrintConfig &config = print.config();
    const size_t num_filaments = config.filament_colour.values.size();
    if (num_filaments < 2)
        return {};

    // Filaments used for support anywhere in the print are protected: they are
    // never rotated so the existing support tool-change behavior (including on
    // multi-extruder / H2C machines) is left completely untouched.
    std::set<unsigned int> protected_filaments;
    for (const PrintObject *object : print.objects()) {
        const int support = object->config().support_filament.value;
        if (support > 0 && size_t(support) <= num_filaments)
            protected_filaments.insert(unsigned(support - 1));
        const int support_interface = object->config().support_interface_filament.value;
        if (support_interface > 0 && size_t(support_interface) <= num_filaments)
            protected_filaments.insert(unsigned(support_interface - 1));
    }

    std::set<unsigned int> rotation;
    auto add_filament = [&](int filament_1based) {
        if (filament_1based > 0 && size_t(filament_1based) <= num_filaments &&
            protected_filaments.count(unsigned(filament_1based - 1)) == 0)
            rotation.insert(unsigned(filament_1based - 1));
    };
    for (size_t region_idx = 0; region_idx < print.num_print_regions(); ++region_idx) {
        const PrintRegionConfig &region_config = print.get_print_region(region_idx).config();
        add_filament(region_config.wall_filament.value);
        add_filament(region_config.solid_infill_filament.value);
        add_filament(region_config.sparse_infill_filament.value);
    }

    return std::vector<unsigned int>(rotation.begin(), rotation.end());
}

std::vector<unsigned int> rotation_sequence(const std::vector<unsigned int> &rotation,
                                            const std::vector<int>          &weights)
{
    return build_balanced_component_sequence(rotation, weights);
}

unsigned int active_filament_for_layer(const std::vector<unsigned int> &sequence, int layer_index)
{
    // Port of the tail of TextureMappingManager::resolve_zone_component().
    if (sequence.empty())
        return 0;
    return sequence[size_t(safe_mod(layer_index, int(sequence.size())))];
}

// ---------------------------------------------------------------------------
// Solver
// ---------------------------------------------------------------------------

static ColorSolverMode effective_solver_mode_from_config(int mode)
{
    // Port of TextureMappingZone::effective_generic_solver_mode(): the
    // sentinel 255 (GenericSolverDefault) resolves to the slicer default
    // OklabSoftCap4Dark4; other values clamp to the valid enum range.
    constexpr int GenericSolverDefault = 255;
    if (mode == GenericSolverDefault)
        return ColorSolverMode::OklabSoftCap4Dark4;
    return color_solver_mode_from_index(std::clamp(mode, 0, 2));
}

bool Solver::init(const PrintConfig &config, const std::vector<unsigned int> &rotation)
{
    m_valid = false;
    m_rotation = rotation;
    m_component_colors.clear();
    m_filament_colors.clear();
    m_weights_by_target.clear();
    m_candidates = nullptr;
    if (rotation.size() < 2)
        return false;

    const std::vector<std::string> &colours = config.filament_colour.values;
    m_filament_colors.resize(colours.size());
    for (size_t i = 0; i < colours.size(); ++i)
        if (!parse_filament_color(colours[i], m_filament_colors[i]))
            m_filament_colors[i] = { 0.f, 0.f, 0.f };

    m_component_colors.reserve(rotation.size());
    for (unsigned int filament : rotation) {
        if (size_t(filament) >= m_filament_colors.size())
            return false;
        m_component_colors.emplace_back(m_filament_colors[filament]);
    }

    m_lookup_mode = color_solver_lookup_mode_from_index(config.image_map_generic_solver_lookup_mode.value);
    m_solver_mode = effective_solver_mode_from_config(config.image_map_generic_solver_mode.value);
    m_mix_model   = color_solver_mix_model_from_index(config.image_map_generic_solver_mix_model.value);

    // Port of the candidate-set construction used by the generic solver path
    // of component_weights_for_sample() (TextureMappingOffset.cpp).
    m_candidates = &color_solver_candidates(m_candidate_cache, m_component_colors, m_mix_model);
    if (m_candidates->empty()) {
        m_candidates = nullptr;
        return false;
    }
    m_valid = true;
    return true;
}

bool Solver::in_rotation(unsigned int filament_0based) const
{
    return std::find(m_rotation.begin(), m_rotation.end(), filament_0based) != m_rotation.end();
}

const std::vector<float> &Solver::weights_for_target(unsigned int target_filament)
{
    auto it = m_weights_by_target.find(target_filament);
    if (it != m_weights_by_target.end())
        return it->second;
    if (!m_valid || m_candidates == nullptr || size_t(target_filament) >= m_filament_colors.size())
        return m_empty_weights;

    const std::array<float, 3> &target_rgb = m_filament_colors[target_filament];
    // Port of the generic-solver branch of component_weights_for_sample():
    // solve_color_solver_weights_for_target() over the rotation candidates.
    std::vector<float> weights = solve_color_solver_weights_for_target(*m_candidates,
                                                                       target_rgb,
                                                                       m_lookup_mode,
                                                                       m_solver_mode);
    if (weights.size() != m_component_colors.size())
        weights.clear();
    for (float &w : weights)
        w = std::clamp(w, 0.f, 1.f);
    return m_weights_by_target.emplace(target_filament, std::move(weights)).first->second;
}

float Solver::weight_for(unsigned int target_filament, unsigned int active_filament)
{
    const auto active_it = std::find(m_rotation.begin(), m_rotation.end(), active_filament);
    if (active_it == m_rotation.end())
        return -1.f;
    const std::vector<float> &weights = weights_for_target(target_filament);
    if (weights.size() != m_rotation.size())
        return -1.f;
    return weights[size_t(active_it - m_rotation.begin())];
}

// ---------------------------------------------------------------------------
// Outer-wall width modulation
// ---------------------------------------------------------------------------

float modulated_outer_wall_width(float weight,
                                 float base_outer_width_mm,
                                 float config_min_width_mm,
                                 float config_max_width_mm,
                                 float global_strength_pct,
                                 float layer_height_mm)
{
    // Port of the clamping formulas of the outer-wall gradient path in origin
    // GCode.cpp (lines ~11118-11144, non-vertex "offset gradient" mode where
    // base_outer_width_mm = path_outer_width_mm):
    const float base = std::max(0.01f, base_outer_width_mm);
    const float safe_layer_height = std::max(0.01f, layer_height_mm);
    const float config_min = std::clamp(config_min_width_mm, 0.05f, base);
    const float min_width_for_positive_spacing = safe_layer_height * float(1. - 0.25 * M_PI) + 1e-4f;
    const float safe_min = std::clamp(std::max(config_min, min_width_for_positive_spacing), 0.05f, base);
    const float max_width_delta = std::max(0.f, base - safe_min);
    const float strength = std::clamp(global_strength_pct / 100.f, 0.f, 1.f);
    const float effective_delta = max_width_delta * strength;

    const float w = std::clamp(weight, 0.f, 1.f);
    float width = base - (1.f - w) * effective_delta;
    // texture_mapping_outer_wall_gradient_max_line_width acts as an absolute cap.
    width = std::min(width, std::max(0.05f, config_max_width_mm));
    return std::clamp(width, safe_min, base);
}

double flow_scale_for_width_change(float old_width_mm, float new_width_mm, float height_mm)
{
    // Rounded-rectangle extrusion cross-section, as used by Slic3r::Flow:
    //   area = h * (w - h * (1 - PI/4))
    const double h = double(height_mm);
    if (h <= 0.)
        return 1.;
    auto area = [h](double w) { return h * (w - h * (1. - 0.25 * M_PI)); };
    const double old_area = area(double(old_width_mm));
    const double new_area = area(double(new_width_mm));
    if (!(old_area > 0.) || !(new_area > 0.))
        return 1.;
    return new_area / old_area;
}

} // namespace ImageMapPerLayer
} // namespace Slic3r
