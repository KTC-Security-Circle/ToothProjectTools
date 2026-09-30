#!/usr/bin/env bash
# Camera-Projector hardware check using a fixed delay before each capture.
# This is not a formal scan dataset generator; it only saves hardware-test frames.
#
# Usage:
#   CAMERA_ID=4 MONITOR_INDEX=1 DELAY_MS=100 \
#     ./scripts/check_camera_projector_delay.sh
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BACKEND="${TOOTH_BACKEND:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
CAMERA_ID="${CAMERA_ID:-0}"
MONITOR_INDEX="${MONITOR_INDEX:-0}"
DELAY_MS="${DELAY_MS:-100}"
MJPEG_PORT="${MJPEG_PORT:-39010}"
OUTPUT_DIR="${OUTPUT_DIR:-${REPO_ROOT}/data/check_camera_projector_delay}"
PROJECTOR_ROLE=projector
WINDOW_ROLE=projector
CAMERA_ROLE=left
RESPONSE=
CAMERA_OPEN=0
STREAM_STARTED=0
WINDOW_OPEN=0
PROJECTOR_OPEN=0
CLEANED_UP=0

command -v jq >/dev/null || { echo 'jq is required' >&2; exit 2; }
[[ -x "${BACKEND}" ]] || { echo "backend not executable: ${BACKEND}" >&2; exit 2; }

for value_name in CAMERA_ID MONITOR_INDEX DELAY_MS MJPEG_PORT; do
  value="${!value_name}"
  [[ "${value}" =~ ^[0-9]+$ ]] || {
    echo "${value_name} must be a non-negative integer: ${value}" >&2
    exit 2
  }
done
(( MJPEG_PORT > 0 && MJPEG_PORT <= 65535 )) || {
  echo "MJPEG_PORT must be between 1 and 65535: ${MJPEG_PORT}" >&2
  exit 2
}

DELAY_SECONDS="$(awk -v ms="${DELAY_MS}" 'BEGIN { printf "%.3f", ms / 1000 }')"

coproc BACKEND_PROC { "${BACKEND}" serve --control stdio --mjpeg-port "${MJPEG_PORT}"; }
BACKEND_PID="${BACKEND_PROC_PID}"
BACKEND_READ_FD="${BACKEND_PROC[0]}"
BACKEND_WRITE_FD="${BACKEND_PROC[1]}"

request() {
  local json="$1" id line
  id="$(jq -r '.id' <<<"${json}")"
  printf '%s\n' "${json}" >&"${BACKEND_WRITE_FD}"
  while IFS= read -r line <&"${BACKEND_READ_FD}"; do
    [[ "$(jq -r '.id // empty' <<<"${line}" 2>/dev/null)" == "${id}" ]] || continue
    jq -e '.ok == true' >/dev/null <<<"${line}" || {
      echo "${line}" >&2
      return 1
    }
    RESPONSE="${line}"
    return 0
  done
  echo "backend closed while waiting for response id=${id}" >&2
  return 1
}

cleanup_request() {
  request "$1" >/dev/null 2>&1 || true
}

cleanup() {
  local exit_status=$?
  (( CLEANED_UP == 0 )) || return "${exit_status}"
  CLEANED_UP=1
  set +e
  (( PROJECTOR_OPEN == 0 )) || cleanup_request '{"id":"cleanup-projector","cmd":"close_projector","projector_role":"projector"}'
  (( WINDOW_OPEN == 0 )) || cleanup_request '{"id":"cleanup-window","cmd":"close_window","window_role":"projector"}'
  (( STREAM_STARTED == 0 )) || cleanup_request '{"id":"cleanup-stream","cmd":"stop_stream","role":"left"}'
  (( CAMERA_OPEN == 0 )) || cleanup_request '{"id":"cleanup-camera","cmd":"close_camera","role":"left"}'
  cleanup_request '{"id":"cleanup-shutdown","cmd":"shutdown"}'
  wait "${BACKEND_PID}" 2>/dev/null || true
  return "${exit_status}"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

capture_pattern() {
  local index="$1" destination="$2" output
  printf -v output '%s/pattern_%03d.png' "${destination}" "${index}"
  mkdir -p -- "${destination}"

  request "$(jq -cn --argjson index "${index}" \
    '{id:"capture-show",cmd:"show_pattern",projector_role:"projector",index:$index}')"
  echo "pattern=${index}"
  echo "wait=${DELAY_MS}ms"
  sleep "${DELAY_SECONDS}"
  request "$(jq -cn --arg output "${output}" \
    '{id:"capture-frame",cmd:"capture_frame",role:"left",output:$output}')"
  echo "captured=$(jq -r '.path' <<<"${RESPONSE}")"
}

echo 'Camera–Projector delay mode'
echo "delay: ${DELAY_MS} ms"
echo 'Photodiode: disabled'

ready=0
while IFS= read -r line <&"${BACKEND_READ_FD}"; do
  if [[ "$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)" == ready ]]; then
    ready=1
    break
  fi
done
(( ready == 1 )) || { echo 'backend closed before ready event' >&2; exit 1; }

request '{"id":"ping","cmd":"ping"}'
request '{"id":"monitors","cmd":"list_monitors"}'
monitor_count="$(jq -r '(.monitor_count // "0") | tonumber' <<<"${RESPONSE}")"
(( MONITOR_INDEX < monitor_count )) || {
  echo "MONITOR_INDEX=${MONITOR_INDEX} is out of range (monitor_count=${monitor_count})" >&2
  exit 2
}
monitors_json="$(jq -r '.monitors_json // "[]"' <<<"${RESPONSE}")"
window_width="$(jq -r --argjson index "${MONITOR_INDEX}" '.[$index].width | tonumber' <<<"${monitors_json}")"
window_height="$(jq -r --argjson index "${MONITOR_INDEX}" '.[$index].height | tonumber' <<<"${monitors_json}")"
(( window_width > 0 && window_height > 0 )) || {
  echo "invalid monitor dimensions: ${window_width}x${window_height}" >&2
  exit 2
}

request "$(jq -cn --argjson camera "${CAMERA_ID}" \
  '{id:"camera",cmd:"open_camera",camera_id:$camera,role:"left"}')"
CAMERA_OPEN=1
request '{"id":"stream","cmd":"start_stream","role":"left"}'
STREAM_STARTED=1
echo "[STREAM] $(jq -r '.url' <<<"${RESPONSE}")"
request "$(jq -cn \
  --argjson monitor "${MONITOR_INDEX}" \
  --argjson width "${window_width}" \
  --argjson height "${window_height}" \
  '{id:"window",cmd:"open_window",window_role:"projector",title:"Camera Projector Delay Check",width:$width,height:$height,monitor_index:$monitor,fullscreen:true}')"
WINDOW_OPEN=1
request '{"id":"projector","cmd":"open_projector","projector_role":"projector","window_role":"projector","width":480,"height":270}'
PROJECTOR_OPEN=1
request "$(jq -cn \
  --argjson monitor "${MONITOR_INDEX}" \
  --argjson width "${window_width}" \
  --argjson height "${window_height}" \
  '{id:"surface",cmd:"configure_projector_surface",projector_role:"projector",monitor_index:$monitor,width:$width,height:$height,placement:"center"}')"
request '{"id":"patterns","cmd":"generate_patterns","projector_role":"projector"}'
pattern_count="$(jq -r '(.pattern_count // "0") | tonumber' <<<"${RESPONSE}")"
(( pattern_count > 0 )) || { echo "invalid pattern_count: ${pattern_count}" >&2; exit 1; }

current_index=0
request '{"id":"initial-pattern","cmd":"show_pattern","projector_role":"projector","index":0}'
echo "monitor: ${MONITOR_INDEX} (${window_width}x${window_height})"
echo 'Gray Code: 480x270'
echo "patterns: ${pattern_count}"

while true; do
  printf '\n[n] next [p] previous [s] delay capture [a] capture all [q] quit: '
  IFS= read -rsn1 key </dev/tty || break
  echo
  case "${key}" in
    n)
      request '{"id":"next","cmd":"next_pattern","projector_role":"projector"}'
      current_index="$(jq -r '.pattern_index | tonumber' <<<"${RESPONSE}")"
      echo "pattern=${current_index}"
      ;;
    p)
      request '{"id":"previous","cmd":"prev_pattern","projector_role":"projector"}'
      current_index="$(jq -r '.pattern_index | tonumber' <<<"${RESPONSE}")"
      echo "pattern=${current_index}"
      ;;
    s)
      capture_pattern "${current_index}" "${OUTPUT_DIR}/manual"
      ;;
    a)
      for ((index = 0; index < pattern_count; index += 1)); do
        capture_pattern "${index}" "${OUTPUT_DIR}/full"
      done
      current_index=$((pattern_count - 1))
      echo "all patterns captured: ${OUTPUT_DIR}/full"
      ;;
    q)
      break
      ;;
  esac
done
