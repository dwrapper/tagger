#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="${ROOT_DIR}/packaging/flatpak/org.tagger.Tagger.yml"
BUILD_DIR="${ROOT_DIR}/build/flatpak"
REPO_DIR="${ROOT_DIR}/build/flatpak-repo"
BUNDLE_PATH="${ROOT_DIR}/build/tagger.flatpak"

if ! command -v flatpak-builder >/dev/null 2>&1; then
  echo "flatpak-builder is required. Install it from your distro packages." >&2
  exit 1
fi

if ! command -v flatpak >/dev/null 2>&1; then
  echo "flatpak is required. Install it from your distro packages." >&2
  exit 1
fi

flatpak-builder \
  --force-clean \
  --install-deps-from=flathub \
  --repo="${REPO_DIR}" \
  "${BUILD_DIR}" \
  "${MANIFEST}"

flatpak build-bundle \
  "${REPO_DIR}" \
  "${BUNDLE_PATH}" \
  org.tagger.Tagger \
  --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo

echo "Flatpak bundle created at ${BUNDLE_PATH}"
