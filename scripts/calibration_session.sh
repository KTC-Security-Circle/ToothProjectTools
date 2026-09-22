#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

BIN="${BIN:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
CAMERA_ID="${CAMERA_ID:-0}"
CAMERA_ROLE="${CAMERA_ROLE:-left}"
OUT_DIR="${OUT_DIR:-${REPO_ROOT}/data/calib}"
BOARD_X="${BOARD_X:-10}"
BOARD_Y="${BOARD_Y:-7}"
SQUARE_MM="${SQUARE_MM:?set SQUARE_MM to the measured checkerboard square size in millimeters}"
MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39010}"

IMAGE_DIR="${OUT_DIR}/mono_${CAMERA_ROLE}"
PREVIEW_FILE="${OUT_DIR}/preview/${CAMERA_ROLE}.png"
OUTPUT_FILE="${OUT_DIR}/mono_${CAMERA_ROLE}.yml"
WORK_DIR="$(mktemp -d)"
FIFO_IN="${WORK_DIR}/backend.in"
BACKEND_LOG="${WORK_DIR}/backend.jsonl"
BACKEND_ERR="${WORK_DIR}/backend.stderr.log"
BACKEND_PID=""
REQUEST_NUMBER=0
LAST_RESPONSE=""
SHUTDOWN_SENT=0

die() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }
log() { printf '[INFO] %s\n' "$*"; }

send_json() { printf '%s\n' "$1" >&3; }

wait_response() {
  local id="$1" timeout="${2:-20}" start line
  start="$(date +%s)"
  while true; do
    line="$(grep -F "\"id\":\"${id}\"" "${BACKEND_LOG}" 2>/dev/null | tail -n 1 || true)"
    if [[ -n "${line}" ]]; then
      LAST_RESPONSE="${line}"
      jq -e '.ok == true' >/dev/null <<<"${line}" || {
        printf '[ERROR] %s: %s: %s\n' "${id}" \
          "$(jq -r '.error.code // "unknown_error"' <<<"${line}")" \
          "$(jq -r '.error.message // "command failed"' <<<"${line}")" >&2
        return 1
      }
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
  id="mono-session-${REQUEST_NUMBER}"
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

next_frame_path() {
  local max=0 file stem number
  shopt -s nullglob
  for file in "${IMAGE_DIR}"/frame_[0-9][0-9][0-9].png; do
    stem="${file##*/frame_}"
    number="${stem%.png}"
    ((10#${number} > max)) && max=$((10#${number}))
  done
  shopt -u nullglob
  printf '%s/frame_%03d.png' "${IMAGE_DIR}" "$((max + 1))"
}

count_frames() {
  find "${IMAGE_DIR}" -maxdepth 1 -type f -name 'frame_[0-9][0-9][0-9].png' -printf . | wc -c
}

command -v jq >/dev/null || die "jq is required"
[[ -x "${BIN}" ]] || die "backend is not executable: ${BIN}"
[[ "${BOARD_X}" =~ ^[1-9][0-9]*$ ]] || die "BOARD_X must be positive"
[[ "${BOARD_Y}" =~ ^[1-9][0-9]*$ ]] || die "BOARD_Y must be positive"
awk -v value="${SQUARE_MM}" 'BEGIN { exit !(value > 0) }' || die "SQUARE_MM must be positive"
mkdir -p -- "${IMAGE_DIR}" "$(dirname -- "${PREVIEW_FILE}")"
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

request "$(jq -cn --argjson camera_id "${CAMERA_ID}" --arg role "${CAMERA_ROLE}" \
  '{cmd:"open_camera",camera_id:$camera_id,role:$role}')"
request "$(jq -cn --arg role "${CAMERA_ROLE}" '{cmd:"start_stream",role:$role}')"
log "MJPEG stream: http://${MJPEG_HOST}:${MJPEG_PORT}/${CAMERA_ROLE}.mjpg"
log "keys: [p] preview  [SPACE] capture  [m] mono calibrate  [i] count  [q] quit"

while IFS= read -rsn1 key; do
  case "${key}" in
    p)
      request "$(jq -cn --arg role "${CAMERA_ROLE}" --arg output "${PREVIEW_FILE}" \
        --argjson bx "${BOARD_X}" --argjson by "${BOARD_Y}" --argjson square "${SQUARE_MM}" \
        '{cmd:"calib_detect_corners",role:$role,output:$output,board_corners_x:$bx,board_corners_y:$by,square_size_mm:$square}')"
      log "preview: found=$(jq -r '.found' <<<"${LAST_RESPONSE}") corners=$(jq -r '.corner_count' <<<"${LAST_RESPONSE}")/${BOARD_X}x${BOARD_Y} path=${PREVIEW_FILE}"
      ;;
    ' ')
      frame_path="$(next_frame_path)"
      request "$(jq -cn --arg role "${CAMERA_ROLE}" --arg output "${frame_path}" \
        '{cmd:"calib_capture_frame",role:$role,output:$output}')"
      log "captured: ${frame_path}"
      ;;
    m)
      request "$(jq -cn --arg folder "${IMAGE_DIR}" --arg output "${OUTPUT_FILE}" \
        --argjson bx "${BOARD_X}" --argjson by "${BOARD_Y}" --argjson square "${SQUARE_MM}" \
        '{cmd:"mono_calibrate",image_folder:$folder,output_file:$output,board_corners_x:$bx,board_corners_y:$by,square_size_mm:$square}')" 120
      log "mono calibration: ${OUTPUT_FILE} RMS=$(jq -r '.rms' <<<"${LAST_RESPONSE}")"
      ;;
    i) log "captured image count: $(count_frames)" ;;
    q) break ;;
  esac
done

shutdown_backend
