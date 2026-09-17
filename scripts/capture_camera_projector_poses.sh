#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

TOOTH_BACKEND="${TOOTH_BACKEND:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
CAMERA_ID="${CAMERA_ID:-0}"
CAMERA_ROLE="${CAMERA_ROLE:-left}"

MONITOR_INDEX="${MONITOR_INDEX:-0}"
WINDOW_WIDTH="${WINDOW_WIDTH:-}"
WINDOW_HEIGHT="${WINDOW_HEIGHT:-}"
DISPLAY_WIDTH="${DISPLAY_WIDTH:-}"
DISPLAY_HEIGHT="${DISPLAY_HEIGHT:-}"

PROJECTOR_ROLE="${PROJECTOR_ROLE:-projector}"
WINDOW_ROLE="${WINDOW_ROLE:-projector}"
CODE_WIDTH="${CODE_WIDTH:-480}"
CODE_HEIGHT="${CODE_HEIGHT:-270}"

SCAN_SETTLE_MS="${SCAN_SETTLE_MS:-120}"
REFERENCE_SETTLE_SEC="${REFERENCE_SETTLE_SEC:-0.20}"
DECODE_THRESHOLD="${DECODE_THRESHOLD:-10}"
SCAN_TIMEOUT_SEC="${SCAN_TIMEOUT_SEC:-120}"
DECODE_TIMEOUT_SEC="${DECODE_TIMEOUT_SEC:-60}"

MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39010}"

OBSERVATIONS_DIR="${OBSERVATIONS_DIR:-${REPO_ROOT}/data/calib/camera_projector/observations}"
KEEP_WORK_DIR="${KEEP_WORK_DIR:-0}"

WORK_DIR="$(mktemp -d)"
FIFO_IN="${WORK_DIR}/backend.in"
BACKEND_LOG="${WORK_DIR}/backend.jsonl"
BACKEND_ERR="${WORK_DIR}/backend.stderr.log"
BACKEND_PID=""

LAST_RESPONSE=""

log()  { printf '[INFO] %s\n' "$*"; }
ok()   { printf '[OK] %s\n' "$*"; }
warn() { printf '[WARN] %s\n' "$*" >&2; }
err()  { printf '[ERROR] %s\n' "$*" >&2; }
die()  { err "$*"; exit 1; }

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

cleanup() {
  local status=$?

  exec 3>&- 2>/dev/null || true

  if [[ -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null; then
    kill "${BACKEND_PID}" 2>/dev/null || true
    wait "${BACKEND_PID}" 2>/dev/null || true
  fi

  if [[ "${KEEP_WORK_DIR}" == "1" || "${status}" -ne 0 ]]; then
    printf '[INFO] backend logs: %s\n' "${WORK_DIR}" >&2
  else
    rm -rf -- "${WORK_DIR}"
  fi

  return "${status}"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

send_json() {
  printf '%s\n' "$1" >&3
}

wait_for_ready() {
  local start
  start="$(date +%s)"

  while true; do
    if grep -Fq '"event":"ready"' "${BACKEND_LOG}" 2>/dev/null; then
      return 0
    fi

    if [[ -n "${BACKEND_PID}" ]] && ! kill -0 "${BACKEND_PID}" 2>/dev/null; then
      err "backend exited before ready"
      [[ -s "${BACKEND_ERR}" ]] && tail -n 30 "${BACKEND_ERR}" >&2
      return 1
    fi

    if (( $(date +%s) - start >= 15 )); then
      err "timeout waiting for backend ready"
      [[ -s "${BACKEND_ERR}" ]] && tail -n 30 "${BACKEND_ERR}" >&2
      return 1
    fi

    sleep 0.05
  done
}

wait_response() {
  local id="$1"
  local timeout="${2:-15}"
  local start line code message
  start="$(date +%s)"

  while true; do
    line="$(grep -F "\"id\":\"${id}\"" "${BACKEND_LOG}" 2>/dev/null | tail -n 1 || true)"
    if [[ -n "${line}" ]]; then
      LAST_RESPONSE="${line}"

      if jq -e '.ok == true' >/dev/null 2>&1 <<<"${line}"; then
        return 0
      fi

      code="$(jq -r '.error.code // "unknown_error"' <<<"${line}" 2>/dev/null || printf 'unknown_error')"
      message="$(jq -r '.error.message // "command failed"' <<<"${line}" 2>/dev/null || printf 'command failed')"
      err "${id}: ${code}: ${message}"
      return 1
    fi

    if [[ -n "${BACKEND_PID}" ]] && ! kill -0 "${BACKEND_PID}" 2>/dev/null; then
      err "backend exited while waiting for response: ${id}"
      return 1
    fi

    if (( $(date +%s) - start >= timeout )); then
      err "timeout waiting for response: ${id}"
      return 1
    fi

    sleep 0.05
  done
}

request() {
  local id="$1"
  local json="$2"
  local timeout="${3:-15}"
  send_json "${json}"
  wait_response "${id}" "${timeout}"
}

wait_scan_terminal() {
  local scan_id="$1"
  local start completed failed progress captured total
  local last_progress=""
  start="$(date +%s)"

  while true; do
    failed="$(
      grep -F '"event":"scan_failed"' "${BACKEND_LOG}" 2>/dev/null |
        grep -F "\"scan_id\":\"${scan_id}\"" |
        tail -n 1 || true
    )"
    if [[ -n "${failed}" ]]; then
      printf '\n' >&2
      err "scan failed: $(jq -r '.error_code // "unknown_error"' <<<"${failed}") - $(jq -r '.error_message // ""' <<<"${failed}")"
      return 1
    fi

    completed="$(
      grep -F '"event":"scan_completed"' "${BACKEND_LOG}" 2>/dev/null |
        grep -F "\"scan_id\":\"${scan_id}\"" |
        tail -n 1 || true
    )"
    if [[ -n "${completed}" ]]; then
      captured="$(jq -r '.captured_count // "?"' <<<"${completed}")"
      total="$(jq -r '.pattern_count // "?"' <<<"${completed}")"
      printf '\r[SCAN] %s/%s frames\n' "${captured}" "${total}"
      return 0
    fi

    progress="$(
      grep -F '"event":"scan_frame_captured"' "${BACKEND_LOG}" 2>/dev/null |
        grep -F "\"scan_id\":\"${scan_id}\"" |
        tail -n 1 || true
    )"
    if [[ -n "${progress}" ]]; then
      captured="$(jq -r '.captured_count // "?"' <<<"${progress}")"
      total="$(jq -r '.pattern_count // "?"' <<<"${progress}")"
      if [[ "${captured}/${total}" != "${last_progress}" ]]; then
        printf '\r[SCAN] %s/%s frames' "${captured}" "${total}"
        last_progress="${captured}/${total}"
      fi
    fi

    if (( $(date +%s) - start >= SCAN_TIMEOUT_SEC )); then
      printf '\n' >&2
      err "scan timeout: ${scan_id}"
      return 1
    fi

    sleep 0.05
  done
}

find_next_pose_index() {
  local i=1
  local name

  while true; do
    printf -v name 'pose_%03d' "${i}"
    if [[ ! -e "${OBSERVATIONS_DIR}/${name}" && ! -e "${OBSERVATIONS_DIR}/.${name}.tmp" ]]; then
      printf '%d' "${i}"
      return 0
    fi
    ((i += 1))
  done
}

show_black_pattern() {
  local id="$1"
  local black_index="$2"
  local json

  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg role "${PROJECTOR_ROLE}" \
      --argjson index "${black_index}" \
      '{id:$id,cmd:"show_pattern",projector_role:$role,index:$index}'
  )"

  request "${id}" "${json}"
}

capture_pose() {
  local pose_index="$1"
  local black_index="$2"

  local pose_name tmp_dir final_dir failed_dir scan_id
  local id json

  printf -v pose_name 'pose_%03d' "${pose_index}"
  tmp_dir="${OBSERVATIONS_DIR}/.${pose_name}.tmp"
  final_dir="${OBSERVATIONS_DIR}/${pose_name}"
  scan_id="cp_${pose_name}"

  rm -rf -- "${tmp_dir}"
  mkdir -p -- "${tmp_dir}"

  log "${pose_name}: reference_before"

  id="${pose_name}-black-before"
  show_black_pattern "${id}" "${black_index}" || return 1
  sleep "${REFERENCE_SETTLE_SEC}"

  id="${pose_name}-before"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg role "${CAMERA_ROLE}" \
      --arg output "${tmp_dir}/reference_before.png" \
      '{id:$id,cmd:"capture_frame",role:$role,output:$output}'
  )"
  request "${id}" "${json}" || return 1

  log "${pose_name}: Gray Code scan"

  id="${pose_name}-scan"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg scan_id "${scan_id}" \
      --arg projector_role "${PROJECTOR_ROLE}" \
      --arg left_role "${CAMERA_ROLE}" \
      --arg output_dir "${tmp_dir}/scan" \
      --argjson settle_ms "${SCAN_SETTLE_MS}" \
      '{
        id:$id,
        cmd:"scan_start",
        scan_id:$scan_id,
        projector_role:$projector_role,
        left_role:$left_role,
        output_dir:$output_dir,
        settle_ms:$settle_ms,
        sync_source:"fixed_delay"
      }'
  )"
  request "${id}" "${json}" || return 1
  wait_scan_terminal "${scan_id}" || return 1

  log "${pose_name}: reference_after"

  # scan sequence末尾は全黒。明示的に再表示してbefore/after条件を揃える。
  id="${pose_name}-black-after"
  show_black_pattern "${id}" "${black_index}" || return 1
  sleep "${REFERENCE_SETTLE_SEC}"

  id="${pose_name}-after"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg role "${CAMERA_ROLE}" \
      --arg output "${tmp_dir}/reference_after.png" \
      '{id:$id,cmd:"capture_frame",role:$role,output:$output}'
  )"
  request "${id}" "${json}" || return 1

  log "${pose_name}: decode"

  id="${pose_name}-decode"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg input_dir "${tmp_dir}/scan" \
      --arg output_dir "${tmp_dir}/decode" \
      --argjson threshold "${DECODE_THRESHOLD}" \
      '{
        id:$id,
        cmd:"decode_patterns",
        input_dir:$input_dir,
        output_dir:$output_dir,
        threshold:$threshold,
        allow_partial:false
      }'
  )"
  request "${id}" "${json}" "${DECODE_TIMEOUT_SEC}" || return 1

  local required=(
    "${tmp_dir}/reference_before.png"
    "${tmp_dir}/reference_after.png"
    "${tmp_dir}/scan/metadata.json"
    "${tmp_dir}/decode/metadata.json"
    "${tmp_dir}/decode/left/projector_x.yml"
    "${tmp_dir}/decode/left/projector_y.yml"
    "${tmp_dir}/decode/left/valid_mask.png"
  )
  local path
  for path in "${required[@]}"; do
    [[ -s "${path}" ]] || {
      err "${pose_name}: required artifact missing or empty: ${path}"
      return 1
    }
  done

  mv -- "${tmp_dir}" "${final_dir}"
  ok "${pose_name} saved: ${final_dir}"
  return 0
}

initialize_runtime() {
  local id json monitor_count monitors_json
  local pattern_count black_index

  id="init-ping"
  request "${id}" "$(jq -cn --arg id "${id}" '{id:$id,cmd:"ping"}')" ||
    die "ping failed"

  id="init-monitors"
  request "${id}" "$(jq -cn --arg id "${id}" '{id:$id,cmd:"list_monitors"}')" ||
    die "list_monitors failed"

  monitor_count="$(jq -r '(.monitor_count // "0") | tonumber' <<<"${LAST_RESPONSE}")"
  (( monitor_count > 0 )) || die "no monitor detected"
  (( MONITOR_INDEX >= 0 && MONITOR_INDEX < monitor_count )) ||
    die "MONITOR_INDEX=${MONITOR_INDEX} is out of range (monitor_count=${monitor_count})"

  monitors_json="$(jq -r '.monitors_json // "[]"' <<<"${LAST_RESPONSE}")"

  if [[ -z "${WINDOW_WIDTH}" ]]; then
    WINDOW_WIDTH="$(jq -r --argjson i "${MONITOR_INDEX}" '.[$i].width' <<<"${monitors_json}")"
  fi
  if [[ -z "${WINDOW_HEIGHT}" ]]; then
    WINDOW_HEIGHT="$(jq -r --argjson i "${MONITOR_INDEX}" '.[$i].height' <<<"${monitors_json}")"
  fi
  if [[ -z "${DISPLAY_WIDTH}" ]]; then
    DISPLAY_WIDTH="${WINDOW_WIDTH}"
  fi
  if [[ -z "${DISPLAY_HEIGHT}" ]]; then
    DISPLAY_HEIGHT="${WINDOW_HEIGHT}"
  fi

  log "camera=${CAMERA_ID}, monitor=${MONITOR_INDEX}, window=${WINDOW_WIDTH}x${WINDOW_HEIGHT}, code=${CODE_WIDTH}x${CODE_HEIGHT}, display=${DISPLAY_WIDTH}x${DISPLAY_HEIGHT}"

  id="init-camera"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg role "${CAMERA_ROLE}" \
      --argjson camera_id "${CAMERA_ID}" \
      '{id:$id,cmd:"open_camera",camera_id:$camera_id,role:$role}'
  )"
  request "${id}" "${json}" || die "open_camera failed"

  id="init-window"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg role "${WINDOW_ROLE}" \
      --argjson width "${WINDOW_WIDTH}" \
      --argjson height "${WINDOW_HEIGHT}" \
      --argjson monitor_index "${MONITOR_INDEX}" \
      '{
        id:$id,
        cmd:"open_window",
        window_role:$role,
        title:"Camera Projector Calibration",
        width:$width,
        height:$height,
        monitor_index:$monitor_index,
        fullscreen:true
      }'
  )"
  request "${id}" "${json}" || die "open_window failed"

  id="init-projector"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg projector_role "${PROJECTOR_ROLE}" \
      --arg window_role "${WINDOW_ROLE}" \
      --argjson width "${CODE_WIDTH}" \
      --argjson height "${CODE_HEIGHT}" \
      '{
        id:$id,
        cmd:"open_projector",
        projector_role:$projector_role,
        window_role:$window_role,
        width:$width,
        height:$height
      }'
  )"
  request "${id}" "${json}" || die "open_projector failed"

  id="init-surface"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg projector_role "${PROJECTOR_ROLE}" \
      --argjson monitor_index "${MONITOR_INDEX}" \
      --argjson width "${DISPLAY_WIDTH}" \
      --argjson height "${DISPLAY_HEIGHT}" \
      '{
        id:$id,
        cmd:"configure_projector_surface",
        projector_role:$projector_role,
        monitor_index:$monitor_index,
        width:$width,
        height:$height,
        placement:"center"
      }'
  )"
  request "${id}" "${json}" || die "configure_projector_surface failed"

  id="init-patterns"
  json="$(
    jq -cn \
      --arg id "${id}" \
      --arg projector_role "${PROJECTOR_ROLE}" \
      '{id:$id,cmd:"generate_patterns",projector_role:$projector_role}'
  )"
  request "${id}" "${json}" || die "generate_patterns failed"

  pattern_count="$(jq -r '(.pattern_count // "0") | tonumber' <<<"${LAST_RESPONSE}")"
  (( pattern_count >= 2 )) || die "invalid generated pattern_count: ${pattern_count}"

  black_index=$((pattern_count - 1))
  show_black_pattern "init-black" "${black_index}" || die "failed to show black pattern"

  BLACK_INDEX="${black_index}"
}

main() {
  local cmd black_index pose_index pose_name key failed_dir

  for cmd in jq mkfifo grep tail date sleep; do
    require_command "${cmd}"
  done

  [[ -x "${TOOTH_BACKEND}" ]] || die "backend not executable: ${TOOTH_BACKEND}"
  [[ -r /dev/tty && -w /dev/tty ]] || die "/dev/tty is required for single-key input"

  mkdir -p -- "${OBSERVATIONS_DIR}"
  mkfifo "${FIFO_IN}"

  "${TOOTH_BACKEND}" \
    serve \
    --control stdio \
    --mjpeg-host "${MJPEG_HOST}" \
    --mjpeg-port "${MJPEG_PORT}" \
    <"${FIFO_IN}" >"${BACKEND_LOG}" 2>"${BACKEND_ERR}" &
  BACKEND_PID="$!"

  exec 3>"${FIFO_IN}"

  wait_for_ready || die "backend did not become ready"
  ok "backend ready"

  initialize_runtime
  black_index="${BLACK_INDEX}"
  pose_index="$(find_next_pose_index)"

  while true; do
    printf -v pose_name 'pose_%03d' "${pose_index}"
    printf '\n[SPACE] %s を撮影    [q] 終了\n' "${pose_name}"

    IFS= read -rsn1 key </dev/tty || break

    case "${key}" in
      q|Q)
        break
        ;;
      ' ')
        printf '\n'
        if capture_pose "${pose_index}" "${black_index}"; then
          ((pose_index += 1))
        else
          printf -v pose_name 'pose_%03d' "${pose_index}"
          failed_dir="${OBSERVATIONS_DIR}/.${pose_name}.failed.$(date +%Y%m%d_%H%M%S)"
          if [[ -d "${OBSERVATIONS_DIR}/.${pose_name}.tmp" ]]; then
            mv -- "${OBSERVATIONS_DIR}/.${pose_name}.tmp" "${failed_dir}"
            warn "failed pose kept outside calibration input: ${failed_dir}"
          fi
          warn "${pose_name} was not accepted. Press SPACE to retry the same pose number."
        fi
        ;;
      *)
        ;;
    esac
  done

  log "shutting down"

  id="shutdown"
  if request "${id}" "$(jq -cn --arg id "${id}" '{id:$id,cmd:"shutdown"}')"; then
    exec 3>&-
    wait "${BACKEND_PID}" 2>/dev/null || true
    BACKEND_PID=""
  fi

  ok "finished"
}

main "$@"
