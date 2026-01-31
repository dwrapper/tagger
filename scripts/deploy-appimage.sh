#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${ROOT_DIR}/build/appimage"
BUILD_DIR="${BUILD_ROOT}/build"
APPDIR="${BUILD_ROOT}/AppDir"
DESKTOP_FILE="${APPDIR}/usr/share/applications/org.tagger.Tagger.desktop"
ICON_FILE="${APPDIR}/usr/share/icons/hicolor/scalable/apps/org.tagger.Tagger.svg"
METAINFO_FILE="${APPDIR}/usr/share/metainfo/org.tagger.Tagger.metainfo.xml"
OUTPUT_NAME="Tagger-$(uname -m).AppImage"
OUTPUT_PATH="${BUILD_ROOT}/${OUTPUT_NAME}"
RECIPE_TEMPLATE="${ROOT_DIR}/packaging/appimage-builder.yml"
RECIPE_PATH="${BUILD_ROOT}/appimage-builder.yml"
VERSION="${VERSION:-$(git -C "${ROOT_DIR}" describe --tags --always --dirty 2>/dev/null || echo 0.0.0)}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}"
LIBMPV_PKG="${LIBMPV_PKG:-libmpv1}"

require_command() {
  local cmd="$1"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "${cmd} is required. Install it from your distro packages." >&2
    exit 1
  fi
}

require_command qmake
require_command make
require_command appimage-builder

rm -rf "${BUILD_ROOT}"
mkdir -p "${BUILD_DIR}" "${APPDIR}"

pushd "${BUILD_DIR}" >/dev/null
qmake CONFIG+=release "${ROOT_DIR}/Tagger.pro"
make -j"${JOBS}"
popd >/dev/null

install -Dm755 "${BUILD_DIR}/Tagger" "${APPDIR}/usr/bin/Tagger"
install -Dm644 "${ROOT_DIR}/packaging/flatpak/org.tagger.Tagger.desktop" "${DESKTOP_FILE}"
install -Dm644 "${ROOT_DIR}/packaging/flatpak/org.tagger.Tagger.svg" "${ICON_FILE}"
install -Dm644 "${ROOT_DIR}/packaging/flatpak/org.tagger.Tagger.metainfo.xml" "${METAINFO_FILE}"

sed \
  -e "s|@VERSION@|${VERSION}|g" \
  -e "s|@ARCH@|$(uname -m)|g" \
  -e "s|@OUTPUT_NAME@|${OUTPUT_NAME}|g" \
  -e "s|@LIBMPV_PKG@|${LIBMPV_PKG}|g" \
  "${RECIPE_TEMPLATE}" > "${RECIPE_PATH}"

pushd "${BUILD_ROOT}" >/dev/null
appimage-builder --recipe "${RECIPE_PATH}"
popd >/dev/null

if [[ ! -f "${OUTPUT_PATH}" ]]; then
  echo "AppImage was not created at ${OUTPUT_PATH}" >&2
  exit 1
fi

echo "AppImage created at ${OUTPUT_PATH}"
