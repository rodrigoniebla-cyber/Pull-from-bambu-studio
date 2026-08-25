#!/bin/sh
# Apply the OrcaSlicer-ImageMap Phase-1 port to a BambuStudio checkout.
#
# Usage: ./apply.sh /path/to/BambuStudio
#
# The patch was generated against bambulab/BambuStudio commit
# 926a7192574bcb9b3a732e1ec59a46d79cb45466 (version 02.08.02.61).
set -e

TARGET="$1"
if [ -z "$TARGET" ] || [ ! -d "$TARGET/src/libslic3r" ]; then
    echo "usage: $0 /path/to/BambuStudio (a checkout containing src/libslic3r)" >&2
    exit 1
fi
HERE="$(cd "$(dirname "$0")" && pwd)"

# 1. New files (vendored color libraries + the ported module).
cp -rv "$HERE/bambustudio/src/." "$TARGET/src/"

# 2. Modifications to existing BambuStudio files.
git -C "$TARGET" apply --verbose "$HERE/patches/imagemap-port-modified-files.patch"

echo "Done. Feature toggle: image_map_per_layer_color_rotation (default off)."
