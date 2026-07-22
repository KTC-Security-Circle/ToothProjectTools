#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

FIXTURE_VERSION="${FIXTURE_VERSION:-v1}"
FIXTURE_NAME="${FIXTURE_NAME:-tooth-reconstruction-fixture-${FIXTURE_VERSION}}"

CAMERA_WIDTH="${CAMERA_WIDTH:-1024}"
CAMERA_HEIGHT="${CAMERA_HEIGHT:-768}"
PROJECTOR_WIDTH="${PROJECTOR_WIDTH:-1920}"
PROJECTOR_HEIGHT="${PROJECTOR_HEIGHT:-1080}"
PATTERN_COUNT="${PATTERN_COUNT:-46}"
DECODE_THRESHOLD="${DECODE_THRESHOLD:-1}"

MONO_LEFT_DIR="${MONO_LEFT_DIR:-${REPO_ROOT}/capture_test/mono_L}"
MONO_RIGHT_DIR="${MONO_RIGHT_DIR:-${REPO_ROOT}/capture_test/mono_R}"
STEREO_LEFT_DIR="${STEREO_LEFT_DIR:-${REPO_ROOT}/capture_test/stereo_L}"
STEREO_RIGHT_DIR="${STEREO_RIGHT_DIR:-${REPO_ROOT}/capture_test/stereo_R}"
SCAN_LEFT_DIR="${SCAN_LEFT_DIR:-${REPO_ROOT}/data/scan/imported/left}"
SCAN_RIGHT_DIR="${SCAN_RIGHT_DIR:-${REPO_ROOT}/data/scan/imported/right}"

WORK_DIR=""
STAGING_ROOT=""
ARCHIVE_PATH=""
ARCHIVE_CHECKSUM_PATH=""

log() { printf '[INFO] %s\n' "$*"; }
ok() { printf '[OK] %s\n' "$*"; }
die() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<EOF
Usage:
  $0 OUTPUT_ARCHIVE
  OUTPUT_ARCHIVE=/path/to/${FIXTURE_NAME}.zip $0

Example:
  $0 "$HOME/Downloads/${FIXTURE_NAME}.zip"
EOF
}

resolve_archive_path() {
  local requested_path="${1:-${OUTPUT_ARCHIVE:-}}"
  local output_dir
  local output_name

  [[ -n "${requested_path}" ]] || {
    usage >&2
    die "OUTPUT_ARCHIVE is required"
  }

  [[ "${requested_path}" == *.zip ]] ||
    die "output archive must have a .zip extension: ${requested_path}"

  output_dir="$(dirname -- "${requested_path}")"
  output_name="$(basename -- "${requested_path}")"

  mkdir -p -- "${output_dir}"
  output_dir="$(cd -- "${output_dir}" && pwd)"

  ARCHIVE_PATH="${output_dir}/${output_name}"
  ARCHIVE_CHECKSUM_PATH="${ARCHIVE_PATH}.sha256"
}

cleanup() {
  if [[ -n "${WORK_DIR}" && -d "${WORK_DIR}" ]]; then
    rm -rf -- "${WORK_DIR}"
  fi
}
trap cleanup EXIT

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

require_positive_integer() {
  local name="$1"
  local value="$2"
  [[ "${value}" =~ ^[1-9][0-9]*$ ]] || die "${name} must be a positive integer: ${value}"
}

collect_images() {
  local directory="$1"
  find "${directory}" -maxdepth 1 -type f \
    \( -iname '*.png' -o -iname '*.jpg' -o -iname '*.jpeg' \) \
    -print0
}

image_count() {
  local directory="$1"
  collect_images "${directory}" | tr -cd '\0' | wc -c
}

validate_directory() {
  local label="$1"
  local directory="$2"
  [[ -d "${directory}" ]] || die "${label} directory not found: ${directory}"

  local count
  count="$(image_count "${directory}")"
  (( count > 0 )) || die "${label} contains no supported images: ${directory}"
  ok "${label}: ${count} images"
}

extract_image_size() {
  local file_path="$1"
  local description
  description="$(file -b -- "${file_path}")"

  if [[ "${description}" =~ ([0-9]+)[[:space:]]x[[:space:]]([0-9]+) ]]; then
    printf '%sx%s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
    return 0
  fi

  die "could not determine image size: ${file_path}: ${description}"
}

validate_image_sizes() {
  local label="$1"
  local directory="$2"
  local expected="${CAMERA_WIDTH}x${CAMERA_HEIGHT}"
  local file_path actual

  while IFS= read -r -d '' file_path; do
    actual="$(extract_image_size "${file_path}")"
    [[ "${actual}" == "${expected}" ]] || \
      die "${label} image size mismatch: ${file_path}: expected=${expected}, actual=${actual}"
  done < <(collect_images "${directory}")

  ok "${label} image size: ${expected}"
}

write_names() {
  local directory="$1"
  local output_file="$2"
  find "${directory}" -maxdepth 1 -type f \
    \( -iname '*.png' -o -iname '*.jpg' -o -iname '*.jpeg' \) \
    -printf '%f\n' | LC_ALL=C sort > "${output_file}"
}

validate_matching_names() {
  local label="$1"
  local left_dir="$2"
  local right_dir="$3"
  local left_names="${WORK_DIR}/${label}-left-names.txt"
  local right_names="${WORK_DIR}/${label}-right-names.txt"
  local difference="${WORK_DIR}/${label}-difference.txt"

  write_names "${left_dir}" "${left_names}"
  write_names "${right_dir}" "${right_names}"
  comm -3 "${left_names}" "${right_names}" > "${difference}"

  if [[ -s "${difference}" ]]; then
    printf '[ERROR] %s filename mismatch:\n' "${label}" >&2
    cat "${difference}" >&2
    exit 1
  fi

  ok "${label} left/right filenames match"
}

validate_scan_names() {
  local directory="$1"
  local file_path filename

  while IFS= read -r -d '' file_path; do
    filename="$(basename -- "${file_path}")"
    [[ "${filename}" =~ ^pattern_[0-9]{3}\.png$ ]] || \
      die "invalid scan filename: ${file_path}; expected pattern_NNN.png"
  done < <(collect_images "${directory}")
}

copy_directory_contents() {
  local source_dir="$1"
  local destination_dir="$2"
  mkdir -p -- "${destination_dir}"
  cp -a -- "${source_dir}/." "${destination_dir}/"
}

write_fixture_readme() {
  cat > "${STAGING_ROOT}/README.md" <<EOT
# Tooth reconstruction fixture ${FIXTURE_VERSION}

This fixture contains calibration and GrayCode scan input images for
ToothProjectTools reconstruction testing.

## Parameters

- Camera image: ${CAMERA_WIDTH}x${CAMERA_HEIGHT}
- Projector: ${PROJECTOR_WIDTH}x${PROJECTOR_HEIGHT}
- Pattern count: ${PATTERN_COUNT}
- Decode threshold: ${DECODE_THRESHOLD}
- Partial dataset: false

Generated calibration YAML, decoded projector maps, masks, and PLY files are not
included. They must be regenerated by the receiving environment.

Before sharing this fixture, confirm that the images do not contain private or
sensitive visual information.
EOT
}

write_manifest() {
  cat > "${STAGING_ROOT}/manifest.json" <<EOT
{
  "fixture_version": "${FIXTURE_VERSION}",
  "camera_width": ${CAMERA_WIDTH},
  "camera_height": ${CAMERA_HEIGHT},
  "projector_width": ${PROJECTOR_WIDTH},
  "projector_height": ${PROJECTOR_HEIGHT},
  "pattern_count": ${PATTERN_COUNT},
  "decode_threshold": ${DECODE_THRESHOLD},
  "allow_partial": false,
  "paths": {
    "mono_left": "calibration/mono_L",
    "mono_right": "calibration/mono_R",
    "stereo_left": "calibration/stereo_L",
    "stereo_right": "calibration/stereo_R",
    "scan_left": "scan/left",
    "scan_right": "scan/right"
  }
}
EOT
}

write_fixture_checksums() {
  (
    cd "${STAGING_ROOT}"
    find . -type f ! -name 'SHA256SUMS' -print0 | \
      LC_ALL=C sort -z | \
      xargs -0 sha256sum > SHA256SUMS
  )
  ok "fixture file checksums generated"
}

create_archive() {
  local archive_directory
  local archive_filename
  local checksum_filename

  archive_directory="$(dirname -- "${ARCHIVE_PATH}")"
  archive_filename="$(basename -- "${ARCHIVE_PATH}")"
  checksum_filename="$(basename -- "${ARCHIVE_CHECKSUM_PATH}")"

  rm -f -- "${ARCHIVE_PATH}" "${ARCHIVE_CHECKSUM_PATH}"

  (
    cd "${WORK_DIR}"
    zip -q -r "${ARCHIVE_PATH}" "${FIXTURE_NAME}"
  )

  (
    cd "${archive_directory}"
    sha256sum "${archive_filename}" > "${checksum_filename}"
  )

  ok "archive created: ${ARCHIVE_PATH}"
  ok "archive checksum: ${ARCHIVE_CHECKSUM_PATH}"
}

main() {
  if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
  fi

  (( $# <= 1 )) || {
    usage >&2
    die "too many arguments"
  }

  for command_name in find file sha256sum zip cp comm sort xargs tr; do
    require_command "${command_name}"
  done

  require_positive_integer CAMERA_WIDTH "${CAMERA_WIDTH}"
  require_positive_integer CAMERA_HEIGHT "${CAMERA_HEIGHT}"
  require_positive_integer PROJECTOR_WIDTH "${PROJECTOR_WIDTH}"
  require_positive_integer PROJECTOR_HEIGHT "${PROJECTOR_HEIGHT}"
  require_positive_integer PATTERN_COUNT "${PATTERN_COUNT}"
  require_positive_integer DECODE_THRESHOLD "${DECODE_THRESHOLD}"

  resolve_archive_path "${1:-}"

  WORK_DIR="$(mktemp -d)"
  STAGING_ROOT="${WORK_DIR}/${FIXTURE_NAME}"

  validate_directory "mono left" "${MONO_LEFT_DIR}"
  validate_directory "mono right" "${MONO_RIGHT_DIR}"
  validate_directory "stereo left" "${STEREO_LEFT_DIR}"
  validate_directory "stereo right" "${STEREO_RIGHT_DIR}"
  validate_directory "scan left" "${SCAN_LEFT_DIR}"
  validate_directory "scan right" "${SCAN_RIGHT_DIR}"

  validate_image_sizes "mono left" "${MONO_LEFT_DIR}"
  validate_image_sizes "mono right" "${MONO_RIGHT_DIR}"
  validate_image_sizes "stereo left" "${STEREO_LEFT_DIR}"
  validate_image_sizes "stereo right" "${STEREO_RIGHT_DIR}"
  validate_image_sizes "scan left" "${SCAN_LEFT_DIR}"
  validate_image_sizes "scan right" "${SCAN_RIGHT_DIR}"

  validate_matching_names "stereo" "${STEREO_LEFT_DIR}" "${STEREO_RIGHT_DIR}"
  validate_matching_names "scan" "${SCAN_LEFT_DIR}" "${SCAN_RIGHT_DIR}"

  validate_scan_names "${SCAN_LEFT_DIR}"
  validate_scan_names "${SCAN_RIGHT_DIR}"

  local scan_left_count scan_right_count
  scan_left_count="$(image_count "${SCAN_LEFT_DIR}")"
  scan_right_count="$(image_count "${SCAN_RIGHT_DIR}")"

  [[ "${scan_left_count}" -eq "${PATTERN_COUNT}" ]] || \
    die "scan left count mismatch: expected=${PATTERN_COUNT}, actual=${scan_left_count}"
  [[ "${scan_right_count}" -eq "${PATTERN_COUNT}" ]] || \
    die "scan right count mismatch: expected=${PATTERN_COUNT}, actual=${scan_right_count}"

  mkdir -p -- "${STAGING_ROOT}/calibration" "${STAGING_ROOT}/scan"

  copy_directory_contents "${MONO_LEFT_DIR}" "${STAGING_ROOT}/calibration/mono_L"
  copy_directory_contents "${MONO_RIGHT_DIR}" "${STAGING_ROOT}/calibration/mono_R"
  copy_directory_contents "${STEREO_LEFT_DIR}" "${STAGING_ROOT}/calibration/stereo_L"
  copy_directory_contents "${STEREO_RIGHT_DIR}" "${STAGING_ROOT}/calibration/stereo_R"
  copy_directory_contents "${SCAN_LEFT_DIR}" "${STAGING_ROOT}/scan/left"
  copy_directory_contents "${SCAN_RIGHT_DIR}" "${STAGING_ROOT}/scan/right"

  write_fixture_readme
  write_manifest
  write_fixture_checksums
  create_archive

  printf '\nFixture package completed.\n'
  printf '  ZIP:      %s\n' "${ARCHIVE_PATH}"
  printf '  SHA-256:  %s\n' "${ARCHIVE_CHECKSUM_PATH}"
}

main "$@"
