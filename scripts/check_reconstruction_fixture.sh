#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

FIXTURE_ZIP="${1:-}"
TOOTH_BACKEND="${TOOTH_BACKEND:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39010}"
TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-60}"
KEEP_WORK_DIR="${KEEP_WORK_DIR:-0}"

WORK_DIR=""
FIXTURE_ROOT=""
OUTPUT_DIR=""
BACKEND_PID=""
BACKEND_IN_FD=""
BACKEND_OUT_FD=""
STDERR_LOG=""
LAST_JSON=""

log() { printf '[INFO] %s\n' "$*"; }
ok() { printf '[OK] %s\n' "$*"; }
die() { printf '[ERROR] %s\n' "$*" >&2; [[ -n "${STDERR_LOG}" && -f "${STDERR_LOG}" ]] && { printf '%s\n' '---- backend stderr ----' >&2; tail -100 "${STDERR_LOG}" >&2; }; exit 1; }

cleanup() {
  if [[ -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null; then
    kill "${BACKEND_PID}" 2>/dev/null || true
    wait "${BACKEND_PID}" 2>/dev/null || true
  fi

  if [[ "${KEEP_WORK_DIR}" == "1" && -n "${WORK_DIR}" ]]; then
    printf '[INFO] keeping work directory: %s\n' "${WORK_DIR}"
  elif [[ -n "${WORK_DIR}" && -d "${WORK_DIR}" ]]; then
    rm -rf -- "${WORK_DIR}"
  fi
}
trap cleanup EXIT

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

read_json_line() {
  local deadline=$((SECONDS + TIMEOUT_SECONDS))
  local line

  while (( SECONDS < deadline )); do
    if IFS= read -r -t 1 -u "${BACKEND_OUT_FD}" line; then
      [[ -n "${line}" ]] || continue
      printf '< %s\n' "${line}"
      if jq -e . >/dev/null 2>&1 <<<"${line}"; then
        LAST_JSON="${line}"
        return 0
      fi
    elif ! kill -0 "${BACKEND_PID}" 2>/dev/null; then
      die "backend exited unexpectedly"
    fi
  done

  die "timeout waiting for backend JSON"
}

wait_for_json() {
  local jq_filter="$1"
  local description="$2"
  local deadline=$((SECONDS + TIMEOUT_SECONDS))

  while (( SECONDS < deadline )); do
    read_json_line
    if jq -e "${jq_filter}" >/dev/null <<<"${LAST_JSON}"; then
      ok "${description}"
      return 0
    fi
  done

  die "timeout waiting for ${description}; last JSON: ${LAST_JSON}"
}

send_request() {
  local json="$1"
  printf '> %s\n' "${json}"
  printf '%s\n' "${json}" >&"${BACKEND_IN_FD}"
}

request_ok() {
  local id="$1"
  local json="$2"
  send_request "${json}"
  wait_for_json ".id == \"${id}\" and .ok == true" "response id=${id}"
}

verify_fixture_checksum() {
  local fixture_checksum_file="${FIXTURE_ROOT}/SHA256SUMS"
  [[ -f "${fixture_checksum_file}" ]] || die "fixture SHA256SUMS not found"
  (cd "${FIXTURE_ROOT}" && sha256sum -c SHA256SUMS)
  ok "fixture checksums"
}

verify_manifest() {
  local manifest="${FIXTURE_ROOT}/manifest.json"
  [[ -f "${manifest}" ]] || die "manifest.json not found"

  for key in camera_width camera_height projector_width projector_height pattern_count decode_threshold; do
    jq -e ".${key} | numbers" "${manifest}" >/dev/null || die "manifest field missing or invalid: ${key}"
  done

  ok "manifest"
}

start_backend() {
  [[ -x "${TOOTH_BACKEND}" ]] || die "tooth-backend not executable: ${TOOTH_BACKEND}"

  STDERR_LOG="${WORK_DIR}/backend.stderr.log"
  coproc BACKEND_PROC {
    cd "${REPO_ROOT}"
    exec "${TOOTH_BACKEND}" serve \
      --control stdio \
      --mjpeg-host "${MJPEG_HOST}" \
      --mjpeg-port "${MJPEG_PORT}" \
      2>"${STDERR_LOG}"
  }

  BACKEND_PID="${BACKEND_PROC_PID}"
  BACKEND_OUT_FD="${BACKEND_PROC[0]}"
  BACKEND_IN_FD="${BACKEND_PROC[1]}"

  wait_for_json '.event == "ready"' 'backend ready'
}

verify_ply() {
  local ply="$1"
  local expected_count="$2"
  [[ -s "${ply}" ]] || die "PLY not found or empty: ${ply}"

  grep -q '^ply$' "${ply}" || die "invalid PLY header"
  grep -q '^format ascii 1.0$' "${ply}" || die "PLY is not ASCII 1.0"

  local header_count
  header_count="$(awk '$1=="element" && $2=="vertex" {print $3; exit}' "${ply}")"
  [[ "${header_count}" == "${expected_count}" ]] || die "PLY vertex count mismatch: header=${header_count}, response=${expected_count}"

  if grep -Eiq '(^|[[:space:]])(nan|inf|-inf)([[:space:]]|$)' "${ply}"; then
    die "PLY contains NaN/Inf"
  fi

  ok "PLY validation: ${expected_count} points"
}

main() {
  [[ -n "${FIXTURE_ZIP}" ]] || die "usage: $0 <fixture.zip>"
  [[ -f "${FIXTURE_ZIP}" ]] || die "fixture ZIP not found: ${FIXTURE_ZIP}"

  for cmd in unzip sha256sum jq awk grep find mktemp; do
    require_command "${cmd}"
  done

  WORK_DIR="$(mktemp -d)"
  unzip -q "${FIXTURE_ZIP}" -d "${WORK_DIR}/fixture"

  FIXTURE_ROOT="$(find "${WORK_DIR}/fixture" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
  [[ -n "${FIXTURE_ROOT}" ]] || die "fixture root directory not found"

  OUTPUT_DIR="${WORK_DIR}/output"
  mkdir -p \
    "${OUTPUT_DIR}/calibration" \
    "${OUTPUT_DIR}/decode" \
    "${OUTPUT_DIR}/reconstruction"

  verify_fixture_checksum
  verify_manifest

  local camera_width camera_height projector_width projector_height pattern_count threshold
  camera_width="$(jq -r '.camera_width' "${FIXTURE_ROOT}/manifest.json")"
  camera_height="$(jq -r '.camera_height' "${FIXTURE_ROOT}/manifest.json")"
  projector_width="$(jq -r '.projector_width' "${FIXTURE_ROOT}/manifest.json")"
  projector_height="$(jq -r '.projector_height' "${FIXTURE_ROOT}/manifest.json")"
  pattern_count="$(jq -r '.pattern_count' "${FIXTURE_ROOT}/manifest.json")"
  threshold="$(jq -r '.decode_threshold' "${FIXTURE_ROOT}/manifest.json")"

  start_backend

  request_ok ping '{"id":"ping","cmd":"ping"}'

  request_ok mono-left "$(jq -cn \
    --arg id mono-left \
    --arg image_folder "${FIXTURE_ROOT}/calibration/mono_L" \
    --arg output_file "${OUTPUT_DIR}/calibration/mono_left.yml" \
    '{id:$id,cmd:"mono_calibrate",image_folder:$image_folder,output_file:$output_file}')"

  request_ok mono-right "$(jq -cn \
    --arg id mono-right \
    --arg image_folder "${FIXTURE_ROOT}/calibration/mono_R" \
    --arg output_file "${OUTPUT_DIR}/calibration/mono_right.yml" \
    '{id:$id,cmd:"mono_calibrate",image_folder:$image_folder,output_file:$output_file}')"

  request_ok stereo "$(jq -cn \
    --arg id stereo \
    --arg left_dir "${FIXTURE_ROOT}/calibration/stereo_L" \
    --arg right_dir "${FIXTURE_ROOT}/calibration/stereo_R" \
    --arg left_file "${OUTPUT_DIR}/calibration/mono_left.yml" \
    --arg right_file "${OUTPUT_DIR}/calibration/mono_right.yml" \
    --arg output_file "${OUTPUT_DIR}/calibration/stereo.yml" \
    '{id:$id,cmd:"stereo_calibrate",left_dir:$left_dir,right_dir:$right_dir,left_calibration_file:$left_file,right_calibration_file:$right_file,output_file:$output_file}')"

  request_ok decode "$(jq -cn \
    --arg id decode \
    --arg input_dir "${FIXTURE_ROOT}/scan" \
    --arg output_dir "${OUTPUT_DIR}/decode" \
    --argjson projector_width "${projector_width}" \
    --argjson projector_height "${projector_height}" \
    --argjson pattern_count "${pattern_count}" \
    --argjson threshold "${threshold}" \
    '{id:$id,cmd:"decode_patterns",input_dir:$input_dir,output_dir:$output_dir,projector_width:$projector_width,projector_height:$projector_height,pattern_count:$pattern_count,threshold:$threshold,allow_partial:false}')"

  [[ "$(jq -r '.image_width' <<<"${LAST_JSON}")" == "${camera_width}" ]] || die "decode image_width mismatch"
  [[ "$(jq -r '.image_height' <<<"${LAST_JSON}")" == "${camera_height}" ]] || die "decode image_height mismatch"
  ok "decode dimensions: ${camera_width}x${camera_height}"

  request_ok validate "$(jq -cn \
    --arg id validate \
    --arg decode_dir "${OUTPUT_DIR}/decode" \
    --arg calibration_file "${OUTPUT_DIR}/calibration/stereo.yml" \
    '{id:$id,cmd:"reconstruct_validate",decode_dir:$decode_dir,calibration_file:$calibration_file}')"

  jq -e '.valid == "true" or .valid == true' >/dev/null <<<"${LAST_JSON}" || die "reconstruct validation returned valid=false: ${LAST_JSON}"
  jq -e '(.issue_count | tonumber) == 0' >/dev/null <<<"${LAST_JSON}" || die "reconstruct validation issues found: ${LAST_JSON}"
  local valid_correspondence_count
  valid_correspondence_count="$(jq -r '.valid_correspondence_count' <<<"${LAST_JSON}")"
  (( valid_correspondence_count > 0 )) || die "no valid correspondences"
  ok "reconstruction validation: ${valid_correspondence_count} correspondences"

  request_ok reconstruct "$(jq -cn \
    --arg id reconstruct \
    --arg decode_dir "${OUTPUT_DIR}/decode" \
    --arg calibration_file "${OUTPUT_DIR}/calibration/stereo.yml" \
    --arg output_file "${OUTPUT_DIR}/reconstruction/cloud.ply" \
    '{id:$id,cmd:"reconstruct_point_cloud",decode_dir:$decode_dir,calibration_file:$calibration_file,output_file:$output_file,overwrite:true}')"

  local point_count
  point_count="$(jq -r '.point_count' <<<"${LAST_JSON}")"
  (( point_count > 0 )) || die "point_count is zero"
  verify_ply "${OUTPUT_DIR}/reconstruction/cloud.ply" "${point_count}"

  request_ok shutdown '{"id":"shutdown","cmd":"shutdown"}'

  printf '\nPASS: reconstruction fixture\n'
  printf '  Camera:          %sx%s\n' "${camera_width}" "${camera_height}"
  printf '  Projector:       %sx%s\n' "${projector_width}" "${projector_height}"
  printf '  Correspondences: %s\n' "${valid_correspondence_count}"
  printf '  Points:          %s\n' "${point_count}"
  if [[ "${KEEP_WORK_DIR}" == "1" ]]; then
    printf '  Output:          %s\n' "${OUTPUT_DIR}"
  fi
}

main "$@"
