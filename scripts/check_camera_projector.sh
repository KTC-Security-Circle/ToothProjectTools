#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BACKEND="${TOOTH_BACKEND:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
DEVICE="${PHOTODIODE_DEVICE:-/dev/ttyUSB0}"; BAUD="${PHOTODIODE_BAUD:-115200}"
CAMERA_ID="${CAMERA_ID:-0}"; GUARD="${SYNC_GUARD_MS:-30}"; TIMEOUT="${SYNC_TIMEOUT_MS:-1000}"
MJPEG_PORT="${MJPEG_PORT:-39010}"; OUTPUT_DIR="${OUTPUT_DIR:-${REPO_ROOT}/data/check_camera_projector}"
MONITOR_INDEX="${MONITOR_INDEX:-0}"
PROJECTOR_ROLE=projector; WINDOW_ROLE=projector; CAMERA_ROLE=left
PENDING_EVENTS=()

command -v jq >/dev/null || { echo 'jq is required' >&2; exit 2; }
[[ -x "${BACKEND}" ]] || { echo "backend not executable: ${BACKEND}" >&2; exit 2; }
[[ -e "${DEVICE}" ]] || { echo "device not found: ${DEVICE}" >&2; exit 2; }
[[ -r "${DEVICE}" && -w "${DEVICE}" ]] || { echo "permission denied: ${DEVICE}" >&2; exit 3; }
stty -F "${DEVICE}" "${BAUD}" raw -echo
exec 4<>"${DEVICE}"

coproc BACKEND_PROC { "${BACKEND}" serve --control stdio --mjpeg-port "${MJPEG_PORT}"; }
trap 'printf "%s\n" '\''{"id":"quit","cmd":"shutdown"}'\'' >&"${BACKEND_PROC[1]}" 2>/dev/null || true; exec 4>&- 4<&-; wait "${BACKEND_PROC_PID}" 2>/dev/null || true' EXIT INT TERM

request() {
  local json="$1" id line
  id="$(jq -r '.id' <<<"${json}")"; printf '%s\n' "${json}" >&"${BACKEND_PROC[1]}"
  while IFS= read -r line <&"${BACKEND_PROC[0]}"; do
    if [[ "$(jq -r '.id // empty' <<<"${line}" 2>/dev/null)" != "${id}" ]]; then
      [[ -n "$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)" ]] && PENDING_EVENTS+=("${line}")
      continue
    fi
    jq -e '.ok == true' >/dev/null <<<"${line}" || { echo "${line}" >&2; return 1; }
    RESPONSE="${line}"; return 0
  done
}

wait_for_event() {
  local event="$1" timeout="${2:-$((TIMEOUT / 1000 + 10))}" line seen i
  for i in "${!PENDING_EVENTS[@]}"; do
    line="${PENDING_EVENTS[$i]}"
    seen="$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)"
    unset 'PENDING_EVENTS[i]'
    if [[ "${seen}" == "scan_failed" ]]; then echo "${line}" >&2; return 1; fi
    if [[ "${seen}" == "${event}" ]]; then RESPONSE="${line}"; return 0; fi
  done
  while IFS= read -r -t "${timeout}" line <&"${BACKEND_PROC[0]}"; do
    seen="$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)"
    if [[ "${seen}" == "scan_failed" ]]; then
      echo "${line}" >&2
      return 1
    fi
    if [[ "${seen}" == "${event}" ]]; then
      RESPONSE="${line}"
      return 0
    fi
  done
  echo "timeout waiting for ${event}" >&2
  return 1
}

while IFS= read -r line <&"${BACKEND_PROC[0]}"; do [[ "${line}" == *'"event":"ready"'* ]] && break; done
request '{"id":"ping","cmd":"ping"}'
request '{"id":"monitors","cmd":"list_monitors"}'
monitor_count="$(jq -r '(.monitor_count // "0") | tonumber' <<<"${RESPONSE}")"
(( MONITOR_INDEX >= 0 && MONITOR_INDEX < monitor_count )) || { echo "MONITOR_INDEX=${MONITOR_INDEX} is out of range" >&2; exit 2; }
monitors_json="$(jq -r '.monitors_json // "[]"' <<<"${RESPONSE}")"
window_width="$(jq -r --argjson i "${MONITOR_INDEX}" '.[$i].width' <<<"${monitors_json}")"
window_height="$(jq -r --argjson i "${MONITOR_INDEX}" '.[$i].height' <<<"${monitors_json}")"
request "$(jq -cn --argjson camera "${CAMERA_ID}" '{id:"camera",cmd:"open_camera",camera_id:$camera,role:"left"}')"
request '{"id":"stream","cmd":"start_stream","role":"left"}'
echo "[STREAM] $(jq -r '.url' <<<"${RESPONSE}")"
request "$(jq -cn --argjson monitor "${MONITOR_INDEX}" --argjson width "${window_width}" --argjson height "${window_height}" '{id:"window",cmd:"open_window",window_role:"projector",title:"Camera Projector Check",width:$width,height:$height,monitor_index:$monitor,fullscreen:true}')"
request '{"id":"projector","cmd":"open_projector","projector_role":"projector","window_role":"projector","width":480,"height":270}'
display_width=$((window_width - 64))
(( display_width > 0 )) || { echo 'photodiode_marker_margin_unavailable' >&2; exit 2; }
request "$(jq -cn --argjson monitor "${MONITOR_INDEX}" --argjson width "${display_width}" --argjson height "${window_height}" '{id:"surface",cmd:"configure_projector_surface",projector_role:"projector",monitor_index:$monitor,width:$width,height:$height,placement:"center"}')"
request '{"id":"patterns","cmd":"generate_patterns","projector_role":"projector"}'
echo "Photodiode connected: ${DEVICE} @ ${BAUD}"

index=0
while true; do
  printf '\n[n] next [p] previous [t] Photodiode test [s] one-pattern synchronized capture [q] quit: '
  IFS= read -rsn1 key </dev/tty; echo
  case "${key}" in
    n) request '{"id":"next","cmd":"next_pattern","projector_role":"projector"}' ;;
    p) request '{"id":"prev","cmd":"prev_pattern","projector_role":"projector"}' ;;
    t)
      index=$((1-index)); expected="${index}"
      request "$(jq -cn --argjson index "${index}" '{id:"transition",cmd:"show_pattern",projector_role:"projector",index:$index}')"
      shown_ns="$(date +%s%N)"
      if IFS= read -r -t 5 received <&4; then
        received="${received%$'\r'}"; now_ns="$(date +%s%N)"
        awk -v e="${expected}" -v r="${received}" -v a="${shown_ns}" -v b="${now_ns}" 'BEGIN{printf "expected=%s received=%s latency_ms=%.3f\n",e,r,(b-a)/1000000}'
      else echo 'photodiode timeout' >&2; fi ;;
    s)
      mkdir -p -- "${OUTPUT_DIR}"
      request "$(jq -cn --arg out "${OUTPUT_DIR}" --arg dev "${DEVICE}" --argjson baud "${BAUD}" --argjson timeout "${TIMEOUT}" --argjson guard "${GUARD}" '{id:"scan",cmd:"scan_start",projector_role:"projector",left_role:"left",output_dir:$out,photodiode_device:$dev,photodiode_baud:$baud,sync_timeout_ms:$timeout,sync_guard_ms:$guard,max_patterns:1}')"
      wait_for_event scan_frame_captured
      echo "captured: $(jq -r '.left_path' <<<"${RESPONSE}")"
      wait_for_event scan_completed
      echo "one-pattern synchronized capture completed: ${OUTPUT_DIR}" ;;
    q) break ;;
  esac
done
