#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

FIXTURE_ZIP="${1:-}"
OUTPUT_DIR="${2:-${REPO_ROOT}/data/reconstruction/latest}"
TOOTH_BACKEND="${TOOTH_BACKEND:-${REPO_ROOT}/build/arch-dev/src/serve/tooth-backend}"

log() {
  printf '[INFO] %s\n' "$*"
}

die() {
  printf '[ERROR] %s\n' "$*" >&2
  exit 1
}

[[ -n "${FIXTURE_ZIP}" ]] ||
  die "usage: $0 <fixture.zip> [output-directory]"

[[ -f "${FIXTURE_ZIP}" ]] ||
  die "fixture ZIP not found: ${FIXTURE_ZIP}"

[[ -x "${TOOTH_BACKEND}" ]] ||
  die "tooth-backend not executable: ${TOOTH_BACKEND}"

LOG_FILE="$(mktemp)"
TEMP_OUTPUT=""

cleanup() {
  rm -f -- "${LOG_FILE}"

  if [[ "${TEMP_OUTPUT}" == /tmp/*/output ]]; then
    rm -rf -- "$(dirname -- "${TEMP_OUTPUT}")"
  fi
}

trap cleanup EXIT

log "running reconstruction pipeline"

env \
  KEEP_WORK_DIR=1 \
  TOOTH_BACKEND="${TOOTH_BACKEND}" \
  "${SCRIPT_DIR}/check_reconstruction_fixture.sh" \
  "${FIXTURE_ZIP}" |
  tee "${LOG_FILE}"

TEMP_OUTPUT="$(
  sed -n \
    's/^[[:space:]]*Output:[[:space:]]*//p' \
    "${LOG_FILE}" |
  tail -n 1
)"

[[ -n "${TEMP_OUTPUT}" ]] ||
  die "temporary output directory was not reported"

[[ -f "${TEMP_OUTPUT}/reconstruction/cloud.ply" ]] ||
  die "cloud.ply was not generated"

rm -rf -- "${OUTPUT_DIR}"
mkdir -p -- "${OUTPUT_DIR}"

cp -a \
  "${TEMP_OUTPUT}/." \
  "${OUTPUT_DIR}/"

printf '\n'
printf '[OK] reconstruction completed\n'
printf '  Output directory: %s\n' "${OUTPUT_DIR}"
printf '  Point cloud:      %s\n' \
  "${OUTPUT_DIR}/reconstruction/cloud.ply"