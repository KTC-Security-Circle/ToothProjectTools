#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BACKEND="${TOOTH_BACKEND:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE:-/dev/ttyUSB0}"
PHOTODIODE_BAUD="${PHOTODIODE_BAUD:-115200}"
CAMERA_ID="${CAMERA_ID:-0}"
MONITOR_INDEX="${MONITOR_INDEX:-0}"
TRANSITIONS="${TRANSITIONS:-60}"
SYNC_TIMEOUT_MS="${SYNC_TIMEOUT_MS:-1000}"
SAFETY_MARGIN_MS="${SAFETY_MARGIN_MS:-5}"
MINIMUM_CONTRAST="${MINIMUM_CONTRAST:-30}"
REQUIRED_RATIO="${REQUIRED_RATIO:-0.90}"
OUTPUT_CSV="${OUTPUT_CSV:-${REPO_ROOT}/data/photodiode_delay.csv}"
MJPEG_PORT="${MJPEG_PORT:-39012}"

command -v jq >/dev/null || { echo 'jq is required' >&2; exit 2; }
[[ -x "${BACKEND}" ]] || { echo "backend not executable: ${BACKEND}" >&2; exit 2; }
[[ -e "${PHOTODIODE_DEVICE}" ]] || { echo "device not found: ${PHOTODIODE_DEVICE}" >&2; exit 2; }
[[ -r "${PHOTODIODE_DEVICE}" && -w "${PHOTODIODE_DEVICE}" ]] || {
  echo "permission denied: ${PHOTODIODE_DEVICE}" >&2; exit 3;
}

coproc BACKEND_PROC { "${BACKEND}" serve --control stdio --mjpeg-port "${MJPEG_PORT}"; }
cleanup() {
  printf '%s\n' '{"id":"quit","cmd":"shutdown"}' >&"${BACKEND_PROC[1]}" 2>/dev/null || true
  wait "${BACKEND_PROC_PID}" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM

request() {
  local json="$1" id line
  id="$(jq -r '.id' <<<"${json}")"
  printf '%s\n' "${json}" >&"${BACKEND_PROC[1]}"
  while IFS= read -r line <&"${BACKEND_PROC[0]}"; do
    [[ "$(jq -r '.id // empty' <<<"${line}" 2>/dev/null)" == "${id}" ]] || continue
    jq -e '.ok == true' >/dev/null <<<"${line}" || { echo "${line}" >&2; return 1; }
    RESPONSE="${line}"
    return 0
  done
}

measure() {
  local json="$1" line event sequence total state delay pixels
  printf '%s\n' "${json}" >&"${BACKEND_PROC[1]}"
  while IFS= read -r line <&"${BACKEND_PROC[0]}"; do
    event="$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)"
    case "${event}" in
      sync_delay_baseline_started) echo 'Building Camera BLACK/WHITE baseline...' ;;
      sync_delay_mask_ready)
        pixels="$(jq -r '.measurement_pixel_count' <<<"${line}")"
        echo "measurement pixels: ${pixels}" ;;
      sync_delay_prearm_started) echo; echo 'Photodiode pre-arm...' ;;
      sync_delay_prearm_ready) echo 'Photodiode ready: black'; echo ;;
      sync_delay_transition)
        sequence="$(jq -r '.sequence' <<<"${line}")"
        total="$(jq -r '.total' <<<"${line}")"
        state="$(jq -r '.state' <<<"${line}")"
        delay="$(jq -r '.delay_ms' <<<"${line}")"
        printf '[%03d/%03d] %s delay=%s ms\n' "${sequence}" "${total}" "${state}" "${delay}" ;;
    esac
    if [[ "$(jq -r '.id // empty' <<<"${line}" 2>/dev/null)" == sync-delay ]]; then
      jq -e '.ok == true' >/dev/null <<<"${line}" || { echo "${line}" >&2; return 1; }
      RESPONSE="${line}"
      return 0
    fi
  done
}

while IFS= read -r line <&"${BACKEND_PROC[0]}"; do
  [[ "${line}" == *'"event":"ready"'* ]] && break
done
request '{"id":"monitors","cmd":"list_monitors"}'
monitor_count="$(jq -r '(.monitor_count // "0") | tonumber' <<<"${RESPONSE}")"
(( MONITOR_INDEX >= 0 && MONITOR_INDEX < monitor_count )) || {
  echo "MONITOR_INDEX=${MONITOR_INDEX} is out of range" >&2; exit 2;
}
monitors_json="$(jq -r '.monitors_json // "[]"' <<<"${RESPONSE}")"
window_width="$(jq -r --argjson i "${MONITOR_INDEX}" '.[$i].width' <<<"${monitors_json}")"
window_height="$(jq -r --argjson i "${MONITOR_INDEX}" '.[$i].height' <<<"${monitors_json}")"

request "$(jq -cn --argjson camera "${CAMERA_ID}" \
  '{id:"camera",cmd:"open_camera",camera_id:$camera,role:"left"}')"
request "$(jq -cn --argjson monitor "${MONITOR_INDEX}" --argjson width "${window_width}" \
  --argjson height "${window_height}" \
  '{id:"window",cmd:"open_window",window_role:"projector",title:"Photodiode delay measurement",width:$width,height:$height,monitor_index:$monitor,fullscreen:true}')"
request '{"id":"projector","cmd":"open_projector","projector_role":"projector","window_role":"projector","width":480,"height":270}'
if (( window_width > 192 )); then marker_reserve=192
elif (( window_width > 128 )); then marker_reserve=128
elif (( window_width > 64 )); then marker_reserve=64
else echo 'photodiode_marker_margin_unavailable' >&2; exit 2
fi
display_width=$((window_width - marker_reserve))
request "$(jq -cn --argjson monitor "${MONITOR_INDEX}" --argjson width "${display_width}" \
  --argjson height "${window_height}" \
  '{id:"surface",cmd:"configure_projector_surface",projector_role:"projector",monitor_index:$monitor,width:$width,height:$height,placement:"center"}')"
request '{"id":"patterns","cmd":"generate_patterns","projector_role":"projector"}'
request '{"id":"locator","cmd":"show_pattern","projector_role":"projector","index":0,"photodiode_marker_mode":"locate"}'

echo
echo 'Photodiode locator marker: RED'
printf 'marker: x=%s y=%s width=%s height=%s\n' \
  "$(jq -r '.marker_x' <<<"${RESPONSE}")" "$(jq -r '.marker_y' <<<"${RESPONSE}")" \
  "$(jq -r '.marker_width' <<<"${RESPONSE}")" "$(jq -r '.marker_height' <<<"${RESPONSE}")"
echo
echo 'Place the photodiode on the red square.'
echo 'Press ENTER to start delay measurement.'
IFS= read -r </dev/tty
echo
echo 'Switching marker: RED -> sync'

measure "$(jq -cn --arg dev "${PHOTODIODE_DEVICE}" --arg csv "${OUTPUT_CSV}" \
  --argjson baud "${PHOTODIODE_BAUD}" --argjson transitions "${TRANSITIONS}" \
  --argjson timeout "${SYNC_TIMEOUT_MS}" --argjson margin "${SAFETY_MARGIN_MS}" \
  --argjson contrast "${MINIMUM_CONTRAST}" --argjson ratio "${REQUIRED_RATIO}" \
  '{id:"sync-delay",cmd:"measure_sync_delay",projector_role:"projector",camera_role:"left",photodiode_device:$dev,photodiode_baud:$baud,transitions:$transitions,sync_timeout_ms:$timeout,safety_margin_ms:$margin,minimum_contrast:$contrast,required_ratio:$ratio,output_csv:$csv}')"

echo
for field in mean_ms median_ms p95_ms p99_ms max_ms; do
  printf '%s: %s\n' "${field}" "$(jq -r --arg field "${field}" '.[$field]' <<<"${RESPONSE}")"
done
guard="$(jq -r '.recommended_guard_ms' <<<"${RESPONSE}")"
echo
echo "recommended_guard_ms: ${guard}"
echo
echo 'Use:'
echo "SYNC_GUARD_MS=${guard}"
csv_warning="$(jq -r '.csv_warning // empty' <<<"${RESPONSE}")"
[[ -z "${csv_warning}" ]] || echo "CSV warning: ${csv_warning}" >&2
