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
DISPLAY_WIDTH="${DISPLAY_WIDTH:-1668}"
DISPLAY_HEIGHT="${DISPLAY_HEIGHT:-1080}"
DISPLAY_X="${DISPLAY_X:-128}"
DISPLAY_Y="${DISPLAY_Y:-0}"

command -v jq >/dev/null || { echo 'jqが必要です' >&2; exit 2; }
[[ -x "${BACKEND}" ]] || { echo "backendを実行できません: ${BACKEND}" >&2; exit 2; }
[[ -e "${PHOTODIODE_DEVICE}" ]] || { echo "デバイスが見つかりません: ${PHOTODIODE_DEVICE}" >&2; exit 2; }
[[ -r "${PHOTODIODE_DEVICE}" && -w "${PHOTODIODE_DEVICE}" ]] || {
  echo "デバイスへのアクセス権がありません: ${PHOTODIODE_DEVICE}" >&2; exit 3;
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
      sync_delay_baseline_started) echo 'カメラのBLACK/WHITE基準画像を作成中...' ;;
      sync_delay_mask_ready)
        pixels="$(jq -r '.measurement_pixel_count' <<<"${line}")"
        echo "測定pixel数: ${pixels}" ;;
      sync_delay_prearm_started) echo; echo 'Photodiodeをpre-arm中...' ;;
      sync_delay_prearm_ready) echo 'Photodiode準備完了: black'; echo ;;
      sync_delay_transition)
        sequence="$(jq -r '.sequence' <<<"${line}")"
        total="$(jq -r '.total' <<<"${line}")"
        state="$(jq -r '.state' <<<"${line}")"
        delay="$(jq -r '.delay_ms' <<<"${line}")"
        printf '[%03d/%03d] %s 遅延=%s ms\n' "${sequence}" "${total}" "${state}" "${delay}" ;;
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
  echo "MONITOR_INDEX=${MONITOR_INDEX}は範囲外です" >&2; exit 2;
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
(( DISPLAY_WIDTH > 0 && DISPLAY_X >= 0 && DISPLAY_X + DISPLAY_WIDTH <= window_width &&
   DISPLAY_HEIGHT > 0 && DISPLAY_Y >= 0 && DISPLAY_Y + DISPLAY_HEIGHT <= window_height )) || {
  echo 'photodiode_marker_margin_unavailable' >&2; exit 2;
}
request "$(jq -cn --argjson monitor "${MONITOR_INDEX}" --argjson width "${DISPLAY_WIDTH}" \
  --argjson height "${DISPLAY_HEIGHT}" --argjson x "${DISPLAY_X}" --argjson y "${DISPLAY_Y}" \
  '{id:"surface",cmd:"configure_projector_surface",projector_role:"projector",monitor_index:$monitor,width:$width,height:$height,x:$x,y:$y,placement:"custom"}')"
request '{"id":"patterns","cmd":"generate_patterns","projector_role":"projector"}'
pattern_count="$(jq -r '.pattern_count' <<<"${RESPONSE}")"
(( pattern_count >= 2 )) || { echo '全面白の基準Patternを利用できません' >&2; exit 2; }
locator_pattern_index=$((pattern_count - 2))
request "$(jq -cn --argjson index "${locator_pattern_index}" \
  '{id:"locator",cmd:"show_pattern",projector_role:"projector",index:$index,photodiode_marker_mode:"locate"}')"

echo
echo 'Photodiode locator: 赤marker'
printf 'pattern: x=%s y=%s 幅=%s 高さ=%s (全面白 index=%s)\n' \
  "$(jq -r '.pattern_x' <<<"${RESPONSE}")" "$(jq -r '.pattern_y' <<<"${RESPONSE}")" \
  "$(jq -r '.pattern_width' <<<"${RESPONSE}")" "$(jq -r '.pattern_height' <<<"${RESPONSE}")" \
  "${locator_pattern_index}"
printf 'marker: x=%s y=%s 幅=%s 高さ=%s\n' \
  "$(jq -r '.marker_x' <<<"${RESPONSE}")" "$(jq -r '.marker_y' <<<"${RESPONSE}")" \
  "$(jq -r '.marker_width' <<<"${RESPONSE}")" "$(jq -r '.marker_height' <<<"${RESPONSE}")"
echo
echo 'Photodiodeを赤い四角の上に配置してください。'
echo 'ENTERで同期遅延測定を開始します。'
IFS= read -r </dev/tty
echo
echo 'markerを赤locatorから同期表示へ切り替えます'

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
echo '設定値:'
echo "SYNC_GUARD_MS=${guard}"
csv_warning="$(jq -r '.csv_warning // empty' <<<"${RESPONSE}")"
[[ -z "${csv_warning}" ]] || echo "CSV警告: ${csv_warning}" >&2
