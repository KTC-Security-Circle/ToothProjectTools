#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

TOOTH_BACKEND="${TOOTH_BACKEND:-${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend}"
LEFT_CAMERA="${LEFT_CAMERA:-0}"
RIGHT_CAMERA="${RIGHT_CAMERA:-2}"
MJPEG_HOST="${MJPEG_HOST:-127.0.0.1}"
MJPEG_PORT="${MJPEG_PORT:-39010}"
PATTERN_COUNT="${PATTERN_COUNT:-46}"
PROJECTOR_WIDTH="${PROJECTOR_WIDTH:-1920}"
PROJECTOR_HEIGHT="${PROJECTOR_HEIGHT:-1080}"
PATTERN_SETTLE_SECONDS="${PATTERN_SETTLE_SECONDS:-0.25}"
PATTERN_ADVANCE_COMMAND="${PATTERN_ADVANCE_COMMAND:-}"

OUTPUT_DIR="${1:-}"
[[ -n "${OUTPUT_DIR}" ]] || {
  echo "Usage: $0 OUTPUT_DIR" >&2
  exit 2
}
mkdir -p "${OUTPUT_DIR}/left" "${OUTPUT_DIR}/right"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"

WORK_DIR="$(mktemp -d)"
FIFO_IN="${WORK_DIR}/backend.in"
BACKEND_LOG="${WORK_DIR}/backend.log"
BACKEND_PID=""

die() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }
ok() { printf '[OK] %s\n' "$*"; }

cleanup() {
  if [[ -n "${BACKEND_PID}" ]] && kill -0 "${BACKEND_PID}" 2>/dev/null; then
    kill "${BACKEND_PID}" 2>/dev/null || true
    wait "${BACKEND_PID}" 2>/dev/null || true
  fi
  rm -rf -- "${WORK_DIR}"
}
trap cleanup EXIT

wait_for_pattern() {
  local pattern="$1"
  local start
  start="$(date +%s)"
  while true; do
    grep -Fq -- "${pattern}" "${BACKEND_LOG}" && return 0
    (( $(date +%s) - start < 20 )) || die "timeout: ${pattern}"
    sleep 0.1
  done
}

send_json() { printf '%s\n' "$1" >&3; }

check_ok() {
  local id="$1"
  wait_for_pattern "\"id\":\"${id}\""
  local line
  line="$(grep -F "\"id\":\"${id}\"" "${BACKEND_LOG}" | tail -1)"
  grep -Fq '"ok":true' <<<"${line}" || die "${id}: ${line}"
}

advance_pattern() {
  local index="$1"
  if [[ -n "${PATTERN_ADVANCE_COMMAND}" ]]; then
    local command="${PATTERN_ADVANCE_COMMAND//\{index\}/${index}}"
    bash -lc "${command}"
  else
    printf 'Pattern %03d を表示し、安定したらEnterを押してください: ' "${index}"
    read -r _
  fi
  sleep "${PATTERN_SETTLE_SECONDS}"
}

[[ -x "${TOOTH_BACKEND}" ]] || die "backend not executable: ${TOOTH_BACKEND}"
rm -f "${OUTPUT_DIR}/left"/pattern_*.png "${OUTPUT_DIR}/right"/pattern_*.png

mkfifo "${FIFO_IN}"
"${TOOTH_BACKEND}" serve \
  --control stdio \
  --mjpeg-host "${MJPEG_HOST}" \
  --mjpeg-port "${MJPEG_PORT}" \
  <"${FIFO_IN}" >"${BACKEND_LOG}" 2>&1 &
BACKEND_PID="$!"
exec 3>"${FIFO_IN}"

wait_for_pattern '"event":"ready"'
send_json "{\"id\":\"scan-open-left\",\"cmd\":\"open_camera\",\"camera_id\":${LEFT_CAMERA},\"role\":\"left\"}"
check_ok "scan-open-left"
send_json "{\"id\":\"scan-open-right\",\"cmd\":\"open_camera\",\"camera_id\":${RIGHT_CAMERA},\"role\":\"right\"}"
check_ok "scan-open-right"

for ((index=0; index<PATTERN_COUNT; index++)); do
  printf -v sequence '%03d' "${index}"
  advance_pattern "${index}"

  id="scan-capture-${sequence}"
  send_json "{\"id\":\"${id}\",\"cmd\":\"capture_stereo\",\"left_role\":\"left\",\"right_role\":\"right\",\"left_output\":\"${OUTPUT_DIR}/left/pattern_${sequence}.png\",\"right_output\":\"${OUTPUT_DIR}/right/pattern_${sequence}.png\"}"
  check_ok "${id}"

  [[ -s "${OUTPUT_DIR}/left/pattern_${sequence}.png" ]] ||
    die "left capture missing: ${sequence}"
  [[ -s "${OUTPUT_DIR}/right/pattern_${sequence}.png" ]] ||
    die "right capture missing: ${sequence}"

  ok "captured pattern ${sequence}"
done

cat >"${OUTPUT_DIR}/metadata.json" <<EOF
{
  "version": "0.1.0",
  "pattern_count": ${PATTERN_COUNT},
  "projector_width": ${PROJECTOR_WIDTH},
  "projector_height": ${PROJECTOR_HEIGHT}
}
EOF

send_json '{"id":"scan-close-left","cmd":"close_camera","role":"left"}'
check_ok "scan-close-left"
send_json '{"id":"scan-close-right","cmd":"close_camera","role":"right"}'
check_ok "scan-close-right"
send_json '{"id":"scan-shutdown","cmd":"shutdown"}'
check_ok "scan-shutdown"

wait "${BACKEND_PID}" || true
BACKEND_PID=""

printf '\nPASS: structured light scan capture\n'
printf 'Dataset: %s\n' "${OUTPUT_DIR}"
