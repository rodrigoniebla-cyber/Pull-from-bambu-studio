# Pull-from-bambu-studio

Phase-1 port of the **"one tool-change per layer" image/texture color printing**
feature from
[OrcaSlicer-ImageMap](https://github.com/sentientstardust-dev/OrcaSlicer-ImageMap)
into [Bambu Studio](https://github.com/bambulab/BambuStudio).

* **Read [`PORT_SUMMARY.md`](PORT_SUMMARY.md) first** — files changed,
  provenance, the default-off toggle, low-confidence flags, and the
  disabled-state / H2C audit.
* `bambustudio/` — newly added files in their in-tree locations (the vendored
  ColorSolver / Pigment Painter / prusa-fdm-mixer libraries and the ported
  `ImageMapPerLayerColor` module).
* `patches/imagemap-port-modified-files.patch` — the diff to existing Bambu
  Studio files (against `bambulab/BambuStudio` commit
  `926a7192574bcb9b3a732e1ec59a46d79cb45466`, v02.08.02.61).
* `apply.sh /path/to/BambuStudio` — copies the new files and applies the patch
  (verified to reproduce the ported tree exactly on a clean base checkout).

The feature ships **off by default** behind the
`image_map_per_layer_color_rotation` print setting.
