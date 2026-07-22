#!/usr/bin/env bash
set -Eeuo pipefail

cat <<'EOF'

============================================================
ToothProjectTools development environment
============================================================

初回セットアップまたはrebuild後:

  docs/development/README.md
  docs/development/opencv-4.10-static-build.md

カメラ・stream・scanの確認:

  docs/development/camera-stream-scan-smoke-test.md

Calibrationから点群再構成までの確認:

  docs/development/reconstruction-fixture.md

Quick start:

  cd /workspace

  ./scripts/build_opencv_4_10_static.sh

  cmake --preset release-opencv-4.10-static
  cmake --build --preset release-opencv-4.10-static --parallel

============================================================

EOF
