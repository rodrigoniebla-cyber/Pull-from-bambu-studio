# Phase 1 Port: "One Tool-Change Per Layer" Image/Texture Printing → Bambu Studio

Port of the per-layer color solver, outer-wall line-width modulation, and
tool-change-collapsing logic from
[sentientstardust-dev/OrcaSlicer-ImageMap](https://github.com/sentientstardust-dev/OrcaSlicer-ImageMap)
(release **v1.0.22**, commit `92548381056dbf72836b0a1bdc455f238218dbfb`) into
[bambulab/BambuStudio](https://github.com/bambulab/BambuStudio)
(base commit `926a7192574bcb9b3a732e1ec59a46d79cb45466`, version 02.08.02.61).

## How this repository is laid out

Because this repository does not itself contain the Bambu Studio sources, the
deliverable is a PR-ready change set against the base commit above:

| Path | Content |
|---|---|
| `bambustudio/src/...` | All **newly added** files, in their in-tree locations (vendored libraries + the ported module) |
| `patches/imagemap-port-modified-files.patch` | Unified diff of the **modifications to existing** BambuStudio files (500 lines, 10 files) |
| `apply.sh` | Copies the new files into a BambuStudio checkout and applies the patch |

## What the feature does (when enabled)

With the toggle on, and a model painted multi-color the normal Bambu Studio way
(3MF mmu-segmentation painting and/or per-object/per-part filament assignment):

1. **Tool-change collapsing** — `ToolOrdering` derives the *rotation set*: the
   filaments used by object-body print regions (walls/infill), excluding any
   filament used for support or support interface. Each layer is assigned one
   *active* filament, cycling through the set across layers
   (C→M→Y→K→C→M→Y→K…, the balanced-sequence port of
   `build_balanced_component_sequence`). Every painted body extrusion on that
   layer is remapped to the active filament, so the object body needs **at most
   one tool change per layer** instead of one per painted region.
2. **Outer-wall line-width modulation** — for each external-perimeter path, the
   painted region's original filament color is the *target*. The vendored
   **ColorSolver** ("Generic Solver", working with any loaded filament set, not
   just CMYK) converts the target color into per-rotation-filament deposition
   weights using the **Pigment Painter** optical mixing model (or the
   prusa-fdm-mixer model). The weight of the layer's active filament scales the
   wall's line width between the configured min line width and the path's
   nominal width: full width where the active filament matches the desired
   color, narrowed (receding, letting neighboring layers' colors show) where it
   does not. Over one rotation cycle the wall surface therefore approximates
   the painted color — the "overhang modulation" technique of the origin
   project, with the per-layer deposition ratios coming from the solver.

## Files changed

### New: vendored libraries (`bambustudio/src/imagemap/`)

Kept deliberately separate from Bambu Studio core code; wired into the build
via `src/CMakeLists.txt`, and only `colorsolver` is linked into `libslic3r`.

| Directory | Role | License (files preserved) |
|---|---|---|
| `src/imagemap/pigment-painter/` | Color-mixing prediction (embedded 37 MB PNG LUT in `lut_wide.png.c`) | **GPLv3** — `COPYING` vendored |
| `src/imagemap/colorsolver/` | Target RGB → filament deposition ratios | **AGPLv3** — original license headers preserved in both sources |
| `src/imagemap/prusa-fdm-mixer/` | Alternative mix-prediction model, linked by ColorSolver (transitive dependency of the origin's ColorSolver, vendored for completeness) | **MIT** — `LICENSE` vendored |

Every vendored file (including the LUT `.c` file and the CMakeLists) carries a
top-of-file provenance comment naming the origin repo, origin file, release,
and commit; all original license headers are intact below the provenance note.

### New: ported module

* `src/libslic3r/ImageMapPerLayerColor.{hpp,cpp}` — the Phase-1 core, with
  per-function provenance notes:
  * `safe_mod`, `build_balanced_component_sequence` — **ported verbatim** from
    origin `TextureMapping.cpp`.
  * `active_filament_for_layer` — port of the tail of
    `TextureMappingManager::resolve_zone_component` (`sequence[layer % size]`).
  * `parse_filament_color` — port of `parse_hex_color`.
  * `Solver` — port of the generic-solver branch of
    `component_weights_for_sample` (origin `TextureMappingOffset.cpp`):
    candidate-set construction + `solve_color_solver_weights_for_target`,
    with per-target caching.
  * `modulated_outer_wall_width` — port of the width-clamping formulas of the
    origin's outer-wall gradient path (`GCode.cpp` ~11118–11144), including
    the positive-spacing lower bound `layer_height·(1−π/4)+1e-4`.
  * `rotation_filaments` — **adaptation, not a verbatim port** (see
    low-confidence flags below).

### Modified existing files (the 500-line patch)

| File | Change |
|---|---|
| `src/libslic3r/PrintConfig.cpp/.hpp` | 7 new options (below), all `comDevelop`, defaults inert |
| `src/libslic3r/Preset.cpp` | The 7 keys added to the print-options list |
| `src/libslic3r/GCode/ToolOrdering.hpp` | `LayerTools::image_map_filament_resolution` map + identity `resolve_image_map()`; declaration + rotation accessor |
| `src/libslic3r/GCode/ToolOrdering.cpp` | The four `resolve_mixed(result)` sites become `resolve_image_map(resolve_mixed(result))`; `resolve_image_map_per_layer_filaments()` called from both `sort_and_build_data` overloads right after `resolve_mixed_filaments()` |
| `src/libslic3r/GCode.hpp/.cpp` | Solver member + `init_image_map_per_layer_color()` (called once in `do_export`) + `image_map_apply_outer_wall_modulation()`; `_extrude` gains a 2-line shim that substitutes a re-widthed copy of the path only when the helper returns true |
| `src/CMakeLists.txt`, `src/libslic3r/CMakeLists.txt` | `add_subdirectory` for the three vendored libs; `colorsolver` added to `libslic3r`'s link list; new module sources registered |

## The default-off toggle

* **`image_map_per_layer_color_rotation`** (`coBool`, default **false**,
  `comDevelop`) — master switch, defined in `PrintConfig.cpp`. Checked by
  `ImageMapPerLayer::enabled()`; every feature entry point returns immediately
  when it is false.
* Supporting options (all inert unless the master switch is on):
  * `texture_mapping_outer_wall_gradient_global_strength` (float, 100) — ported definition
  * `texture_mapping_outer_wall_gradient_max_line_width` (float, 0.95 mm) — ported definition
  * `texture_mapping_outer_wall_gradient_min_line_width` (float, 0.32 mm) — ported definition
  * `image_map_generic_solver_lookup_mode` (int, 0 = closest mix)
  * `image_map_generic_solver_mode` (int, 255 = slicer default → Oklab soft-cap)
  * `image_map_generic_solver_mix_model` (int, 0 = Pigment Painter)

Changing any of these keys triggers a full reslice (they are intentionally not
listed in `Print::invalidate_state_by_config_options`, whose fallback branch
invalidates all steps — the conservative default).

## Disabled-state no-op audit (reasoning, not just assertion)

Every touched code path, traced with the toggle off:

1. **`ToolOrdering::resolve_image_map_per_layer_filaments`** — first statement
   clears `m_image_map_rotation` (already empty) and returns before touching
   any `LayerTools`. No state changes.
2. **`LayerTools::wall_filament / sparse_infill_filament / solid_infill_filament /
   extruder`** — the added `resolve_image_map()` wrapper does a `std::map::find`
   on a map that is empty when the feature is off, returning its argument
   unchanged. Identical return values, no side effects.
3. **`GCode::init_image_map_per_layer_color`** — sets
   `m_image_map_modulation_enabled = false` and returns at the toggle check.
4. **`GCode::_extrude`** — the shim calls
   `image_map_apply_outer_wall_modulation()`, whose first statement returns
   false when `m_image_map_modulation_enabled` is false; `path` then aliases
   the original parameter (same object, including for `ExtrusionPathSloped`,
   whose later `dynamic_cast` still sees the derived object). The remainder of
   `_extrude` is textually unchanged.
5. **Vendored libraries** — linked into the binary but no code path calls into
   them unless the solver is initialized, which only happens behind the toggle.
6. **Config plumbing** — new options only add defaulted values.

**One honest caveat on "bit-for-bit":** exported G-code embeds the full print
config as `; key = value` comment lines between `CONFIG_BLOCK_START/END`
(`GCode::append_full_config` dumps every key). The 7 new keys therefore add 7
comment lines to that metadata block even when the feature is disabled. This is
inherent to adding any config option to Bambu Studio (the upstream
mixed-filament feature has the same effect) and has zero effect on toolpaths,
motion, tool changes, or print quality — every executable line of G-code is
unchanged. If literal byte-identity of the config comment block is required,
the keys would have to be removed from `Preset.cpp`'s print-options list, at
the cost of the options not being persistable.

## Reuse of existing tool-change G-code (requirement 4)

No tool-change G-code generation was added or altered. Tool changes are
emitted, exactly as before, by iterating `layer_tools.extruders` in
`GCode::process_layer` → `set_extruder` → the existing `change_filament_gcode`
/ machine G-code path (M620 family for AMS, nozzle-change handling for
dual-nozzle machines). The feature only **shrinks the per-layer extruder list**
in `ToolOrdering` before that machinery runs — the change is frequency only,
never shape.

## H2C / multi-extruder preservation (requirement 3)

H2C ("Hyper Multi Color") support in this code base is the multi-extruder /
multi-nozzle filament grouping machinery. Traced H2C-related paths and how the
change relates to each:

* `ToolOrdering::get_recommended_filament_maps` and the
  `fmmManual`/`fmmNozzleManual` branches (`ToolOrdering.cpp` ~1876–1899,
  including the explicit "处理H2C的…" branches) — **untouched**. They consume
  per-layer filament lists collected from `m_layer_tools`; with the feature on
  they simply see fewer filaments per layer (ordinary physical filament ids),
  the same contract under which the upstream mixed-filament feature already
  feeds them.
* `reorder_extruders_for_minimum_flush_volume` (flush/grouping optimization for
  multi-nozzle machines) — **untouched**; runs *after* the collapse, mirroring
  the position of the existing `resolve_mixed_filaments()`.
* Wipe-tower planning incl. the H2C prime-volume special case
  (`Print.cpp` ~3820, `PrimeVolumeMode::pvmSaving`) — **untouched**; plans
  fewer tool changes but with identical logic per change.
* H2C/X2D timelapse handling (`GCode.cpp` ~4467) — **untouched**.
* Support filaments are excluded from the rotation set and never remapped or
  removed from `LayerTools::extruders`, so support/interface tool changes —
  including their H2C nozzle handling — are byte-identical.

The deliberate design choice, per the task's guidance, was **not** to hook the
origin's zone resolution into `collect_extruders` (which has diverged heavily),
but to piggyback the exact insertion point and remap mechanism Bambu Studio
already uses for its own virtual-filament feature (`resolve_mixed_filaments`),
so every downstream H2C path is exercised the same way it already is today.
If both features are configured at once, the port refuses to activate
(mixed-filament wins) rather than compose untested interactions.

## Low-confidence areas (flagged in code comments too)

1. **Rotation-set derivation** (`ImageMapPerLayer::rotation_filaments`,
   flagged in `ImageMapPerLayerColor.hpp` and `ToolOrdering.cpp`): the origin
   derives components from explicit TextureMappingZone filament lists; Phase 1
   has no zones, so the set is derived from painted body print-region configs.
   This is an adaptation, not a proven mapping. Corner cases: a filament used
   both for support and painting is excluded from rotation (that region keeps
   normal tool changes, so such a layer can exceed one tool change); per-layer
   custom filament switches (`extruder_override`) are also remapped when they
   point into the rotation set.
2. **Layer indexing**: the rotation advances by index into the merged
   `m_layer_tools` list (all objects + support layers z-merged), not by
   per-object layer id. For a single object this matches the origin's
   `layer_index`; for multiple objects of different heights printed "by layer"
   it is an approximation. The wall-modulation side is index-independent (it
   keys off the actual active tool), so the two sides cannot disagree.
3. **Per-path (not per-segment) width modulation**: with painting, the color
   target is constant per region, so a constant width per path is exact for the
   solver output; the origin's per-segment image sampling, dithering, halftone
   and centerline-shift machinery is Phase 2/3 scope. Consequence: narrowed
   walls recede slightly inward (outer surface texture), as in the origin's
   non-vertex "offset gradient" mode; the outward "vertex color match" mode
   (base width = configured max) was *not* ported because it requires the
   centerline-shift geometry to keep dimensions accurate.
4. **Wipe-tower sizing**: fewer per-layer tool changes shrink wipe-tower
   partitions via the existing (`fill_wipe_tower_partitions`) logic after the
   collapse. This is the intended benefit but has not been exercised end-to-end.

## Out of scope (Phase 2/3, per task)

Texture/image import, vertex-color import UI, gradient/halftone/projection
panels, per-segment sampling and dithering, top-surface contoning
(`TextureMappingContoning`), the raw-filament offset atlas, prime-tower image
painting, and 3MF persistence of zone definitions.

## Testing performed (and not performed)

* All four vendored translation units compile standalone (g++ 13, C++17),
  including the 37 MB LUT (compiled as C, as the origin build does).
* Every modified/added libslic3r file (`ImageMapPerLayerColor.cpp`,
  `ToolOrdering.cpp`, `GCode.cpp`, `PrintConfig.cpp`, `Preset.cpp`, and the
  headers they pull in) passes a full `g++ -fsyntax-only` check against the
  real Bambu Studio headers (system boost/TBB/eigen; OCCT stubbed with a
  header shim for checking only).
* A functional unit test (not committed; reproduced in this summary's history)
  exercised the ported logic with the real vendored libraries:
  * rotation sequence `{0,1,2,3}` → layers 0..8 yield `0 1 2 3 0 1 2 3 0`;
    weighted `{0:3, 1:1}` → `0 0 1 0`.
  * width modulation: weight 1 → nominal 0.42 mm, weight 0 → configured min
    0.32 mm, monotone in between; strength 0 disables modulation.
  * ColorSolver: a component color solves to weight ≈ 1.0 for itself; a mixed
    target yields weights summing to 1.0; both the prusa-fdm-mixer path and
    the Pigment Painter path (embedded-PNG LUT decode) produce sane output.
* **Not performed**: a full Bambu Studio build (the dependency tree — OCCT,
  OpenCV, wxWidgets, etc. — is not buildable in this environment), an
  end-to-end slicing comparison, and any print on real hardware. No claim is
  made that the feature produces correct colors on a physical printer; the
  disabled-state and H2C conclusions above are based on the code-path analysis
  described, not on runtime evidence.
