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
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}"

require_command() {
  local cmd="$1"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "${cmd} is required. Install it from your distro packages." >&2
    exit 1
  fi
}

require_command qmake
require_command make
require_command linuxdeployqt

rm -rf "${BUILD_ROOT}"
mkdir -p "${BUILD_DIR}" "${APPDIR}"

pushd "${BUILD_DIR}" >/dev/null
qmake CONFIG+=release "${ROOT_DIR}/Tagger.pro"
make -j"${JOBS}"
popd >/dev/null

install -Dm755 "${BUILD_DIR}/tagger" "${APPDIR}/usr/bin/tagger"
install -Dm644 "${ROOT_DIR}/packaging/flatpak/org.tagger.Tagger.desktop" "${DESKTOP_FILE}"
install -Dm644 "${ROOT_DIR}/packaging/flatpak/org.tagger.Tagger.svg" "${ICON_FILE}"
install -Dm644 "${ROOT_DIR}/packaging/flatpak/org.tagger.Tagger.metainfo.xml" "${METAINFO_FILE}"

LINUXDEPLOYQT_HELP="$(linuxdeployqt -h 2>&1 || true)"
if command -v appimagetool >/dev/null 2>&1; then
  linuxdeployqt "${DESKTOP_FILE}" -bundle-non-qt-libs
  find "${APPDIR}" -path "*/plugins/platforms/*wayland*.so" -delete
  find "${APPDIR}" -path "*/plugins/wayland*" -delete
  appimagetool "${APPDIR}" "${OUTPUT_PATH}"
else
  if grep -q "exclude-plugins" <<<"${LINUXDEPLOYQT_HELP}"; then
    linuxdeployqt "${DESKTOP_FILE}" -bundle-non-qt-libs -exclude-plugins=wayland -appimage
    mv "${BUILD_ROOT}"/*.AppImage "${OUTPUT_PATH}"
  else
    echo "appimagetool is required to strip Wayland plugins before packaging." >&2
    echo "Install appimagetool or a newer linuxdeployqt that supports -exclude-plugins=wayland." >&2
    exit 1
  fi
fi

echo "AppImage created at ${OUTPUT_PATH}"
