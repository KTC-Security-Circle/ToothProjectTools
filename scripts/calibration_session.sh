#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

BIN="${BIN:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
LEFT_CAMERA="${LEFT_CAMERA:-0}"
RIGHT_CAMERA="${RIGHT_CAMERA:-2}"
OUT_DIR="${OUT_DIR:-${REPO_ROOT}/data/calib}"
BOARD_X="${BOARD_X:-10}"
BOARD_Y="${BOARD_Y:-7}"
SQUARE_MM="${SQUARE_MM:-10.0}"
MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39010}"
RESPONSE_TIMEOUT="${RESPONSE_TIMEOUT:-30}"

WORK_DIR=""
BACKEND_LOG=""
BACKEND_PID=""
BACKEND_IN=""
BACKEND_OUT=""
LAST_JSON=""
REQUEST_SEQUENCE=0
STEREO_PREVIEW_READY=false
CLEANING_UP=false
LEFT_STREAM_URL=""
RIGHT_STREAM_URL=""

die() {
  printf '[ERROR] %s\n' "$*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

positive_integer() {
  [[ "$1" =~ ^[0-9]+$ ]] && (( 10#$1 > 0 ))
}

nonnegative_integer() {
  [[ "$1" =~ ^[0-9]+$ ]]
}

positive_number() {
  awk -v value="$1" 'BEGIN { exit !(value ~ /^[0-9]+([.][0-9]+)?$/ && value + 0 > 0) }'
}

send_cleanup_commands() {
  if [[ -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null && [[ -n "${BACKEND_IN}" ]]; then
    printf '%s\n' '{"id":"cleanup-stop-left","cmd":"stop_stream","role":"left"}' >&"${BACKEND_IN}" 2>/dev/null || true
    printf '%s\n' '{"id":"cleanup-stop-right","cmd":"stop_stream","role":"right"}' >&"${BACKEND_IN}" 2>/dev/null || true
    printf '%s\n' '{"id":"session-shutdown","cmd":"shutdown"}' >&"${BACKEND_IN}" 2>/dev/null || true
  fi
}

cleanup() {
  [[ "${CLEANING_UP}" == false ]] || return
  CLEANING_UP=true
  trap - EXIT INT TERM

  send_cleanup_commands
  if [[ -n "${BACKEND_IN}" ]]; then
    exec {BACKEND_IN}>&- 2>/dev/null || true
  fi
  if [[ -n "${BACKEND_OUT}" ]]; then
    exec {BACKEND_OUT}<&- 2>/dev/null || true
  fi
  if [[ -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null; then
    for _ in {1..20}; do
      kill -0 "${BACKEND_PID}" 2>/dev/null || break
      sleep 0.05
    done
    if kill -0 "${BACKEND_PID}" 2>/dev/null; then
      kill "${BACKEND_PID}" 2>/dev/null || true
    fi
  fi
  if [[ -n "${BACKEND_PID}" ]]; then
    wait "${BACKEND_PID}" 2>/dev/null || true
  fi
  if [[ -n "${WORK_DIR}" && -d "${WORK_DIR}" ]]; then
    rm -rf -- "${WORK_DIR}"
  fi
}

trap cleanup EXIT
trap 'exit 130' INT TERM

wait_for_json() {
  local filter="$1"
  local label="$2"
  local deadline=$((SECONDS + RESPONSE_TIMEOUT))
  local line remain

  while (( SECONDS < deadline )); do
    kill -0 "${BACKEND_PID}" 2>/dev/null || die "backend terminated while waiting for ${label}; log: ${BACKEND_LOG}"
    remain=$((deadline - SECONDS))
    if ! IFS= read -r -t "${remain}" line <&"${BACKEND_OUT}"; then
      continue
    fi
    [[ -n "${line}" ]] || continue
    if ! jq -e . >/dev/null 2>&1 <<<"${line}"; then
      printf '[WARN] backend non-JSON output: %s\n' "${line}" >&2
      continue
    fi
    if jq -e "${filter}" >/dev/null 2>&1 <<<"${line}"; then
      LAST_JSON="${line}"
      return 0
    fi
  done
  die "timeout waiting for ${label}; log: ${BACKEND_LOG}"
}

new_request_id() {
  REQUEST_SEQUENCE=$((REQUEST_SEQUENCE + 1))
  printf -v REQUEST_ID 'calibration-session-%04d' "${REQUEST_SEQUENCE}"
}

request() {
  local id="$1"
  local json="$2"
  printf '%s\n' "${json}" >&"${BACKEND_IN}"
  wait_for_json ".id == \"${id}\"" "response id=${id}"
  if jq -e '.ok == true' >/dev/null <<<"${LAST_JSON}"; then
    return 0
  fi
  printf '[ERROR] %s: %s: %s\n' "${id}" \
    "$(jq -r '.error.code // "command_failed"' <<<"${LAST_JSON}")" \
    "$(jq -r '.error.message // "unknown error"' <<<"${LAST_JSON}")" >&2
  return 1
}

board_args() {
  jq -cn --argjson x "${BOARD_X}" --argjson y "${BOARD_Y}" --argjson square "${SQUARE_MM}" \
    '{board_corners_x:$x,board_corners_y:$y,square_size_mm:$square}'
}

next_image_number() {
  local directory="$1"
  local max=0 path name number
  shopt -s nullglob
  for path in "${directory}"/*.png; do
    name="${path##*/}"
    if [[ "${name}" =~ ^([0-9]+)[.]png$ ]]; then
      number=$((10#${BASH_REMATCH[1]}))
      (( number > max )) && max="${number}"
    fi
  done
  shopt -u nullglob
  printf '%d' "$((max + 1))"
}

next_stereo_number() {
  local left_next right_next
  left_next="$(next_image_number "${OUT_DIR}/stereo/left")"
  right_next="$(next_image_number "${OUT_DIR}/stereo/right")"
  (( left_next > right_next )) && printf '%d' "${left_next}" || printf '%d' "${right_next}"
}

count_images() {
  local directory="$1"
  local count=0 path name
  shopt -s nullglob
  for path in "${directory}"/*.png; do
    name="${path##*/}"
    [[ "${name}" =~ ^[0-9]+[.]png$ ]] && count=$((count + 1))
  done
  shopt -u nullglob
  printf '%d' "${count}"
}

count_stereo_pairs() {
  local count=0 left name
  shopt -s nullglob
  for left in "${OUT_DIR}/stereo/left"/*.png; do
    name="${left##*/}"
    if [[ "${name}" =~ ^[0-9]+[.]png$ && -f "${OUT_DIR}/stereo/right/${name}" ]]; then
      count=$((count + 1))
    fi
  done
  shopt -u nullglob
  printf '%d' "${count}"
}

validate_stereo_pairs() {
  local path name
  local -a missing_left=()
  local -a missing_right=()
  shopt -s nullglob
  for path in "${OUT_DIR}/stereo/left"/*.png; do
    name="${path##*/}"
    if [[ "${name}" =~ ^[0-9]+[.]png$ && ! -f "${OUT_DIR}/stereo/right/${name}" ]]; then
      missing_right+=("${name}")
    fi
  done
  for path in "${OUT_DIR}/stereo/right"/*.png; do
    name="${path##*/}"
    if [[ "${name}" =~ ^[0-9]+[.]png$ && ! -f "${OUT_DIR}/stereo/left/${name}" ]]; then
      missing_left+=("${name}")
    fi
  done
  shopt -u nullglob

  if (( ${#missing_left[@]} > 0 )); then
    printf '[WARN] stereo pair mismatch: left is missing: %s\n' "${missing_left[*]}" >&2
  fi
  if (( ${#missing_right[@]} > 0 )); then
    printf '[WARN] stereo pair mismatch: right is missing: %s\n' "${missing_right[*]}" >&2
  fi
  (( ${#missing_left[@]} == 0 && ${#missing_right[@]} == 0 ))
}

show_counts() {
  printf '\nDataset:\n'
  printf '  left mono   : %s\n' "$(count_images "${OUT_DIR}/mono_left")"
  printf '  right mono  : %s\n' "$(count_images "${OUT_DIR}/mono_right")"
  printf '  stereo pairs: %s\n' "$(count_stereo_pairs)"
  validate_stereo_pairs || true
  printf '\n'
}

detect_mono_corners() {
  local role="$1" id json config found corner_count expected_count
  new_request_id
  id="${REQUEST_ID}"
  config="$(board_args)"
  json="$(jq -cn --arg id "${id}" --arg role "${role}" \
    --arg output "${OUT_DIR}/preview/${role}.png" --argjson board "${config}" \
    '{id:$id,cmd:"calib_detect_corners",role:$role,output:$output} + $board')"
  if ! request "${id}" "${json}"; then
    printf '[SKIP] mono image was not saved\n' >&2
    return 1
  fi

  found="$(jq -r '.found' <<<"${LAST_JSON}")"
  corner_count="$(jq -r '.corner_count' <<<"${LAST_JSON}")"
  expected_count="$(jq -r '.expected_corner_count' <<<"${LAST_JSON}")"
  if [[ "${found}" != true || "${corner_count}" != "${expected_count}" ]]; then
    printf '[WARN] %s corners not found: %s/%s\n' "${role}" "${corner_count}" "${expected_count}" >&2
    printf '[SKIP] mono image was not saved\n' >&2
    return 1
  fi
  printf '[OK] %s corners found: %s/%s (preview: %s)\n' \
    "${role}" "${corner_count}" "${expected_count}" "${OUT_DIR}/preview/${role}.png"
}

capture_mono() {
  local role="$1" directory number sequence path id json
  if ! detect_mono_corners "${role}"; then
    return
  fi
  directory="${OUT_DIR}/mono_${role}"
  number="$(next_image_number "${directory}")"
  printf -v sequence '%03d' "${number}"
  path="${directory}/${sequence}.png"
  new_request_id
  id="${REQUEST_ID}"
  json="$(jq -cn --arg id "${id}" --arg role "${role}" --arg output "${path}" \
    '{id:$id,cmd:"calib_capture_frame",role:$role,output:$output}')"
  if request "${id}" "${json}"; then
    printf '[OK] %s mono saved: %s\n' "${role}" "${path}"
  fi
}

preview_stereo() {
  local id json config
  new_request_id
  id="${REQUEST_ID}"
  config="$(board_args)"
  json="$(jq -cn --arg id "${id}" \
    --arg left_output "${OUT_DIR}/preview/left.png" \
    --arg right_output "${OUT_DIR}/preview/right.png" \
    --argjson board "${config}" \
    '{id:$id,cmd:"calib_detect_stereo_corners",left_role:"left",right_role:"right",left_output:$left_output,right_output:$right_output} + $board')"
  STEREO_PREVIEW_READY=false
  if request "${id}" "${json}"; then
    printf '\nStereo corner preview:\n'
    printf '  left : found=%s, corners=%s\n' "$(jq -r '.left_found' <<<"${LAST_JSON}")" "$(jq -r '.left_corner_count' <<<"${LAST_JSON}")"
    printf '  right: found=%s, corners=%s\n' "$(jq -r '.right_found' <<<"${LAST_JSON}")" "$(jq -r '.right_corner_count' <<<"${LAST_JSON}")"
    printf '  both : %s\n' "$(jq -r '.both_found' <<<"${LAST_JSON}")"
    printf '  images: %s, %s\n\n' "${OUT_DIR}/preview/left.png" "${OUT_DIR}/preview/right.png"
    if [[ "$(jq -r '.both_found' <<<"${LAST_JSON}")" == true ]]; then
      STEREO_PREVIEW_READY=true
    fi
  fi
}

capture_stereo() {
  if [[ "${STEREO_PREVIEW_READY}" != true ]]; then
    printf '[WARN] stereo capture refused: press p and confirm both_found=true immediately before capture.\n' >&2
    return
  fi
  STEREO_PREVIEW_READY=false
  local number sequence left_path right_path id json
  number="$(next_stereo_number)"
  printf -v sequence '%03d' "${number}"
  left_path="${OUT_DIR}/stereo/left/${sequence}.png"
  right_path="${OUT_DIR}/stereo/right/${sequence}.png"
  new_request_id
  id="${REQUEST_ID}"
  json="$(jq -cn --arg id "${id}" --arg left "${left_path}" --arg right "${right_path}" \
    '{id:$id,cmd:"calib_capture_stereo",left_role:"left",right_role:"right",left_output:$left,right_output:$right}')"
  if request "${id}" "${json}"; then
    printf '[OK] stereo pair saved: %s\n' "${sequence}"
  fi
}

calibrate_mono() {
  local role="$1" id json config
  new_request_id
  id="${REQUEST_ID}"
  config="$(board_args)"
  json="$(jq -cn --arg id "${id}" --arg folder "${OUT_DIR}/mono_${role}" \
    --arg output "${OUT_DIR}/mono_${role}.yml" --argjson board "${config}" \
    '{id:$id,cmd:"mono_calibrate",image_folder:$folder,output_file:$output} + $board')"
  if request "${id}" "${json}"; then
    printf '[OK] %s mono calibration: %s (RMS=%s)\n' "${role}" "${OUT_DIR}/mono_${role}.yml" \
      "$(jq -r '.rms' <<<"${LAST_JSON}")"
  fi
}

calibrate_stereo() {
  local id json config
  if ! validate_stereo_pairs; then
    printf '[SKIP] stereo calibration was not started\n' >&2
    return
  fi
  new_request_id
  id="${REQUEST_ID}"
  config="$(board_args)"
  json="$(jq -cn --arg id "${id}" --arg left_dir "${OUT_DIR}/stereo/left" \
    --arg right_dir "${OUT_DIR}/stereo/right" --arg left_calib "${OUT_DIR}/mono_left.yml" \
    --arg right_calib "${OUT_DIR}/mono_right.yml" --arg output "${OUT_DIR}/stereo.yml" \
    --argjson board "${config}" \
    '{id:$id,cmd:"stereo_calibrate",left_dir:$left_dir,right_dir:$right_dir,left_calibration_file:$left_calib,right_calibration_file:$right_calib,output_file:$output} + $board')"
  if request "${id}" "${json}"; then
    printf '[OK] stereo calibration: %s (RMS=%s)\n' "${OUT_DIR}/stereo.yml" "$(jq -r '.rms' <<<"${LAST_JSON}")"
  fi
}

print_menu() {
  printf '\nCalibration session (%sx%s corners, %s mm)\n' "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}"
  printf '  p preview stereo corners    c capture stereo pair\n'
  printf '  l capture left mono         r capture right mono\n'
  printf '  1 calibrate left mono       2 calibrate right mono\n'
  printf '  3 calibrate stereo          i show image counts\n'
  printf '  v show live preview URLs\n'
  printf '  q shutdown and quit\n\n'
}

show_live_preview() {
  printf '\nLive preview:\n'
  printf '  left : %s\n' "${LEFT_STREAM_URL}"
  printf '  right: %s\n\n' "${RIGHT_STREAM_URL}"
}

start_backend() {
  WORK_DIR="$(mktemp -d -t calibration-session.XXXXXX)"
  BACKEND_LOG="${WORK_DIR}/backend.stderr.log"
  coproc CALIBRATION_BACKEND {
    "${BIN}" serve --control stdio --mjpeg-host "${MJPEG_HOST}" --mjpeg-port "${MJPEG_PORT}" 2>"${BACKEND_LOG}"
  }
  BACKEND_PID="${CALIBRATION_BACKEND_PID}"
  exec {BACKEND_OUT}<&"${CALIBRATION_BACKEND[0]}"
  exec {BACKEND_IN}>&"${CALIBRATION_BACKEND[1]}"
  wait_for_json '.event == "ready"' 'backend ready event'
}

open_camera() {
  local role="$1" camera="$2" id json
  new_request_id
  id="${REQUEST_ID}"
  json="$(jq -cn --arg id "${id}" --arg role "${role}" --argjson camera "${camera}" \
    '{id:$id,cmd:"open_camera",camera_id:$camera,role:$role}')"
  request "${id}" "${json}" || die "failed to open ${role} camera ${camera}"
  printf '[OK] opened %s camera: %s\n' "${role}" "${camera}"
}

start_stream() {
  local role="$1" id json url
  new_request_id
  id="${REQUEST_ID}"
  json="$(jq -cn --arg id "${id}" --arg role "${role}" \
    '{id:$id,cmd:"start_stream",role:$role}')"
  request "${id}" "${json}" || die "failed to start ${role} stream"
  url="$(jq -r '.url // empty' <<<"${LAST_JSON}")"
  [[ -n "${url}" ]] || die "start_stream response has no URL for ${role}"
  if [[ "${role}" == left ]]; then
    LEFT_STREAM_URL="${url}"
  else
    RIGHT_STREAM_URL="${url}"
  fi
}

stop_stream() {
  local role="$1" id json
  new_request_id
  id="${REQUEST_ID}"
  json="$(jq -cn --arg id "${id}" --arg role "${role}" \
    '{id:$id,cmd:"stop_stream",role:$role}')"
  request "${id}" "${json}" || true
}

graceful_shutdown() {
  local id json
  stop_stream left
  stop_stream right
  new_request_id
  id="${REQUEST_ID}"
  json="$(jq -cn --arg id "${id}" '{id:$id,cmd:"shutdown"}')"
  request "${id}" "${json}" || true
  wait "${BACKEND_PID}" 2>/dev/null || true
  BACKEND_PID=""
}

need_cmd jq
need_cmd awk
[[ -r /dev/tty ]] || die '/dev/tty is not available'
[[ -x "${BIN}" ]] || die "backend is not executable: ${BIN}"
nonnegative_integer "${LEFT_CAMERA}" || die 'LEFT_CAMERA must be a non-negative integer'
nonnegative_integer "${RIGHT_CAMERA}" || die 'RIGHT_CAMERA must be a non-negative integer'
[[ "${LEFT_CAMERA}" != "${RIGHT_CAMERA}" ]] || die 'LEFT_CAMERA and RIGHT_CAMERA must differ'
positive_integer "${BOARD_X}" || die 'BOARD_X must be a positive integer'
positive_integer "${BOARD_Y}" || die 'BOARD_Y must be a positive integer'
positive_number "${SQUARE_MM}" || die 'SQUARE_MM must be a positive number'

mkdir -p "${OUT_DIR}/mono_left" "${OUT_DIR}/mono_right" \
  "${OUT_DIR}/stereo/left" "${OUT_DIR}/stereo/right" "${OUT_DIR}/preview"
OUT_DIR="$(cd -- "${OUT_DIR}" && pwd)"

start_backend
open_camera left "${LEFT_CAMERA}"
start_stream left
open_camera right "${RIGHT_CAMERA}"
start_stream right
print_menu
show_live_preview
show_counts

while true; do
  printf 'calibration> '
  IFS= read -rsn1 key </dev/tty || break
  printf '%s\n' "${key}"
  case "${key}" in
    p) preview_stereo ;;
    c) capture_stereo ;;
    l) capture_mono left ;;
    r) capture_mono right ;;
    1) calibrate_mono left ;;
    2) calibrate_mono right ;;
    3) calibrate_stereo ;;
    i) show_counts ;;
    v) show_live_preview ;;
    q) break ;;
    *) printf '[WARN] unknown key: %q\n' "${key}" >&2 ;;
  esac
done

graceful_shutdown
printf 'Calibration session finished. Artifacts were kept in %s\n' "${OUT_DIR}"
