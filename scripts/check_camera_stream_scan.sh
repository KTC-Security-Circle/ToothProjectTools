#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

TOOTH_BACKEND="${TOOTH_BACKEND:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
LEFT_CAMERA="${LEFT_CAMERA:-0}"
RIGHT_CAMERA="${RIGHT_CAMERA:-2}"
MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39010}"
KEEP_WORK_DIR="${KEEP_WORK_DIR:-0}"

WORK_DIR="$(mktemp -d)"
FIFO_IN="${WORK_DIR}/backend.in"
BACKEND_LOG="${WORK_DIR}/backend.log"
BACKEND_PID=""

log() { printf '[INFO] %s\n' "$*"; }
ok() { printf '[OK] %s\n' "$*"; }
die() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }

cleanup() {
  if [[ -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null; then
    kill "${BACKEND_PID}" 2>/dev/null || true
    wait "${BACKEND_PID}" 2>/dev/null || true
  fi
  if [[ "${KEEP_WORK_DIR}" == "1" ]]; then
    printf '[INFO] work directory kept: %s\n' "${WORK_DIR}"
  else
    rm -rf -- "${WORK_DIR}"
  fi
}
trap cleanup EXIT

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

wait_for_pattern() {
  local pattern="$1"
  local timeout_seconds="${2:-15}"
  local start
  start="$(date +%s)"
  while true; do
    grep -Fq -- "${pattern}" "${BACKEND_LOG}" && return 0
    (( $(date +%s) - start < timeout_seconds )) ||
      die "timeout waiting for backend output: ${pattern}"
    sleep 0.1
  done
}

send_json() {
  printf '%s\n' "$1" >&3
}

check_response_ok() {
  local id="$1"
  wait_for_pattern "\"id\":\"${id}\""
  local line
  line="$(grep -F "\"id\":\"${id}\"" "${BACKEND_LOG}" | tail -1)"
  grep -Fq '"ok":true' <<<"${line}" ||
    die "command failed: ${id}: ${line}"
  ok "${id}"
}

capture_size() {
  file -b -- "$1" | sed -nE 's/.* ([0-9]+) x ([0-9]+).*/\1x\2/p'
}

check_mjpeg_endpoint() {
  local role="$1"
  local url="http://${MJPEG_HOST}:${MJPEG_PORT}/${role}.mjpg"
  local sample_file="${WORK_DIR}/${role}.mjpeg.sample"
  local curl_status

  set +e
  timeout 5s curl \
    --fail \
    --silent \
    --show-error \
    --max-time 4 \
    --output "${sample_file}" \
    "${url}"
  curl_status=$?
  set -e

  case "${curl_status}" in
    0|28)
      ;;
    *)
      die "${role} MJPEG endpoint request failed: status=${curl_status}, url=${url}"
      ;;
  esac

  [[ -s "${sample_file}" ]] ||
    die "${role} MJPEG endpoint returned no data: ${url}"

  ok "${role} MJPEG endpoint reachable"
}

main() {
  for cmd in mkfifo curl timeout file grep date; do
    require_command "${cmd}"
  done
  [[ -x "${TOOTH_BACKEND}" ]] || die "backend not executable: ${TOOTH_BACKEND}"

  mkdir -p "${WORK_DIR}/capture"
  mkfifo "${FIFO_IN}"

  "${TOOTH_BACKEND}" \
    serve \
    --control stdio \
    --mjpeg-host "${MJPEG_HOST}" \
    --mjpeg-port "${MJPEG_PORT}" \
    <"${FIFO_IN}" >"${BACKEND_LOG}" 2>&1 &
  BACKEND_PID="$!"

  exec 3>"${FIFO_IN}"
  wait_for_pattern '"event":"ready"'
  ok "backend ready"

  send_json '{"id":"smoke-ping","cmd":"ping"}'
  check_response_ok "smoke-ping"

  send_json "{\"id\":\"open-left\",\"cmd\":\"open_camera\",\"camera_id\":${LEFT_CAMERA},\"role\":\"left\"}"
  check_response_ok "open-left"

  send_json "{\"id\":\"open-right\",\"cmd\":\"open_camera\",\"camera_id\":${RIGHT_CAMERA},\"role\":\"right\"}"
  check_response_ok "open-right"

  send_json '{"id":"stream-left","cmd":"start_stream","role":"left"}'
  check_response_ok "stream-left"

  send_json '{"id":"stream-right","cmd":"start_stream","role":"right"}'
  check_response_ok "stream-right"

  check_mjpeg_endpoint "left"
  check_mjpeg_endpoint "right"

  send_json "{\"id\":\"capture-left\",\"cmd\":\"capture_frame\",\"role\":\"left\",\"output\":\"${WORK_DIR}/capture/left.png\"}"
  check_response_ok "capture-left"

  send_json "{\"id\":\"capture-right\",\"cmd\":\"capture_frame\",\"role\":\"right\",\"output\":\"${WORK_DIR}/capture/right.png\"}"
  check_response_ok "capture-right"

  send_json "{\"id\":\"capture-stereo\",\"cmd\":\"capture_stereo\",\"left_role\":\"left\",\"right_role\":\"right\",\"left_output\":\"${WORK_DIR}/capture/stereo_left.png\",\"right_output\":\"${WORK_DIR}/capture/stereo_right.png\"}"
  check_response_ok "capture-stereo"

  for image in left.png right.png stereo_left.png stereo_right.png; do
    [[ -s "${WORK_DIR}/capture/${image}" ]] ||
      die "capture missing or empty: ${image}"
  done

  left_size="$(capture_size "${WORK_DIR}/capture/stereo_left.png")"
  right_size="$(capture_size "${WORK_DIR}/capture/stereo_right.png")"
  [[ -n "${left_size}" && "${left_size}" == "${right_size}" ]] ||
    die "stereo image size mismatch: left=${left_size}, right=${right_size}"
  ok "stereo capture size: ${left_size}"

  send_json '{"id":"stop-left","cmd":"stop_stream","role":"left"}'
  check_response_ok "stop-left"
  send_json '{"id":"stop-right","cmd":"stop_stream","role":"right"}'
  check_response_ok "stop-right"

  send_json '{"id":"close-left","cmd":"close_camera","role":"left"}'
  check_response_ok "close-left"
  send_json '{"id":"close-right","cmd":"close_camera","role":"right"}'
  check_response_ok "close-right"

  send_json '{"id":"smoke-shutdown","cmd":"shutdown"}'
  check_response_ok "smoke-shutdown"

  wait "${BACKEND_PID}" || true
  BACKEND_PID=""

  printf '\nPASS: camera and stream smoke test\n'
}

main "$@"
