#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

BIN="${BIN:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
LEFT_CAMERA="${LEFT_CAMERA:-0}"
RIGHT_CAMERA="${RIGHT_CAMERA:-2}"
MONITOR_INDEX="${MONITOR_INDEX:-}"
CALIBRATION_FILE="${CALIBRATION_FILE:-data/calib/stereo.yml}"
SYNC_MODE="${SYNC_MODE:-photodiode}"
PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE:-/dev/ttyUSB0}"
PHOTODIODE_BAUD="${PHOTODIODE_BAUD:-115200}"
SYNC_TIMEOUT_MS="${SYNC_TIMEOUT_MS:-1000}"
GUARD_MS="${GUARD_MS:-99}"
SCAN_ID="${SCAN_ID:-scan_$(date +%Y%m%d_%H%M%S)}"
OUTPUT_DIR="${OUTPUT_DIR:-data/scans/${SCAN_ID}}"
PLY_FILE="${PLY_FILE:-${OUTPUT_DIR}/cloud.ply}"
SCAN_DIR="${OUTPUT_DIR}/scan"
DECODE_DIR="${OUTPUT_DIR}/decode"
MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39012}"
CODE_WIDTH="${CODE_WIDTH:-480}"
CODE_HEIGHT="${CODE_HEIGHT:-270}"
DISPLAY_WIDTH="${DISPLAY_WIDTH:-1668}"
DISPLAY_HEIGHT="${DISPLAY_HEIGHT:-1080}"
DECODE_THRESHOLD="${DECODE_THRESHOLD:-}"
MAX_EPIPOLAR_ERROR_PX="${MAX_EPIPOLAR_ERROR_PX:-}"
KEEP_WORK_DIR="${KEEP_WORK_DIR:-0}"
READY_TIMEOUT_SECONDS="${READY_TIMEOUT_SECONDS:-10}"
DEBUG_PROGRESS="${DEBUG_PROGRESS:-0}"
DEBUG_TIMING="${DEBUG_TIMING:-0}"

WORK_DIR=""
BACKEND_LOG=""
BACKEND_ERR=""
BACKEND_PID=""
BACKEND_INPUT_FD=""
BACKEND_OUTPUT_FD=""
RESPONSE=""
PENDING_EVENTS=()
SUCCESS=0
SHUTDOWN_SENT=0
SCAN_RUNNING=0
LEFT_CAMERA_OPEN=0
RIGHT_CAMERA_OPEN=0
LEFT_STREAM_OPEN=0
RIGHT_STREAM_OPEN=0
WINDOW_OPEN=0
PROJECTOR_OPEN=0
RUN_RESULT=""
LOCATOR_PATTERN_INDEX=""

die() { printf '[エラー] %s\n' "$*" >&2; exit 1; }
is_non_negative_integer() { [[ "$1" =~ ^[0-9]+$ ]]; }
is_positive_integer() { [[ "$1" =~ ^[1-9][0-9]*$ ]]; }
is_positive_number() {
  jq -en --arg value "$1" '($value | tonumber) as $n | ($n > 0)' >/dev/null 2>&1
}

backend_is_running() {
  [[ -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null
}

record_line() {
  printf '%s\n' "$1" >>"${BACKEND_LOG}"
}

send_json() {
  printf '%s\n' "$1" >&"${BACKEND_INPUT_FD}"
}

read_backend_line() {
  local timeout="$1" line
  if IFS= read -r -t "${timeout}" line <&"${BACKEND_OUTPUT_FD}"; then
    record_line "${line}"
    REPLY="${line}"
    return 0
  fi
  backend_is_running || return 2
  return 1
}

request() {
  local json="$1" timeout="${2:-20}" id line start
  id="$(jq -r '.id' <<<"${json}")"
  send_json "${json}"
  start="$(date +%s)"
  while (( $(date +%s) - start < timeout )); do
    if ! read_backend_line 1; then
      backend_is_running || { printf '[エラー] backendが終了しました（応答待ち: %s）\n' "${id}" >&2; return 1; }
      continue
    fi
    line="${REPLY}"
    if [[ "$(jq -r '.id // empty' <<<"${line}" 2>/dev/null)" != "${id}" ]]; then
      [[ -n "$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)" ]] && PENDING_EVENTS+=("${line}")
      continue
    fi
    RESPONSE="${line}"
    if ! jq -e '.ok == true' >/dev/null <<<"${line}"; then
      printf '[エラー] %s に失敗しました\n' "${id}" >&2
      printf 'code=%s\nmessage=%s\n' \
        "$(jq -r '.error.code // "unknown_error"' <<<"${line}")" \
        "$(jq -r '.error.message // "command failed"' <<<"${line}")" >&2
      return 1
    fi
    return 0
  done
  printf '[エラー] %s の応答待ちがtimeoutしました\n' "${id}" >&2
  return 1
}

cleanup_request() {
  local json="$1"
  backend_is_running || return 0
  request "${json}" 5 >/dev/null 2>&1 || true
}

stop_scan_for_cleanup() {
  local deadline line event
  (( SCAN_RUNNING == 1 )) || return 0
  cleanup_request '{"id":"scan-stop","cmd":"scan_stop"}'
  deadline=$(( $(date +%s) + 5 ))
  while (( $(date +%s) < deadline )); do
    if take_pending_scan_event "${SCAN_ID}"; then
      line="${REPLY}"
    elif read_backend_line 1; then
      line="${REPLY}"
      [[ "$(jq -r '.scan_id // empty' <<<"${line}" 2>/dev/null)" == "${SCAN_ID}" ]] || continue
    else
      backend_is_running || break
      continue
    fi
    event="$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)"
    if [[ "${event}" == scan_stopped || "${event}" == scan_failed || "${event}" == scan_completed ]]; then
      SCAN_RUNNING=0
      break
    fi
  done
}

take_pending_scan_event() {
  local scan_id="$1" i line event_scan_id
  for i in "${!PENDING_EVENTS[@]}"; do
    line="${PENDING_EVENTS[$i]}"
    event_scan_id="$(jq -r '.scan_id // empty' <<<"${line}" 2>/dev/null)"
    if [[ "${event_scan_id}" == "${scan_id}" ]]; then
      unset 'PENDING_EVENTS[i]'
      REPLY="${line}"
      return 0
    fi
  done
  return 1
}

wait_for_scan_terminal_event() {
  local scan_id="$1" line event
  while true; do
    if take_pending_scan_event "${scan_id}"; then
      line="${REPLY}"
    else
      if ! read_backend_line 1; then
        backend_is_running || die "Scan中にbackendが終了しました"
        continue
      fi
      line="${REPLY}"
      [[ "$(jq -r '.scan_id // empty' <<<"${line}" 2>/dev/null)" == "${scan_id}" ]] || continue
    fi
    event="$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)"
    case "${event}" in
      scan_started)
        printf '[SCAN] スキャンを開始します\n'
        ;;
      scan_pattern_shown)
        if [[ "${DEBUG_PROGRESS}" == 1 ]]; then
          printf '[投影] %s/%s  pattern_index=%s\n' \
            "$(jq -r '(.pattern_index | tonumber) + 1' <<<"${line}")" \
            "$(jq -r '.pattern_count' <<<"${line}")" \
            "$(jq -r '.pattern_index' <<<"${line}")"
        fi
        ;;
      scan_frame_selected)
        printf '[撮影] %s/%s' \
          "$(jq -r '(.pattern_index | tonumber) + 1' <<<"${line}")" \
          "$(jq -r '.pattern_count' <<<"${line}")"
        [[ "${DEBUG_PROGRESS}" != 1 ]] || printf '  pattern_index=%s' "$(jq -r '.pattern_index' <<<"${line}")"
        printf '\n'
        if [[ "$(jq -r '(.pattern_index | tonumber) + 1 == (.pattern_count | tonumber)' <<<"${line}")" == true ]]; then
          printf '[SCAN] 画像保存の完了を待っています...\n'
        fi
        ;;
      scan_frame_captured)
        if [[ "${DEBUG_PROGRESS}" == 1 ]]; then
          printf '[保存] %s/%s  pattern_index=%s\n' \
            "$(jq -r '(.pattern_index | tonumber) + 1' <<<"${line}")" \
            "$(jq -r '.pattern_count' <<<"${line}")" \
            "$(jq -r '.pattern_index' <<<"${line}")"
        fi
        ;;
      scan_pattern_timing)
        if [[ "${DEBUG_TIMING}" == 1 ]]; then
          printf '\n[計測] %s/%s\n' \
            "$(jq -r '(.pattern_index | tonumber) + 1' <<<"${line}")" \
            "$(jq -r '.pattern_count' <<<"${line}")"
          printf '  投影       : %.1f ms\n' "$(jq -r '.show_ms | tonumber' <<<"${line}")"
          printf '  Photodiode : %.1f ms\n' "$(jq -r '.photodiode_wait_ms | tonumber' <<<"${line}")"
          printf '  Guard      : %.1f ms\n' "$(jq -r '.guard_ms | tonumber' <<<"${line}")"
          printf '  Camera待機 : %.1f ms\n' "$(jq -r '.camera_wait_ms | tonumber' <<<"${line}")"
          printf '  Queue待機  : %.1f ms\n' "$(jq -r '.enqueue_wait_ms | tonumber' <<<"${line}")"
          printf '  合計       : %.1f ms\n' "$(jq -r '.loop_ms | tonumber' <<<"${line}")"
        fi
        ;;
      scan_performance_summary)
        printf '\n[Scan性能]\n'
        printf 'Pattern数          : %s\n' "$(jq -r '.pattern_count' <<<"${line}")"
        printf 'Scan取得時間       : %.2f s\n' "$(jq -r '(.scan_acquisition_ms | tonumber) / 1000' <<<"${line}")"
        printf '画像保存完了時間   : %.2f s\n\n' "$(jq -r '(.save_completion_ms | tonumber) / 1000' <<<"${line}")"
        printf '平均 / 最大\n'
        printf '  投影             : %.1f / %.1f ms\n' "$(jq -r '.show_avg_ms|tonumber' <<<"${line}")" "$(jq -r '.show_max_ms|tonumber' <<<"${line}")"
        printf '  Photodiode待機   : %.1f / %.1f ms\n' "$(jq -r '.photodiode_avg_ms|tonumber' <<<"${line}")" "$(jq -r '.photodiode_max_ms|tonumber' <<<"${line}")"
        printf '  Guard            : %.1f ms\n' "$(jq -r '.guard_ms|tonumber' <<<"${line}")"
        printf '  Camera待機       : %.1f / %.1f ms\n' "$(jq -r '.camera_avg_ms|tonumber' <<<"${line}")" "$(jq -r '.camera_max_ms|tonumber' <<<"${line}")"
        printf '  Queue待機        : %.1f / %.1f ms\n' "$(jq -r '.enqueue_avg_ms|tonumber' <<<"${line}")" "$(jq -r '.enqueue_max_ms|tonumber' <<<"${line}")"
        printf '  PNG保存          : %.1f / %.1f ms\n\n' "$(jq -r '.save_avg_ms|tonumber' <<<"${line}")" "$(jq -r '.save_max_ms|tonumber' <<<"${line}")"
        printf '最大Queueサイズ    : %s / %s\n' "$(jq -r '.max_queue_size' <<<"${line}")" "$(jq -r '.queue_capacity' <<<"${line}")"
        printf 'Pre-arm black/white: %.1f / %.1f ms\n' "$(jq -r '.prearm_black_ms|tonumber' <<<"${line}")" "$(jq -r '.prearm_white_ms|tonumber' <<<"${line}")"
        printf 'Reference pre-arm  : %s 回 (平均 %.1f ms)\n' "$(jq -r '.reference_prearm_count' <<<"${line}")" "$(jq -r '.reference_prearm_avg_ms|tonumber' <<<"${line}")"
        ;;
      scan_completed)
        SCAN_RUNNING=0
        printf '[SCAN] 画像保存が完了しました\n'
        return 0
        ;;
      scan_failed)
        SCAN_RUNNING=0
        printf '[エラー] Scanに失敗しました\n' >&2
        printf 'code=%s\nmessage=%s\npattern_index=%s\npattern_kind=%s\nexpected_marker_state=%s\ncaptured_count=%s\ncurrent_index=%s\n' \
          "$(jq -r '.error_code // empty' <<<"${line}")" \
          "$(jq -r '.error_message // empty' <<<"${line}")" \
          "$(jq -r '.pattern_index // empty' <<<"${line}")" \
          "$(jq -r '.pattern_kind // empty' <<<"${line}")" \
          "$(jq -r '.expected_marker_state // empty' <<<"${line}")" \
          "$(jq -r '.captured_count // empty' <<<"${line}")" \
          "$(jq -r '.current_index // empty' <<<"${line}")" >&2
        return 1
        ;;
      scan_stopped)
        SCAN_RUNNING=0
        printf '[エラー] Scanを停止しました\n' >&2
        return 1
        ;;
    esac
  done
}

wait_for_ready() {
  local start line
  start="$(date +%s)"
  while (( $(date +%s) - start < READY_TIMEOUT_SECONDS )); do
    if ! read_backend_line 1; then
      backend_is_running || die "準備完了前にbackendが終了しました"
      continue
    fi
    line="${REPLY}"
    if [[ "$(jq -r '.event // empty' <<<"${line}" 2>/dev/null)" == ready ]]; then
      printf '[OK] backendの準備ができました\n'
      return 0
    fi
  done
  die "backendの起動待ちがtimeoutしました"
}

show_locator() {
  [[ -n "${LOCATOR_PATTERN_INDEX}" ]] || { printf '[エラー] locator pattern indexが未初期化です\n' >&2; return 1; }
  local locator_request
  locator_request="$(jq -cn --argjson index "${LOCATOR_PATTERN_INDEX}" \
    '{id:"locator",cmd:"show_pattern",projector_role:"projector",index:$index,photodiode_marker_mode:"locate"}')"
  if ! request "${locator_request}"; then
    printf '[エラー] 赤locatorを復元できないためsessionを継続できません\n' >&2
    return 1
  fi
  printf '[LOCATOR] 赤 / 全面白\n'
}

print_run_retained() {
  printf '\n[実行] Scan結果を保持しました:\n  %s/\n' "${RUN_DIR}"
}

run_scan_once() {
  local scan_request decode_request reconstruct_request point_count

  RUN_RESULT="running"
  SCAN_ID="scan_$(date +%Y%m%d_%H%M%S)_$(date +%N)"
  RUN_DIR="${SESSION_OUTPUT_DIR}/${SCAN_ID}"
  SCAN_DIR="${RUN_DIR}/scan"
  DECODE_DIR="${RUN_DIR}/decode"
  PLY_FILE="${RUN_DIR}/${PLY_BASENAME}"
  printf '[SCAN] スキャンを開始します\n[同期] 赤locatorから白黒同期へ切り替えます\n[同期] pre-arm中...\n'

  scan_request="$(jq -cn \
    --arg scan_id "${SCAN_ID}" --arg output_dir "${SCAN_DIR}" --arg device "${PHOTODIODE_DEVICE}" \
    --argjson baud "${PHOTODIODE_BAUD}" --argjson timeout "${SYNC_TIMEOUT_MS}" --argjson guard "${GUARD_MS}" \
    '{id:"scan",cmd:"scan_start",scan_id:$scan_id,projector_role:"projector",left_role:"left",right_role:"right",output_dir:$output_dir,sync_mode:"photodiode",photodiode_device:$device,photodiode_baud:$baud,sync_timeout_ms:$timeout,guard_ms:$guard}')"
  if ! request "${scan_request}"; then
    RUN_RESULT="scan_failed"
    print_run_retained
    show_locator || return 1
    return 0
  fi

  SCAN_ID="$(jq -r '.scan_id' <<<"${RESPONSE}")"
  SCAN_RUNNING=1
  printf '[SCAN] id=%s patterns=%s\n' "${SCAN_ID}" "$(jq -r '.pattern_count' <<<"${RESPONSE}")"
  if ! wait_for_scan_terminal_event "${SCAN_ID}"; then
    RUN_RESULT="scan_failed"
    print_run_retained
    show_locator || return 1
    return 0
  fi

  # The scan worker has stopped, so it is safe to return the projector to
  # placement mode while filesystem-only decode/reconstruction runs.
  show_locator || return 1

  decode_request="$(jq -cn --arg input "${SCAN_DIR}" --arg output "${DECODE_DIR}" --arg threshold "${DECODE_THRESHOLD}" \
    '{id:"decode",cmd:"decode_patterns",input_dir:$input,output_dir:$output,allow_partial:false}
     + (if $threshold != "" then {threshold:($threshold|tonumber)} else {} end)')"
  if ! request "${decode_request}" 300; then
    RUN_RESULT="decode_failed"
    print_run_retained
    show_locator || return 1
    return 0
  fi
  printf '[DECODE] デコードが完了しました: 有効 left=%s right=%s\n' \
    "$(jq -r '.left_valid_count' <<<"${RESPONSE}")" "$(jq -r '.right_valid_count' <<<"${RESPONSE}")"

  reconstruct_request="$(jq -cn --arg decode "${DECODE_DIR}" --arg calibration "${CALIBRATION_FILE}" \
    --arg output "${PLY_FILE}" --arg epipolar "${MAX_EPIPOLAR_ERROR_PX}" \
    '{id:"reconstruct",cmd:"reconstruct_point_cloud",decode_dir:$decode,calibration_file:$calibration,output_file:$output}
     + (if $epipolar != "" then {max_epipolar_error_px:($epipolar|tonumber)} else {} end)')"
  if ! request "${reconstruct_request}" 300; then
    RUN_RESULT="reconstruct_failed"
    print_run_retained
    show_locator || return 1
    return 0
  fi
  point_count="$(jq -r '.point_count // empty' <<<"${RESPONSE}")"
  printf '[復元] 点群を生成しました\n'

  if [[ ! -f "${PLY_FILE}" ]]; then
    printf '[エラー] 復元に失敗しました\ncode=ply_not_created\nmessage=PLY fileが作成されませんでした: %s\n' "${PLY_FILE}" >&2
    RUN_RESULT="reconstruct_failed"
    print_run_retained
    show_locator || return 1
    return 0
  fi
  if [[ ! -s "${PLY_FILE}" ]]; then
    printf '[エラー] 復元に失敗しました\ncode=ply_empty\nmessage=PLY fileが空です: %s\n' "${PLY_FILE}" >&2
    RUN_RESULT="reconstruct_failed"
    print_run_retained
    show_locator || return 1
    return 0
  fi

  RUN_RESULT="success"
  printf '[OK] PLY: %s\n' "${PLY_FILE}"
  printf '\n[OK] Scanが完了しました\n\nscan:\n  %s\n\ndecode:\n  %s\n\nply:\n  %s\n' \
    "${SCAN_DIR}" "${DECODE_DIR}" "${PLY_FILE}"
  [[ -z "${point_count}" ]] || printf '\npoints:\n  %s\n' "${point_count}"
  return 0
}

interactive_loop() {
  local scan_key
  while true; do
    if [[ "${RUN_RESULT}" == "" || "${RUN_RESULT}" == success ]]; then
      printf '\n[準備完了] SPACE: Scan開始   Q: 終了\n'
    else
      printf '\n[準備完了] SPACE: 再試行   Q: 終了\n'
    fi
    IFS= read -rsn1 scan_key </dev/tty
    [[ "${scan_key}" == q || "${scan_key}" == Q ]] && break
    [[ "${scan_key}" == ' ' ]] || continue
    # A nonzero result here is reserved for a session-fatal condition, such
    # as losing the projector while restoring the locator.
    run_scan_once || return 1
  done
}

shutdown_backend() {
  local i
  if backend_is_running && (( SHUTDOWN_SENT == 0 )); then
    SHUTDOWN_SENT=1
    cleanup_request '{"id":"shutdown","cmd":"shutdown"}'
  fi
  if [[ -n "${BACKEND_INPUT_FD}" ]]; then
    exec {BACKEND_INPUT_FD}>&- 2>/dev/null || true
    BACKEND_INPUT_FD=""
  fi
  if [[ -n "${BACKEND_OUTPUT_FD}" ]]; then
    exec {BACKEND_OUTPUT_FD}<&- 2>/dev/null || true
    BACKEND_OUTPUT_FD=""
  fi
  if backend_is_running; then
    for i in {1..100}; do
      backend_is_running || break
      sleep 0.05
    done
  fi
  if backend_is_running; then
    kill "${BACKEND_PID}" 2>/dev/null || true
  fi
  [[ -z "${BACKEND_PID}" ]] || wait "${BACKEND_PID}" 2>/dev/null || true
}

cleanup_session() {
  local status=$?
  trap - EXIT INT TERM
  if backend_is_running; then
    stop_scan_for_cleanup
    (( PROJECTOR_OPEN == 0 )) || cleanup_request '{"id":"cleanup-close-projector","cmd":"close_projector","projector_role":"projector"}'
    (( WINDOW_OPEN == 0 )) || cleanup_request '{"id":"cleanup-close-window","cmd":"close_window","window_role":"projector"}'
    (( LEFT_STREAM_OPEN == 0 )) || cleanup_request '{"id":"cleanup-stop-stream-left","cmd":"stop_stream","role":"left"}'
    (( RIGHT_STREAM_OPEN == 0 )) || cleanup_request '{"id":"cleanup-stop-stream-right","cmd":"stop_stream","role":"right"}'
    (( LEFT_CAMERA_OPEN == 0 )) || cleanup_request '{"id":"cleanup-close-camera-left","cmd":"close_camera","role":"left"}'
    (( RIGHT_CAMERA_OPEN == 0 )) || cleanup_request '{"id":"cleanup-close-camera-right","cmd":"close_camera","role":"right"}'
  fi
  shutdown_backend
  if [[ -n "${WORK_DIR}" ]]; then
    if (( status == 0 && SUCCESS == 1 )) && [[ "${KEEP_WORK_DIR}" != 1 ]]; then
      rm -rf -- "${WORK_DIR}"
    else
      printf '[情報] backend stderr: %s\n' "${BACKEND_ERR}" >&2
      printf '[情報] backend作業directory: %s\n' "${WORK_DIR}" >&2
    fi
  fi
  exit "${status}"
}
trap cleanup_session EXIT
trap 'exit 130' INT TERM

command -v jq >/dev/null || die "jqが必要です"
[[ -x "${BIN}" ]] || die "backendを実行できません: ${BIN}"
[[ -n "${MONITOR_INDEX}" ]] || die "MONITOR_INDEXが必要です"
is_non_negative_integer "${LEFT_CAMERA}" || die "LEFT_CAMERAは0以上の整数にしてください"
is_non_negative_integer "${RIGHT_CAMERA}" || die "RIGHT_CAMERAは0以上の整数にしてください"
[[ "${LEFT_CAMERA}" != "${RIGHT_CAMERA}" ]] || die "LEFT_CAMERAとRIGHT_CAMERAは異なる値にしてください"
is_non_negative_integer "${MONITOR_INDEX}" || die "MONITOR_INDEXは0以上の整数にしてください"
[[ -e "${CALIBRATION_FILE}" ]] || die $'Stereo calibration fileが見つかりません:\n'"${CALIBRATION_FILE}"
[[ -f "${CALIBRATION_FILE}" ]] || die "Stereo calibration pathが通常fileではありません: ${CALIBRATION_FILE}"
[[ -s "${CALIBRATION_FILE}" ]] || die "Stereo calibration fileが空です: ${CALIBRATION_FILE}"
[[ "${SYNC_MODE}" == photodiode ]] || die "SYNC_MODEはphotodiodeにしてください"
[[ -e "${PHOTODIODE_DEVICE}" ]] || die "Photodiodeデバイスが見つかりません: ${PHOTODIODE_DEVICE}"
[[ -r "${PHOTODIODE_DEVICE}" ]] || die "Photodiodeデバイスを読み取れません: ${PHOTODIODE_DEVICE}"
[[ -w "${PHOTODIODE_DEVICE}" ]] || die "Photodiodeデバイスへ書き込めません: ${PHOTODIODE_DEVICE}"
is_positive_integer "${PHOTODIODE_BAUD}" || die "PHOTODIODE_BAUDは正の整数にしてください"
is_positive_integer "${SYNC_TIMEOUT_MS}" || die "SYNC_TIMEOUT_MSは正の整数にしてください"
is_non_negative_integer "${GUARD_MS}" || die "GUARD_MSは0以上の整数にしてください"
[[ "${DEBUG_TIMING}" == 0 || "${DEBUG_TIMING}" == 1 ]] || die "DEBUG_TIMINGは0または1にしてください"
is_positive_integer "${MJPEG_PORT}" || die "MJPEG_PORTは正の整数にしてください"
is_positive_integer "${CODE_WIDTH}" || die "CODE_WIDTHは正の整数にしてください"
is_positive_integer "${CODE_HEIGHT}" || die "CODE_HEIGHTは正の整数にしてください"
is_positive_integer "${READY_TIMEOUT_SECONDS}" || die "READY_TIMEOUT_SECONDSは正の整数にしてください"
if [[ -n "${DISPLAY_WIDTH}" || -n "${DISPLAY_HEIGHT}" ]]; then
  [[ -n "${DISPLAY_WIDTH}" && -n "${DISPLAY_HEIGHT}" ]] || die "DISPLAY_WIDTHとDISPLAY_HEIGHTは両方指定してください"
  is_positive_integer "${DISPLAY_WIDTH}" || die "DISPLAY_WIDTHは正の整数にしてください"
  is_positive_integer "${DISPLAY_HEIGHT}" || die "DISPLAY_HEIGHTは正の整数にしてください"
fi
[[ -z "${DECODE_THRESHOLD}" ]] || is_non_negative_integer "${DECODE_THRESHOLD}" || die "DECODE_THRESHOLDは0以上の整数にしてください"
[[ -z "${MAX_EPIPOLAR_ERROR_PX}" ]] || is_positive_number "${MAX_EPIPOLAR_ERROR_PX}" || die "MAX_EPIPOLAR_ERROR_PXは正の数にしてください"

setup_session() {
printf '[CONFIG]\nleft_camera=%s\nright_camera=%s\nmonitor=%s\ncalibration=%s\nsync=photodiode\n' \
  "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${MONITOR_INDEX}" "${CALIBRATION_FILE}"
printf 'photodiode=%s\nbaud=%s\ntimeout_ms=%s\nguard_ms=%s\noutput=%s\nply=%s\n' \
  "${PHOTODIODE_DEVICE}" "${PHOTODIODE_BAUD}" "${SYNC_TIMEOUT_MS}" "${GUARD_MS}" "${OUTPUT_DIR}" "${PLY_FILE}"

WORK_DIR="$(mktemp -d)"
BACKEND_LOG="${WORK_DIR}/backend.jsonl"
BACKEND_ERR="${WORK_DIR}/backend.stderr.log"
: >"${BACKEND_LOG}"
: >"${BACKEND_ERR}"

coproc BACKEND_PROC {
  "${BIN}" serve --control stdio --mjpeg-host "${MJPEG_HOST}" --mjpeg-port "${MJPEG_PORT}" 2>"${BACKEND_ERR}"
}
BACKEND_PID="${BACKEND_PROC_PID}"
BACKEND_OUTPUT_FD="${BACKEND_PROC[0]}"
BACKEND_INPUT_FD="${BACKEND_PROC[1]}"

wait_for_ready
request '{"id":"ping","cmd":"ping"}'
request '{"id":"monitors","cmd":"list_monitors"}'
MONITOR_COUNT="$(jq -r '(.monitor_count // "0") | tonumber' <<<"${RESPONSE}")"
(( MONITOR_INDEX < MONITOR_COUNT )) || die "MONITOR_INDEX=${MONITOR_INDEX}は範囲外です (monitor_count=${MONITOR_COUNT})"
MONITORS_JSON="$(jq -r '.monitors_json // "[]"' <<<"${RESPONSE}")"
WINDOW_WIDTH="$(jq -r --argjson index "${MONITOR_INDEX}" '.[$index].width' <<<"${MONITORS_JSON}")"
WINDOW_HEIGHT="$(jq -r --argjson index "${MONITOR_INDEX}" '.[$index].height' <<<"${MONITORS_JSON}")"
is_positive_integer "${WINDOW_WIDTH}" || die "モニター幅が不正です"
is_positive_integer "${WINDOW_HEIGHT}" || die "モニター高さが不正です"
printf '[OK] モニターを確認しました: %sx%s\n' "${WINDOW_WIDTH}" "${WINDOW_HEIGHT}"

request "$(jq -cn --argjson camera "${LEFT_CAMERA}" '{id:"camera-left",cmd:"open_camera",camera_id:$camera,role:"left"}')"
LEFT_CAMERA_OPEN=1
printf '[OK] 左カメラを開きました\n'
request "$(jq -cn --argjson camera "${RIGHT_CAMERA}" '{id:"camera-right",cmd:"open_camera",camera_id:$camera,role:"right"}')"
RIGHT_CAMERA_OPEN=1
printf '[OK] 右カメラを開きました\n'

request '{"id":"stream-left","cmd":"start_stream","role":"left"}'
LEFT_STREAM_OPEN=1
printf '[STREAM] left : %s\n' "$(jq -r '.url' <<<"${RESPONSE}")"
request '{"id":"stream-right","cmd":"start_stream","role":"right"}'
RIGHT_STREAM_OPEN=1
printf '[STREAM] right: %s\n' "$(jq -r '.url' <<<"${RESPONSE}")"

request "$(jq -cn --argjson monitor "${MONITOR_INDEX}" --argjson width "${WINDOW_WIDTH}" --argjson height "${WINDOW_HEIGHT}" \
  '{id:"window",cmd:"open_window",window_role:"projector",title:"Projector",width:$width,height:$height,monitor_index:$monitor,fullscreen:true}')"
WINDOW_OPEN=1
printf '[OK] Projector windowを開きました\n'
request "$(jq -cn --argjson width "${CODE_WIDTH}" --argjson height "${CODE_HEIGHT}" \
  '{id:"projector",cmd:"open_projector",projector_role:"projector",window_role:"projector",width:$width,height:$height}')"
PROJECTOR_OPEN=1
printf '[OK] Projectorを開きました\n'

DISPLAY_X=128
DISPLAY_Y=0
(( DISPLAY_WIDTH > 0 && DISPLAY_X + DISPLAY_WIDTH <= WINDOW_WIDTH && DISPLAY_HEIGHT > 0 && DISPLAY_HEIGHT <= WINDOW_HEIGHT )) || \
  die "photodiode_marker_margin_unavailable"
request "$(jq -cn --argjson monitor "${MONITOR_INDEX}" --argjson width "${DISPLAY_WIDTH}" --argjson height "${DISPLAY_HEIGHT}" \
  --argjson x "${DISPLAY_X}" --argjson y "${DISPLAY_Y}" \
  '{id:"surface",cmd:"configure_projector_surface",projector_role:"projector",monitor_index:$monitor,width:$width,height:$height,x:$x,y:$y,placement:"custom"}')"
printf '[OK] 投影surfaceを設定しました\n'

request '{"id":"patterns","cmd":"generate_patterns","projector_role":"projector"}'
printf '[OK] Patternを生成しました\n'
printf '[PROJECTOR] code=%sx%s\n' "$(jq -r '.code_width' <<<"${RESPONSE}")" "$(jq -r '.code_height' <<<"${RESPONSE}")"
printf '[PROJECTOR] display=%sx%s\n' "$(jq -r '.display_width' <<<"${RESPONSE}")" "$(jq -r '.display_height' <<<"${RESPONSE}")"
PATTERN_COUNT="$(jq -r '.pattern_count' <<<"${RESPONSE}")"
(( PATTERN_COUNT >= 2 )) || die "全面白の基準Patternを利用できません"
LOCATOR_PATTERN_INDEX=$((PATTERN_COUNT - 2))
printf '[PATTERN] count=%s\n' "${PATTERN_COUNT}"
show_locator || die "初期locatorを表示できませんでした"
printf '[LOCATOR] marker=(%s,%s) %sx%s\n[LOCATOR] pattern=(%s,%s) %sx%s\n' \
  "$(jq -r '.marker_x' <<<"${RESPONSE}")" "$(jq -r '.marker_y' <<<"${RESPONSE}")" \
  "$(jq -r '.marker_width' <<<"${RESPONSE}")" "$(jq -r '.marker_height' <<<"${RESPONSE}")" \
  "$(jq -r '.pattern_x' <<<"${RESPONSE}")" "$(jq -r '.pattern_y' <<<"${RESPONSE}")" \
  "$(jq -r '.pattern_width' <<<"${RESPONSE}")" "$(jq -r '.pattern_height' <<<"${RESPONSE}")"
}

SESSION_OUTPUT_DIR="${OUTPUT_DIR}"
PLY_BASENAME="$(basename -- "${PLY_FILE}")"
setup_session
interactive_loop || die "対話sessionを継続できません"

request '{"id":"stop-stream-left","cmd":"stop_stream","role":"left"}'
LEFT_STREAM_OPEN=0
request '{"id":"stop-stream-right","cmd":"stop_stream","role":"right"}'
RIGHT_STREAM_OPEN=0
request '{"id":"close-camera-left","cmd":"close_camera","role":"left"}'
LEFT_CAMERA_OPEN=0
request '{"id":"close-camera-right","cmd":"close_camera","role":"right"}'
RIGHT_CAMERA_OPEN=0
SUCCESS=1
