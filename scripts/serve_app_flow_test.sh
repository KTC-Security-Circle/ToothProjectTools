#!/usr/bin/env bash
set -Eeuo pipefail

BIN="${BIN:-./build/src/serve/tooth-backend}"
MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39010}"
LEFT_CAMERA="${LEFT_CAMERA:-0}"
RIGHT_CAMERA="${RIGHT_CAMERA:-2}"
OUT_DIR="${OUT_DIR:-./data/serve_app_test}"
TIMEOUT_SEC="${TIMEOUT_SEC:-5}"

STDERR_LOG=""
SIDECAR_PID=""
SIDECAR_IN=""
SIDECAR_OUT=""

PENDING_LINES=()
LAST_JSON=""

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "[FAIL] required command not found: $1" >&2
    exit 1
  }
}

die() {
  echo "[FAIL] $*" >&2

  if [[ -n "${LAST_JSON:-}" ]]; then
    echo "---- last json ----" >&2
    echo "${LAST_JSON}" >&2
    echo "-------------------" >&2
  fi

  if [[ -n "${STDERR_LOG:-}" && -f "${STDERR_LOG}" ]]; then
    echo "---- stderr tail ----" >&2
    tail -n 80 "${STDERR_LOG}" >&2 || true
    echo "---------------------" >&2
  fi

  exit 1
}

cleanup() {
  if [[ -n "${SIDECAR_IN:-}" ]]; then
    exec {SIDECAR_IN}>&- || true
  fi

  if [[ -n "${SIDECAR_OUT:-}" ]]; then
    exec {SIDECAR_OUT}<&- || true
  fi

  if [[ -n "${SIDECAR_PID:-}" ]] && kill -0 "${SIDECAR_PID}" >/dev/null 2>&1; then
    kill "${SIDECAR_PID}" >/dev/null 2>&1 || true
    sleep 0.2
    kill -9 "${SIDECAR_PID}" >/dev/null 2>&1 || true
  fi

  if [[ -n "${STDERR_LOG:-}" && -f "${STDERR_LOG}" ]]; then
    rm -f "${STDERR_LOG}"
  fi
}

trap cleanup EXIT

json_match() {
  local line="$1"
  local filter="$2"

  jq -e "${filter}" >/dev/null 2>&1 <<<"${line}"
}

wait_json() {
  local filter="$1"
  local label="$2"
  local deadline=$((SECONDS + TIMEOUT_SEC))

  while true; do
    for i in "${!PENDING_LINES[@]}"; do
      local pending="${PENDING_LINES[$i]}"

      if json_match "${pending}" "${filter}"; then
        unset 'PENDING_LINES[i]'
        LAST_JSON="${pending}"
        echo "< ${pending}" >&2
        echo "[OK] ${label}" >&2
        return 0
      fi
    done

    (( SECONDS < deadline )) || die "timeout waiting for ${label}: ${filter}"

    local remain=$((deadline - SECONDS))
    local line=""

    if ! IFS= read -r -t "${remain}" line <&"${SIDECAR_OUT}"; then
      die "timeout waiting for ${label}: ${filter}"
    fi

    [[ -n "${line}" ]] || continue

    if ! jq -e . >/dev/null 2>&1 <<<"${line}"; then
      echo "[WARN] non-json stdout: ${line}" >&2
      continue
    fi

    if json_match "${line}" "${filter}"; then
      LAST_JSON="${line}"
      echo "< ${line}" >&2
      echo "[OK] ${label}" >&2
      return 0
    fi

    echo "<skip ${line}" >&2
    PENDING_LINES+=("${line}")
  done
}

send_json() {
  local json="$1"

  [[ -n "${SIDECAR_PID:-}" ]] || die "sidecar is not started"
  kill -0 "${SIDECAR_PID}" >/dev/null 2>&1 || die "sidecar process is not running"

  echo "> ${json}" >&2
  printf '%s\n' "${json}" >&"${SIDECAR_IN}"
}

request_ok() {
  local id="$1"
  local json="$2"

  send_json "${json}"
  wait_json ".id == \"${id}\" and .ok == true" "ok response id=${id}"
}

request_error() {
  local id="$1"
  local code="$2"
  local json="$3"

  send_json "${json}"
  wait_json ".id == \"${id}\" and .ok == false and .error.code == \"${code}\"" \
    "error response id=${id} code=${code}"
}

expect_event() {
  local event="$1"
  local filter="${2:-.event == \"${event}\"}"

  wait_json "${filter}" "event ${event}"
}

assert_last_json() {
  local filter="$1"
  local label="$2"

  jq -e "${filter}" >/dev/null 2>&1 <<<"${LAST_JSON}" \
    || die "assertion failed: ${label}; filter=${filter}"
}

start_sidecar() {
  [[ -x "${BIN}" ]] || die "binary is not executable: ${BIN}"

  STDERR_LOG="$(mktemp -t serve-app-test-stderr.XXXXXX.log)"

  coproc SIDECAR_PROC {
    "${BIN}" serve \
      --control stdio \
      --mjpeg-host "${MJPEG_HOST}" \
      --mjpeg-port "${MJPEG_PORT}" \
      2>"${STDERR_LOG}"
  }

  SIDECAR_PID="${SIDECAR_PROC_PID}"
  exec {SIDECAR_OUT}<&"${SIDECAR_PROC[0]}"
  exec {SIDECAR_IN}>&"${SIDECAR_PROC[1]}"

  wait_json '.event == "ready"' "ready event"
  assert_last_json '.version == "0.1.0"' "ready version"
}

shutdown_sidecar() {
  if [[ -n "${SIDECAR_PID:-}" ]] && kill -0 "${SIDECAR_PID}" >/dev/null 2>&1; then
    send_json '{"id":"999","cmd":"shutdown"}'
    wait_json '.id == "999" and .ok == true' "shutdown response" || true
    wait "${SIDECAR_PID}" || true
  fi
}

test_ping() {
  request_ok "1" '{"id":"1","cmd":"ping"}'
  assert_last_json '.result == "pong"' "ping result is pong"
}

test_open_camera() {
  local id="$1"
  local role="$2"
  local camera_id="$3"

  request_ok "${id}" \
    "{\"id\":\"${id}\",\"cmd\":\"open_camera\",\"camera_id\":${camera_id},\"role\":\"${role}\"}"

  expect_event "camera_opened" \
    ".event == \"camera_opened\" and .role == \"${role}\""

  assert_last_json ".role == \"${role}\"" "camera_opened role=${role}"
}

test_close_camera() {
  local id="$1"
  local role="$2"

  request_ok "${id}" \
    "{\"id\":\"${id}\",\"cmd\":\"close_camera\",\"role\":\"${role}\"}"
}

test_start_stream() {
  local id="$1"
  local role="$2"

  request_ok "${id}" \
    "{\"id\":\"${id}\",\"cmd\":\"start_stream\",\"role\":\"${role}\"}"

  assert_last_json ".url == \"http://${MJPEG_HOST}:${MJPEG_PORT}/${role}.mjpg\"" \
    "start_stream url role=${role}"

  expect_event "stream_started" \
    ".event == \"stream_started\" and .role == \"${role}\""

  assert_last_json ".url == \"http://${MJPEG_HOST}:${MJPEG_PORT}/${role}.mjpg\"" \
    "stream_started url role=${role}"
}

test_stop_stream() {
  local id="$1"
  local role="$2"

  request_ok "${id}" \
    "{\"id\":\"${id}\",\"cmd\":\"stop_stream\",\"role\":\"${role}\"}"
}

test_capture_frame() {
  local id="$1"
  local role="$2"
  local output="$3"

  request_ok "${id}" \
    "{\"id\":\"${id}\",\"cmd\":\"capture_frame\",\"role\":\"${role}\",\"output\":\"${output}\"}"

  assert_last_json ".path == \"${output}\"" "capture_frame path role=${role}"

  expect_event "frame_saved" \
    ".event == \"frame_saved\" and .role == \"${role}\" and .path == \"${output}\""
}

test_calib_capture_frame() {
  local id="$1"
  local role="$2"
  local output="$3"

  request_ok "${id}" \
    "{\"id\":\"${id}\",\"cmd\":\"calib_capture_frame\",\"role\":\"${role}\",\"output\":\"${output}\"}"

  assert_last_json ".path == \"${output}\" and .purpose == \"calibration\"" \
    "calib_capture_frame path/purpose role=${role}"

  expect_event "calibration_frame_saved" \
    ".event == \"calibration_frame_saved\" and .role == \"${role}\" and .path == \"${output}\""
}

test_capture_stereo() {
  local id="$1"
  local left_output="$2"
  local right_output="$3"

  request_ok "${id}" \
    "{\"id\":\"${id}\",\"cmd\":\"capture_stereo\",\"left_role\":\"left\",\"right_role\":\"right\",\"left_output\":\"${left_output}\",\"right_output\":\"${right_output}\"}"

  assert_last_json ".left_path == \"${left_output}\" and .right_path == \"${right_output}\"" \
    "capture_stereo paths"

  expect_event "stereo_frame_saved" \
    ".event == \"stereo_frame_saved\" and .left_role == \"left\" and .right_role == \"right\" and .left_path == \"${left_output}\" and .right_path == \"${right_output}\""
}

test_calib_capture_stereo() {
  local id="$1"
  local left_output="$2"
  local right_output="$3"

  request_ok "${id}" \
    "{\"id\":\"${id}\",\"cmd\":\"calib_capture_stereo\",\"left_role\":\"left\",\"right_role\":\"right\",\"left_output\":\"${left_output}\",\"right_output\":\"${right_output}\"}"

  assert_last_json ".left_path == \"${left_output}\" and .right_path == \"${right_output}\" and .purpose == \"calibration\"" \
    "calib_capture_stereo paths/purpose"

  expect_event "calibration_stereo_frame_saved" \
    ".event == \"calibration_stereo_frame_saved\" and .left_role == \"left\" and .right_role == \"right\" and .left_path == \"${left_output}\" and .right_path == \"${right_output}\""
}

run_flow_test() {
  mkdir -p "${OUT_DIR}"

  echo "[FLOW] ping" >&2
  test_ping

  echo "[FLOW] left_open / right_open" >&2
  test_open_camera "2" "left" "${LEFT_CAMERA}"
  test_open_camera "3" "right" "${RIGHT_CAMERA}"

  echo "[FLOW] left_close / right_close" >&2
  test_close_camera "4" "left"
  test_close_camera "5" "right"

  echo "[FLOW] left_open / right_open again" >&2
  test_open_camera "6" "left" "${LEFT_CAMERA}"
  test_open_camera "7" "right" "${RIGHT_CAMERA}"

  echo "[FLOW] left_stream_open / right_stream_open" >&2
  test_start_stream "8" "left"
  test_start_stream "9" "right"

  echo "[FLOW] left_stream_close / right_stream_close" >&2
  test_stop_stream "10" "left"
  test_stop_stream "11" "right"

  echo "[FLOW] left_stream_open / right_stream_open again" >&2
  test_start_stream "12" "left"
  test_start_stream "13" "right"

  echo "[FLOW] left_capture_frame / right_capture_frame" >&2
  test_capture_frame "14" "left" "${OUT_DIR}/mono_left/001.png"
  test_capture_frame "15" "right" "${OUT_DIR}/mono_right/001.png"

  echo "[FLOW] left_calib_capture_frame / right_calib_capture_frame" >&2
  test_calib_capture_frame "16" "left" "${OUT_DIR}/calib/mono_left/001.png"
  test_calib_capture_frame "17" "right" "${OUT_DIR}/calib/mono_right/001.png"

  echo "[FLOW] capture_stereo" >&2
  test_capture_stereo "18" \
    "${OUT_DIR}/stereo/left_001.png" \
    "${OUT_DIR}/stereo/right_001.png"

  echo "[FLOW] calib_capture_stereo" >&2
  test_calib_capture_stereo "19" \
    "${OUT_DIR}/calib/stereo/left_001.png" \
    "${OUT_DIR}/calib/stereo/right_001.png"


  echo "[FLOW] calibration command validation" >&2
  request_error "camera-missing-id" "missing_field" \
    '{"id":"camera-missing-id","cmd":"open_camera","role":"left"}'
  request_error "camera-missing-role" "missing_field" \
    '{"id":"camera-missing-role","cmd":"open_camera","camera_id":0}'
  request_error "close-missing-role" "missing_field" \
    '{"id":"close-missing-role","cmd":"close_camera"}'

  request_error "30" "missing_field"     '{"id":"30","cmd":"mono_calibrate","role":"left"}'
  request_error "31" "missing_field"     '{"id":"31","cmd":"stereo_calibrate","left_role":"left","right_role":"right"}'
  request_error "32" "invalid_command"     '{"id":"32","cmd":"stereo_calibrate","left_role":"left","right_role":"right","left_dir":"./data/calib/same","right_dir":"./data/calib/same","output_file":"./data/calib/stereo.yml"}'

  echo "[FLOW] shutdown" >&2
  shutdown_sidecar

  echo "[OK] serve app integration flow passed" >&2
}

main() {
  need_cmd jq

  start_sidecar
  run_flow_test
}

main "$@"