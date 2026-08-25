#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# ci/build-macos.sh — one-shot macOS (arm64 / Apple Silicon) build of the
# ImageMap "one tool-change per layer" port of this repository.
#
# Run by .github/workflows/build-macos-dmg.yml on a macos-26 runner
# (GH_TOKEN must be exported; the script also works locally on a Mac).
#
# What it does:
#   1. Clones bambulab/BambuStudio at the pinned base commit (v02.08.02.61).
#   2. Builds the dependency bundle for arm64 — unless a prebuilt
#      BambuStudio_dep_mac_arm64.tar.gz (actions/cache or manual) is present.
#      After a from-scratch deps build the tarball is (re)created so the
#      workflow's cache/save step can pick it up.
#   3. Applies this repository's port (./apply.sh: new vendored files + patch).
#   4. Builds the slicer with upstream's own BuildMac.sh flow.
#   5. Ad-hoc codesigns the .app, packs a .dmg (+ .sha256), and attaches both
#      to the GitHub Release tagged below.
#
# The build steps mirror upstream's .github/workflows/build_deps.yml and
# build_bambu.yml (brew cmake@3.31.0 pin, zstd dance, BuildMac.sh flags).
# ---------------------------------------------------------------------------
set -euo pipefail

BASE_REPO=bambulab/BambuStudio
BASE_SHA=926a7192574bcb9b3a732e1ec59a46d79cb45466   # v02.08.02.61 (port is pinned to this)
DEPS_TARBALL=BambuStudio_dep_mac_arm64.tar.gz
RELEASE_TAG=imagemap-v02.08.02.61
BUILD_ARCH=arm64          # Apple Silicon only (halves CI time vs universal)
DMG_SUFFIX=ImageMap

log() { printf '\n\033[1;34m=== %s ===\033[0m\n' "$*"; }

[ -n "${GH_TOKEN:-}" ] || { echo "ERROR: GH_TOKEN must be set (release upload)."; exit 1; }
export GH_TOKEN
REPO="${GITHUB_REPOSITORY:-rodrigoniebla-cyber/Pull-from-bambu-studio}"
TARGET_SHA="${GITHUB_SHA:-$(git rev-parse HEAD)}"

# cd to this repository's root (the overlay: apply.sh, bambustudio/, patches/)
cd "$(dirname "$0")/.."
ROOT="$PWD"

log "Stage 0: brew prerequisites (cmake@3.31.0 pin, as upstream CI)"
brew install automake texinfo
brew unlink cmake || true
brew tap-new "$USER/old-cmake" >/dev/null
brew extract --version=3.31.0 cmake "$USER/old-cmake"
brew install "$USER/old-cmake/cmake@3.31.0"
brew link --overwrite cmake@3.31.0
brew pin cmake@3.31.0

log "Stage 1: clone upstream $BASE_REPO @ $BASE_SHA"
if [ ! -d BambuStudio/.git ]; then
    git init BambuStudio
    (
        cd BambuStudio
        git remote add origin "https://github.com/${BASE_REPO}.git"
        git fetch --depth 1 origin "$BASE_SHA"
        git checkout FETCH_HEAD
    )
else
    echo "BambuStudio checkout already present, reusing."
fi

log "Stage 2: dependencies (tarball/cache first, else build from source)"
if [ -d "BambuStudio/deps/build/${BUILD_ARCH}/BambuStudio_deps/usr/local" ]; then
    echo "Deps already unpacked — skipping."
elif [ -f "$DEPS_TARBALL" ]; then
    echo "Extracting prebuilt deps tarball: $DEPS_TARBALL"
    tar -xzf "$DEPS_TARBALL" -C BambuStudio
elif [ -f "BambuStudio_dep_mac_arm64.tar.gz" ]; then  # same, alternate location
    tar -xzf "BambuStudio_dep_mac_arm64.tar.gz" -C BambuStudio
else
    brew install nasm yasm x264
    brew uninstall --ignore-dependencies zstd || true
    ( cd BambuStudio && ./BuildMac.sh -d -x -a "$BUILD_ARCH" -t 10.15 -1 )
    brew install zstd || true
    ( cd "BambuStudio/deps/build/${BUILD_ARCH}" && \
      find . -mindepth 1 -maxdepth 1 ! -name 'BambuStudio_deps' -exec rm -rf {} + )
    tar -czf "$DEPS_TARBALL" -C BambuStudio deps/build
fi
ls "BambuStudio/deps/build/${BUILD_ARCH}/BambuStudio_deps/usr/local" | head -5
# Keep a tarball around for the workflow's cache/save step.
if [ ! -f "$DEPS_TARBALL" ] && [ -d BambuStudio/deps/build ]; then
    tar -czf "$DEPS_TARBALL" -C BambuStudio deps/build
fi

log "Stage 3: apply the ImageMap port (./apply.sh)"
if [ ! -e BambuStudio/src/libslic3r/ImageMapPerLayerColor.hpp ]; then
    ./apply.sh "$ROOT/BambuStudio" | tail -12
else
    echo "Port already applied — skipping."
fi
( cd BambuStudio && git status --short | head -15 )

log "Stage 4: build slicer (${BUILD_ARCH}) — this is the long one"
( cd BambuStudio && ./BuildMac.sh -s -x -a "$BUILD_ARCH" -t 10.15 -1 )

APP="BambuStudio/build/${BUILD_ARCH}/BambuStudio/BambuStudio.app"
[ -d "$APP" ] || { echo "ERROR: built app not found at $APP"; exit 1; }

log "Stage 5: ad-hoc codesign + pack DMG"
codesign --force --deep --sign - "$APP" \
    || echo "::warning::ad-hoc codesign failed, continuing unsigned"
codesign --verify --verbose "$APP" || true

VER=$(grep 'set(SLIC3R_VERSION' BambuStudio/version.inc | cut -d '"' -f2)
DMG="BambuStudio_${BUILD_ARCH}_V${VER}_${DMG_SUFFIX}.dmg"
rm -rf dmgroot "$DMG"
mkdir -p dmgroot
cp -R "$APP" dmgroot/
ln -s /Applications dmgroot/Applications
hdiutil create -volname "Bambu Studio" -srcfolder dmgroot -ov -format UDZO "$DMG"
shasum -a 256 "$DMG" > "${DMG}.sha256"
ls -lh "$DMG" "${DMG}.sha256"
cat "${DMG}.sha256"

log "Stage 6: publish GitHub Release '$RELEASE_TAG'"
cat > /tmp/release-notes.md <<'NOTES'
Bambu Studio v%VERSION% with the "one tool-change per layer" image/texture
color printing port enabled (Phase 1 of the OrcaSlicer-ImageMap feature
ported into Bambu Studio).

- Built from branch `%BRANCH%` commit `%SHA%`
- Upstream base: bambulab/BambuStudio@%BASESHA% (v%VERSION%)
- Target: macOS 10.15+ on **Apple Silicon** (M1/M2/M3/M4)

**Download:** the `.dmg` below (checksum in the `.sha256` file). Drag
BambuStudio to Applications.

> ⚠️ This is a fork build: the app is **not notarized / developer-signed**.
> On first launch Gatekeeper will complain — right-click the app → *Open* →
> *Open*, or run `xattr -cr /Applications/BambuStudio.app` first.

The feature ships **off by default**. Enable *"Image-map per-layer color
rotation"* in the print settings (develop mode), paint the model multi-color
the normal Bambu Studio way (3MF mmu-segmentation / per-part filament), and
slice.
NOTES
BRANCH="${GITHUB_REF_NAME:-$(git rev-parse --abbrev-ref HEAD)}"
sed -e "s/%VERSION%/$VER/g" \
    -e "s/%BRANCH%/$BRANCH/g" \
    -e "s/%SHA%/$TARGET_SHA/g" \
    -e "s/%BASESHA%/$BASE_SHA/g" \
    /tmp/release-notes.md > /tmp/release-notes-final.md

gh release view "$RELEASE_TAG" -R "$REPO" >/dev/null 2>&1 || \
    gh release create "$RELEASE_TAG" -R "$REPO" \
        --target "$TARGET_SHA" \
        --title "Bambu Studio ${VER} — ImageMap per-layer color port (macOS ${BUILD_ARCH})" \
        --notes-file /tmp/release-notes-final.md
gh release upload "$RELEASE_TAG" -R "$REPO" "$DMG" "${DMG}.sha256" --clobber

log "Done"
echo "Release: https://github.com/$REPO/releases/tag/$RELEASE_TAG"
