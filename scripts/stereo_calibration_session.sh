#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

BIN="${BIN:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
LEFT_CAMERA="${LEFT_CAMERA:-0}"
RIGHT_CAMERA="${RIGHT_CAMERA:-2}"
LEFT_ROLE="${LEFT_ROLE:-left}"
RIGHT_ROLE="${RIGHT_ROLE:-right}"
BOARD_X="${BOARD_X:-10}"
BOARD_Y="${BOARD_Y:-7}"
SQUARE_MM="${SQUARE_MM:?set SQUARE_MM to the measured checkerboard square size in millimeters}"
OUT_DIR="${OUT_DIR:-${REPO_ROOT}/data/calib}"
MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39011}"

STEREO_DIR="${OUT_DIR}/stereo"
LEFT_DIR="${STEREO_DIR}/left"
RIGHT_DIR="${STEREO_DIR}/right"
PREVIEW_DIR="${OUT_DIR}/preview"
LEFT_PREVIEW="${PREVIEW_DIR}/stereo_left.png"
RIGHT_PREVIEW="${PREVIEW_DIR}/stereo_right.png"
MONO_LEFT="${OUT_DIR}/mono_left.yml"
MONO_RIGHT="${OUT_DIR}/mono_right.yml"
STEREO_OUTPUT="${OUT_DIR}/stereo.yml"
EXPECTED_CORNERS=$((BOARD_X * BOARD_Y))

WORK_DIR="$(mktemp -d)"
FIFO_IN="${WORK_DIR}/backend.in"
BACKEND_LOG="${WORK_DIR}/backend.jsonl"
BACKEND_ERR="${WORK_DIR}/backend.stderr.log"
BACKEND_PID=""
REQUEST_NUMBER=0
LAST_RESPONSE=""
SHUTDOWN_SENT=0
LEFT_FOUND=false
RIGHT_FOUND=false

die() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }
info() { printf '[INFO] %s\n' "$*"; }
send_json() { printf '%s\n' "$1" >&3; }

wait_response() {
  local id="$1" timeout="${2:-20}" start line
  start="$(date +%s)"
  while true; do
    line="$(grep -F "\"id\":\"${id}\"" "${BACKEND_LOG}" 2>/dev/null | tail -n 1 || true)"
    if [[ -n "${line}" ]]; then
      LAST_RESPONSE="${line}"
      if ! jq -e '.ok == true' >/dev/null <<<"${line}"; then
        printf '[ERROR] %s: %s: %s\n' "${id}" \
          "$(jq -r '.error.code // "unknown_error"' <<<"${line}")" \
          "$(jq -r '.error.message // "command failed"' <<<"${line}")" >&2
        return 1
      fi
      return 0
    fi
    kill -0 "${BACKEND_PID}" 2>/dev/null || die "backend exited while waiting for ${id}"
    (( $(date +%s) - start < timeout )) || die "timeout waiting for ${id}"
    sleep 0.05
  done
}

request() {
  local json="$1" timeout="${2:-20}" id
  ((REQUEST_NUMBER += 1))
  id="stereo-session-${REQUEST_NUMBER}"
  json="$(jq -c --arg id "${id}" '. + {id:$id}' <<<"${json}")"
  send_json "${json}"
  wait_response "${id}" "${timeout}"
}

shutdown_backend() {
  if [[ "${SHUTDOWN_SENT}" == 0 && -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null; then
    SHUTDOWN_SENT=1
    request '{"cmd":"shutdown"}' 10 || true
  fi
}

cleanup() {
  local status=$?
  shutdown_backend
  exec 3>&- 2>/dev/null || true
  if [[ -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null; then
    kill "${BACKEND_PID}" 2>/dev/null || true
  fi
  [[ -z "${BACKEND_PID}" ]] || wait "${BACKEND_PID}" 2>/dev/null || true
  if [[ "${status}" -eq 0 ]]; then
    rm -rf -- "${WORK_DIR}"
  else
    printf '[INFO] backend logs: %s\n' "${WORK_DIR}" >&2
  fi
  return "${status}"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

pair_numbers() {
  local directory="$1" file name number
  shopt -s nullglob
  for file in "${directory}"/pair_[0-9][0-9][0-9].png; do
    name="${file##*/pair_}"
    number="${name%.png}"
    printf '%d\n' "$((10#${number}))"
  done
  shopt -u nullglob
}

verify_pairs() {
  local left_numbers right_numbers
  left_numbers="$(pair_numbers "${LEFT_DIR}" | sort -n)"
  right_numbers="$(pair_numbers "${RIGHT_DIR}" | sort -n)"
  [[ "${left_numbers}" == "${right_numbers}" ]] || {
    printf '[ERROR] stereo pair mismatch between %s and %s\n' "${LEFT_DIR}" "${RIGHT_DIR}" >&2
    return 1
  }
}

next_pair_number() {
  local number max=0
  verify_pairs || return 1
  while IFS= read -r number; do
    if [[ -n "${number}" ]] && ((number > max)); then
      max="${number}"
    fi
  done < <(pair_numbers "${LEFT_DIR}")
  printf '%d\n' "$((max + 1))"
}

detect_role() {
  local role="$1" output="$2" label="$3" found corners
  request "$(jq -cn --arg role "${role}" --arg output "${output}" \
    --argjson bx "${BOARD_X}" --argjson by "${BOARD_Y}" --argjson square "${SQUARE_MM}" \
    '{cmd:"calib_detect_corners",role:$role,output:$output,board_corners_x:$bx,board_corners_y:$by,square_size_mm:$square}')" || return 1
  found="$(jq -r '.found' <<<"${LAST_RESPONSE}")"
  corners="$(jq -r '.corner_count' <<<"${LAST_RESPONSE}")"
  printf '[%s] %s: found=%s  corners=%s/%s\n' "${label}" "${role}" "${found}" "${corners}" "${EXPECTED_CORNERS}"
  [[ "${role}" == "${LEFT_ROLE}" ]] && LEFT_FOUND="${found}" || RIGHT_FOUND="${found}"
}

preview_corners() {
  detect_role "${LEFT_ROLE}" "${LEFT_PREVIEW}" PREVIEW
  detect_role "${RIGHT_ROLE}" "${RIGHT_PREVIEW}" PREVIEW
}

capture_pair() {
  local number left_output right_output

  verify_pairs || return 1
  number="$(next_pair_number)" || return 1

  left_output="$(printf '%s/pair_%03d.png' "${LEFT_DIR}" "${number}")"
  right_output="$(printf '%s/pair_%03d.png' "${RIGHT_DIR}" "${number}")"

  request "$(
    jq -cn \
      --arg left_role "${LEFT_ROLE}" \
      --arg right_role "${RIGHT_ROLE}" \
      --arg left_output "${left_output}" \
      --arg right_output "${right_output}" \
      '{
        cmd:"calib_capture_stereo",
        left_role:$left_role,
        right_role:$right_role,
        left_output:$left_output,
        right_output:$right_output
      }'
  )" || return 1

  printf '[CAPTURE] pair_%03d saved\n' "${number}"
}

show_pair_count() {
  local left_count right_count
  left_count="$(pair_numbers "${LEFT_DIR}" | wc -l)"
  right_count="$(pair_numbers "${RIGHT_DIR}" | wc -l)"
  printf 'captured pairs: %s\nleft images: %s\nright images: %s\nrecommended minimum: 10+\n' \
    "$([[ "${left_count}" == "${right_count}" ]] && printf '%s' "${left_count}" || printf 'mismatch')" \
    "${left_count}" "${right_count}"
}

run_mono_calibration() {
  verify_pairs || return 1
  request "$(jq -cn --arg folder "${LEFT_DIR}" --arg output "${MONO_LEFT}" \
    --argjson bx "${BOARD_X}" --argjson by "${BOARD_Y}" --argjson square "${SQUARE_MM}" \
    '{cmd:"mono_calibrate",image_folder:$folder,output_file:$output,board_corners_x:$bx,board_corners_y:$by,square_size_mm:$square}')" 120 || return 1
  printf '[CALIB] left mono RMS=%s\n' "$(jq -r '.rms' <<<"${LAST_RESPONSE}")"
  request "$(jq -cn --arg folder "${RIGHT_DIR}" --arg output "${MONO_RIGHT}" \
    --argjson bx "${BOARD_X}" --argjson by "${BOARD_Y}" --argjson square "${SQUARE_MM}" \
    '{cmd:"mono_calibrate",image_folder:$folder,output_file:$output,board_corners_x:$bx,board_corners_y:$by,square_size_mm:$square}')" 120 || return 1
  printf '[CALIB] right mono RMS=%s\n' "$(jq -r '.rms' <<<"${LAST_RESPONSE}")"
}

validate_stereo_output() {
  local key
  [[ -s "${STEREO_OUTPUT}" ]] || { printf '[ERROR] stereo output is missing or empty: %s\n' "${STEREO_OUTPUT}" >&2; return 1; }
  for key in R T R1 R2 P1 P2 Q; do
    grep -Eq "^[[:space:]]*${key}:" "${STEREO_OUTPUT}" || {
      printf '[ERROR] stereo output is missing key %s\n' "${key}" >&2
      return 1
    }
  done
}

run_stereo_calibration() {
  verify_pairs || return 1
  if [[ ! -s "${MONO_LEFT}" || ! -s "${MONO_RIGHT}" ]]; then
    printf '[ERROR] run mono calibration first\n' >&2
    return 1
  fi
  request "$(jq -cn --arg left_dir "${LEFT_DIR}" --arg right_dir "${RIGHT_DIR}" \
    --arg left_calibration_file "${MONO_LEFT}" --arg right_calibration_file "${MONO_RIGHT}" \
    --arg output_file "${STEREO_OUTPUT}" \
    '{cmd:"stereo_calibrate",left_dir:$left_dir,right_dir:$right_dir,left_calibration_file:$left_calibration_file,right_calibration_file:$right_calibration_file,output_file:$output_file}')" 120 || return 1
  validate_stereo_output || return 1
  printf '[CALIB] stereo RMS=%s\n' "$(jq -r '.rms' <<<"${LAST_RESPONSE}")"
  printf '[CALIB] output=%s\n' "${STEREO_OUTPUT}"
}

command -v jq >/dev/null || die "jq is required"
[[ -x "${BIN}" ]] || die "backend is not executable: ${BIN}"
[[ "${LEFT_CAMERA}" =~ ^[0-9]+$ ]] || die "LEFT_CAMERA must be a non-negative integer"
[[ "${RIGHT_CAMERA}" =~ ^[0-9]+$ ]] || die "RIGHT_CAMERA must be a non-negative integer"
[[ "${LEFT_CAMERA}" != "${RIGHT_CAMERA}" ]] || die "LEFT_CAMERA and RIGHT_CAMERA must be different"
[[ "${LEFT_ROLE}" != "${RIGHT_ROLE}" ]] || die "LEFT_ROLE and RIGHT_ROLE must be different"
[[ "${BOARD_X}" =~ ^[1-9][0-9]*$ ]] || die "BOARD_X must be positive"
[[ "${BOARD_Y}" =~ ^[1-9][0-9]*$ ]] || die "BOARD_Y must be positive"
awk -v value="${SQUARE_MM}" 'BEGIN { exit !(value > 0) }' || die "SQUARE_MM must be positive"

mkdir -p -- "${LEFT_DIR}" "${RIGHT_DIR}" "${PREVIEW_DIR}"
verify_pairs || die "repair the stereo pair dataset before starting"
mkfifo "${FIFO_IN}"
: >"${BACKEND_LOG}"

"${BIN}" serve --control stdio --mjpeg-host "${MJPEG_HOST}" --mjpeg-port "${MJPEG_PORT}" \
  <"${FIFO_IN}" >"${BACKEND_LOG}" 2>"${BACKEND_ERR}" &
BACKEND_PID=$!
exec 3>"${FIFO_IN}"

for _ in {1..200}; do
  grep -Fq '"event":"ready"' "${BACKEND_LOG}" && break
  kill -0 "${BACKEND_PID}" 2>/dev/null || die "backend exited before ready"
  sleep 0.05
done
grep -Fq '"event":"ready"' "${BACKEND_LOG}" || die "timeout waiting for ready"

request "$(jq -cn --argjson camera_id "${LEFT_CAMERA}" --arg role "${LEFT_ROLE}" \
  '{cmd:"open_camera",camera_id:$camera_id,role:$role}')"
request "$(jq -cn --argjson camera_id "${RIGHT_CAMERA}" --arg role "${RIGHT_ROLE}" \
  '{cmd:"open_camera",camera_id:$camera_id,role:$role}')"
request "$(jq -cn --arg role "${LEFT_ROLE}" '{cmd:"start_stream",role:$role}')"
request "$(jq -cn --arg role "${RIGHT_ROLE}" '{cmd:"start_stream",role:$role}')"

printf '[STREAM] left : http://%s:%s/%s.mjpg\n' "${MJPEG_HOST}" "${MJPEG_PORT}" "${LEFT_ROLE}"
printf '[STREAM] right: http://%s:%s/%s.mjpg\n\n' "${MJPEG_HOST}" "${MJPEG_PORT}" "${RIGHT_ROLE}"
printf '%s\n' 'Stereo calibration:' \
  '- Keep both cameras fixed.' \
  '- Move only the checkerboard.' \
  '- Capture different positions, angles and distances.' \
  "- The full ${BOARD_X}x${BOARD_Y} inner corners must be visible in both cameras." ''
printf '%s\n' '[p] preview corners' '[SPACE] capture stereo pair' '[i] captured pair count' \
  '[m] run mono calibration' '[s] run stereo calibration' '[a] run mono + stereo calibration' '[q] quit'

while IFS= read -rsn1 key; do
  case "${key}" in
    p) preview_corners || true ;;
    ' ') capture_pair || true ;;
    i) show_pair_count ;;
    m) run_mono_calibration || true ;;
    s) run_stereo_calibration || true ;;
    a) run_mono_calibration && run_stereo_calibration || true ;;
    q) break ;;
  esac
done

shutdown_backend
