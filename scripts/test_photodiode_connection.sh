#!/usr/bin/env bash
set -Eeuo pipefail

DEVICE="${PHOTODIODE_DEVICE:-/dev/ttyUSB0}"
BAUD="${PHOTODIODE_BAUD:-115200}"
TIMEOUT_SEC="${PHOTODIODE_TIMEOUT_SEC:-5}"

[[ -e "${DEVICE}" ]] || { printf 'device not found: %s\n' "${DEVICE}" >&2; exit 2; }
[[ -r "${DEVICE}" && -w "${DEVICE}" ]] || { printf 'permission denied: %s\n' "${DEVICE}" >&2; exit 3; }

printf 'device: %s\nbaud: %s\n\n' "${DEVICE}" "${BAUD}"
stty -F "${DEVICE}" "${BAUD}" raw -echo -echoe -echok || { printf 'failed to configure: %s\n' "${DEVICE}" >&2; exit 4; }
exec 3<>"${DEVICE}"
trap 'exec 3>&- 3<&- 2>/dev/null || true; printf "\nclosed: %s\n" "${DEVICE}"' EXIT INT TERM

count=0
previous=""
while true; do
  if ! IFS= read -r -t "${TIMEOUT_SEC}" line <&3; then
    printf 'timeout waiting for photodiode event (%ss)\n' "${TIMEOUT_SEC}" >&2
    exit 5
  fi
  line="${line%$'\r'}"
  case "${line}" in
    0) state=black ;;
    1) state=white ;;
    *) printf 'invalid line: %q\n' "${line}" >&2; continue ;;
  esac
  if [[ "${line}" == "${previous}" ]]; then
    printf 'duplicate state ignored: %s\n' "${state}" >&2
    continue
  fi
  previous="${line}"
  count=$((count + 1))
  printf '[%03d] %s\n' "${count}" "${state}"
done
