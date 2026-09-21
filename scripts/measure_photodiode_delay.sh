#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE:-/dev/ttyUSB0}"
PHOTODIODE_BAUD="${PHOTODIODE_BAUD:-115200}"
CAMERA_ID="${CAMERA_ID:-0}"
TRANSITIONS="${TRANSITIONS:-60}"
SAFETY_MARGIN_MS="${SAFETY_MARGIN_MS:-5}"
OUTPUT_CSV="${OUTPUT_CSV:-${SCRIPT_DIR}/../data/photodiode_delay.csv}"
PROJECTOR_X="${PROJECTOR_X:-0}"
PROJECTOR_Y="${PROJECTOR_Y:-0}"
PROJECTOR_WIDTH="${PROJECTOR_WIDTH:-1920}"
PROJECTOR_HEIGHT="${PROJECTOR_HEIGHT:-1080}"

[[ -e "${PHOTODIODE_DEVICE}" ]] || { printf 'device not found: %s\n' "${PHOTODIODE_DEVICE}" >&2; exit 2; }
[[ -r "${PHOTODIODE_DEVICE}" && -w "${PHOTODIODE_DEVICE}" ]] || { printf 'permission denied: %s\n' "${PHOTODIODE_DEVICE}" >&2; exit 3; }
command -v python3 >/dev/null || { printf 'python3 is required\n' >&2; exit 4; }
mkdir -p -- "$(dirname -- "${OUTPUT_CSV}")"

exec python3 "${SCRIPT_DIR}/measure_photodiode_delay.py" \
  --device "${PHOTODIODE_DEVICE}" --baud "${PHOTODIODE_BAUD}" --camera "${CAMERA_ID}" \
  --transitions "${TRANSITIONS}" --safety-margin-ms "${SAFETY_MARGIN_MS}" --output "${OUTPUT_CSV}" \
  --projector-x "${PROJECTOR_X}" --projector-y "${PROJECTOR_Y}" \
  --projector-width "${PROJECTOR_WIDTH}" --projector-height "${PROJECTOR_HEIGHT}"
