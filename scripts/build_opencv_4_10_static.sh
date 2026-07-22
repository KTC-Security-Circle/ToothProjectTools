#!/usr/bin/env bash
set -Eeuo pipefail

OPENCV_VERSION="${OPENCV_VERSION:-4.10.0}"
OPENCV_WORK_DIR="${OPENCV_WORK_DIR:-/tmp/opencv-${OPENCV_VERSION}-work}"
OPENCV_INSTALL_PREFIX="${OPENCV_INSTALL_PREFIX:-${HOME}/.local/opencv/${OPENCV_VERSION}-static}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"
CLEAN_BUILD="${CLEAN_BUILD:-0}"

OPENCV_SRC="${OPENCV_WORK_DIR}/opencv"
OPENCV_CONTRIB_SRC="${OPENCV_WORK_DIR}/opencv_contrib"
BUILD_DIR="${OPENCV_WORK_DIR}/build"

log() { printf '[INFO] %s\n' "$*"; }
ok() { printf '[OK] %s\n' "$*"; }
die() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

clone_or_verify_repo() {
  local url="$1"
  local path="$2"

  if [[ -d "${path}/.git" ]]; then
    log "using existing repository: ${path}"
    git -C "${path}" fetch --tags --depth 1 origin "${OPENCV_VERSION}" >/dev/null 2>&1 || true
    git -C "${path}" checkout --detach "${OPENCV_VERSION}" >/dev/null 2>&1 || \
      git -C "${path}" checkout --detach "tags/${OPENCV_VERSION}" >/dev/null 2>&1 || \
      die "failed to checkout ${OPENCV_VERSION}: ${path}"
  else
    rm -rf -- "${path}"
    git clone --branch "${OPENCV_VERSION}" --depth 1 "${url}" "${path}"
  fi
}

verify_gtk() {
  pkg-config --exists gtk+-3.0 || die "gtk+-3.0 development package not found; install libgtk-3-dev and pkg-config"
  ok "GTK3 detected: $(pkg-config --modversion gtk+-3.0)"
}

configure() {
  if [[ "${CLEAN_BUILD}" == "1" ]]; then
    rm -rf -- "${BUILD_DIR}"
  fi

  mkdir -p -- "${BUILD_DIR}" "${OPENCV_INSTALL_PREFIX}"

  cmake \
    -S "${OPENCV_SRC}" \
    -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${OPENCV_INSTALL_PREFIX}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DOPENCV_EXTRA_MODULES_PATH="${OPENCV_CONTRIB_SRC}/modules" \
    -DBUILD_LIST=core,imgproc,highgui,videoio,imgcodecs,calib3d,features2d,flann,structured_light,phase_unwrapping \
    -DWITH_GTK=ON \
    -DWITH_QT=OFF \
    -DWITH_FFMPEG=OFF \
    -DWITH_GSTREAMER=OFF \
    -DWITH_V4L=ON \
    -DBUILD_opencv_gapi=OFF \
    -DBUILD_opencv_dnn=OFF \
    -DBUILD_opencv_dnn_objdetect=OFF \
    -DBUILD_opencv_dnn_superres=OFF \
    -DWITH_PROTOBUF=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_opencv_apps=OFF \
    -DBUILD_JAVA=OFF \
    -DBUILD_opencv_python3=OFF
}

verify_configuration() {
  grep -q '^WITH_GTK:BOOL=ON$' "${BUILD_DIR}/CMakeCache.txt" || die "WITH_GTK is not enabled"

  if [[ -f "${BUILD_DIR}/cvconfig.h" ]]; then
    grep -q 'define HAVE_GTK' "${BUILD_DIR}/cvconfig.h" || die "OpenCV was configured without GTK support"
  fi

  ok "OpenCV configured with GTK support"
}

build_and_install() {
  cmake --build "${BUILD_DIR}" --target ade --parallel "${JOBS}"
  cmake --build "${BUILD_DIR}" --parallel "${JOBS}"
  cmake --install "${BUILD_DIR}"
}

verify_install() {
  local config_file="${OPENCV_INSTALL_PREFIX}/lib/cmake/opencv4/OpenCVConfig.cmake"
  local ade_file="${OPENCV_INSTALL_PREFIX}/lib/opencv4/3rdparty/libade.a"

  [[ -f "${config_file}" ]] || die "OpenCVConfig.cmake not found: ${config_file}"
  [[ -f "${ade_file}" ]] || die "libade.a not found: ${ade_file}"

  ok "OpenCVConfig.cmake: ${config_file}"
  ok "libade.a: ${ade_file}"
}

main() {
  for cmd in git cmake ninja pkg-config grep nproc; do
    require_command "${cmd}"
  done

  verify_gtk
  mkdir -p -- "${OPENCV_WORK_DIR}"
  clone_or_verify_repo https://github.com/opencv/opencv.git "${OPENCV_SRC}"
  clone_or_verify_repo https://github.com/opencv/opencv_contrib.git "${OPENCV_CONTRIB_SRC}"
  configure
  verify_configuration
  build_and_install
  verify_install

  printf '\nOpenCV static build completed.\n'
  printf '  Version:       %s\n' "${OPENCV_VERSION}"
  printf '  Install prefix:%s\n' "${OPENCV_INSTALL_PREFIX}"
  printf '  OpenCV_DIR:    %s\n' "${OPENCV_INSTALL_PREFIX}/lib/cmake/opencv4"
}

main "$@"
