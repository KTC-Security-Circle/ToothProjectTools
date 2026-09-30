#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

LEFT_CAMERA="${LEFT_CAMERA:-0}"
RIGHT_CAMERA="${RIGHT_CAMERA:-2}"
MONITOR_INDEX="${MONITOR_INDEX:-}"
BOARD_X="${BOARD_X:-10}"
BOARD_Y="${BOARD_Y:-7}"
SQUARE_MM="${SQUARE_MM:-}"
PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE:-/dev/ttyUSB0}"
PHOTODIODE_BAUD="${PHOTODIODE_BAUD:-115200}"
GUARD_MS="${GUARD_MS:-95}"
OUT_DIR="${OUT_DIR:-${REPO_ROOT}/data/calib}"

CALIBRATION_SESSION_SCRIPT="${CALIBRATION_SESSION_SCRIPT:-${SCRIPT_DIR}/calibration_session.sh}"
STEREO_CALIBRATION_SESSION_SCRIPT="${STEREO_CALIBRATION_SESSION_SCRIPT:-${SCRIPT_DIR}/stereo_calibration_session.sh}"
PHOTODIODE_CHECK_SCRIPT="${PHOTODIODE_CHECK_SCRIPT:-${SCRIPT_DIR}/test_photodiode_connection.sh}"
MEASURE_DELAY_SCRIPT="${MEASURE_DELAY_SCRIPT:-${SCRIPT_DIR}/measure_photodiode_delay.sh}"
STEREO_SCAN_SCRIPT="${STEREO_SCAN_SCRIPT:-${SCRIPT_DIR}/stereo_scan.sh}"

MONO_LEFT="${OUT_DIR}/mono_left.yml"
MONO_RIGHT="${OUT_DIR}/mono_right.yml"
STEREO_FILE="${OUT_DIR}/stereo.yml"
INTERRUPTED=0

on_interrupt() {
  INTERRUPTED=1
  printf '\nUse Q to exit safely.\n'
}
trap on_interrupt INT
trap 'exit 143' TERM

is_positive_integer() { [[ "$1" =~ ^[1-9][0-9]*$ ]]; }
is_non_negative_integer() { [[ "$1" =~ ^[0-9]+$ ]]; }
is_positive_number() {
  awk -v value="$1" 'BEGIN { exit !(value ~ /^[0-9]+([.][0-9]+)?$/ && value > 0) }'
}
photodiode_available() {
  [[ -e "${PHOTODIODE_DEVICE}" &&
     -r "${PHOTODIODE_DEVICE}" &&
     -w "${PHOTODIODE_DEVICE}" ]]
}
display_path() {
  local path="$1"
  if [[ "${path}" == "${REPO_ROOT}/"* ]]; then printf '%s' "${path#${REPO_ROOT}/}"
  else printf '%s' "${path}"
  fi
}
mark_file() { [[ -f "$1" ]] && printf '[✓]' || printf '[ ]'; }
pause_menu() {
  printf '\nENTER : return to menu\n'
  IFS= read -r _ || true
}
read_key() {
  REPLY=""
  IFS= read -rsn1 REPLY
}
confirm_enter_or_back() {
  local key
  printf '\nENTER : start\nB     : back\n'
  read_key || return 1
  key="${REPLY}"
  [[ -z "${key}" ]] && return 0
  [[ "${key}" == b || "${key}" == B ]] && return 1
  return 1
}
confirm_continue_or_back() {
  local key
  printf '\nENTER : continue\nB     : back\n'
  read_key || return 1
  key="${REPLY}"
  [[ -z "${key}" ]]
}
finish_child() {
  local label="$1" status="$2"
  if (( status == 0 )); then
    printf '\n%s finished.\n' "${label}"
  elif (( status == 130 )); then
    printf '\nOperation interrupted.\n'
  else
    printf '\n[ERROR] operation failed: exit=%s\n' "${status}"
  fi
  pause_menu
}
ensure_monitor() {
  local value
  while [[ -z "${MONITOR_INDEX}" ]]; do
    printf '\nMonitor index is required for Projector operations.\nMonitor index:\n> '
    if ! IFS= read -r value; then return 1; fi
    if is_non_negative_integer "${value}"; then MONITOR_INDEX="${value}"
    else printf '[ERROR] Enter a non-negative integer.\n'
    fi
  done
}

print_configuration() {
  printf '%s\n' 'ToothProjectTools - Hardware Quick Start' '========================================' '' 'Configuration'
  printf '  Left Camera  : %s\n  Right Camera : %s\n  Monitor      : %s\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${MONITOR_INDEX:-not set}"
  printf '  Board        : %s x %s corners\n  Square       : %s mm\n' "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}"
  printf '  Photodiode   : %s\n  Baud         : %s\n  Guard        : %s ms\n\n' \
    "${PHOTODIODE_DEVICE}" "${PHOTODIODE_BAUD}" "${GUARD_MS}"
  printf '%s\n' 'Calibration'
  printf '  %s Left Mono\n      %s\n\n' "$(mark_file "${MONO_LEFT}")" "$(display_path "${MONO_LEFT}")"
  printf '  %s Right Mono\n      %s\n\n' "$(mark_file "${MONO_RIGHT}")" "$(display_path "${MONO_RIGHT}")"
  printf '  %s Stereo\n      %s\n\n' "$(mark_file "${STEREO_FILE}")" "$(display_path "${STEREO_FILE}")"
}
main_menu() {
  print_configuration
  printf '%s\n' '----------------------------------------' '' \
    '[1] Mono Calibration - LEFT' '[2] Mono Calibration - RIGHT' '[3] Stereo Calibration' \
    '[4] Photodiode Check / Locator' '[5] Stereo Scan + Decode + Reconstruction' '[6] Status' '' \
    '[H] Help' '[Q] Quit'
}

run_mono() {
  local side="$1" display_side camera role output status
  if [[ "${side}" == LEFT ]]; then camera="${LEFT_CAMERA}"; role=left; output="${MONO_LEFT}"; display_side=Left
  else camera="${RIGHT_CAMERA}"; role=right; output="${MONO_RIGHT}"; display_side=Right
  fi
  printf '\n%s Mono Calibration\n---------------------\n\n' "${side}"
  printf 'Camera ID : %s\nRole      : %s\nBoard     : %s x %s\nSquare    : %s mm\nOutput    : %s\n' \
    "${camera}" "${role}" "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}" "$(display_path "${output}")"
  confirm_enter_or_back || return 0
  printf '\nRunning:\n\nCAMERA_ID=%s\nCAMERA_ROLE=%s\nBOARD_X=%s\nBOARD_Y=%s\nSQUARE_MM=%s\nOUT_DIR=%s\n%s\n\n' \
    "${camera}" "${role}" "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}" "${OUT_DIR}" "${CALIBRATION_SESSION_SCRIPT}"
  env CAMERA_ID="${camera}" CAMERA_ROLE="${role}" BOARD_X="${BOARD_X}" BOARD_Y="${BOARD_Y}" \
    SQUARE_MM="${SQUARE_MM}" OUT_DIR="${OUT_DIR}" "${CALIBRATION_SESSION_SCRIPT}"
  status=$?
  finish_child "${display_side} Mono Calibration" "${status}"
}

run_stereo() {
  local missing=() status
  [[ -f "${MONO_LEFT}" ]] || missing+=("${MONO_LEFT}")
  [[ -f "${MONO_RIGHT}" ]] || missing+=("${MONO_RIGHT}")
  if (( ${#missing[@]} > 0 )); then
    printf '\nStereo Calibration cannot start.\n\nMissing:\n'
    for path in "${missing[@]}"; do printf '  [ ] %s\n' "$(display_path "${path}")"; done
    printf '\nRun the missing Mono Calibration first.\n'
    pause_menu
    return
  fi
  printf '\nIMPORTANT\n\nCalibration開始後は、\n\n- Camera position\n- Camera angle\n- Focus\n- Zoom\n- Resolution\n\nを変更しないでください。\n\n動かすのはCheckerboardだけです。\n'
  confirm_continue_or_back || return 0
  printf '\nStereo Calibration\n------------------\n\n'
  printf 'Left Camera  : %s\nRight Camera : %s\nLeft Mono    : OK\nRight Mono   : OK\nBoard        : %s x %s\nSquare       : %s mm\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}"
  confirm_enter_or_back || return 0
  printf '\nRunning:\n\nLEFT_CAMERA=%s\nRIGHT_CAMERA=%s\nBOARD_X=%s\nBOARD_Y=%s\nSQUARE_MM=%s\nOUT_DIR=%s\n%s\n\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}" "${OUT_DIR}" \
    "${STEREO_CALIBRATION_SESSION_SCRIPT}"
  env LEFT_CAMERA="${LEFT_CAMERA}" RIGHT_CAMERA="${RIGHT_CAMERA}" BOARD_X="${BOARD_X}" BOARD_Y="${BOARD_Y}" \
    SQUARE_MM="${SQUARE_MM}" OUT_DIR="${OUT_DIR}" "${STEREO_CALIBRATION_SESSION_SCRIPT}"
  status=$?
  finish_child 'Stereo Calibration' "${status}"
}

run_connection_check() {
  local status
  printf '\nPhotodiode Connection Check\n----------------------------\n\nDevice: %s\n' "${PHOTODIODE_DEVICE}"
  if [[ ! -e "${PHOTODIODE_DEVICE}" ]]; then
    printf '\n[ERROR] %s was not found.\n\nCheck:\n  ls -l /dev/ttyUSB*\n' "${PHOTODIODE_DEVICE}"
    pause_menu
    return
  fi
  confirm_enter_or_back || return 0
  printf '\nRunning:\n\nPHOTODIODE_DEVICE=%s\nPHOTODIODE_BAUD=%s\n%s\n\n' \
    "${PHOTODIODE_DEVICE}" "${PHOTODIODE_BAUD}" "${PHOTODIODE_CHECK_SCRIPT}"
  env PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE}" PHOTODIODE_BAUD="${PHOTODIODE_BAUD}" \
    "${PHOTODIODE_CHECK_SCRIPT}"
  status=$?
  finish_child 'Photodiode Connection Check' "${status}"
}

run_delay_measurement() {
  local status log_file measured answer
  ensure_monitor || return
  printf '\nPhotodiode Delay Measurement\n----------------------------\n\n'
  printf 'Camera    : LEFT (%s)\nProjector : Monitor %s\nDevice    : %s\n\n' \
    "${LEFT_CAMERA}" "${MONITOR_INDEX}" "${PHOTODIODE_DEVICE}"
  printf 'Projectorに\n\n  FULL WHITE\n  + RED locator\n\nを表示します。\n\nPhotodiodeを赤い四角の中央へ配置してください。\n'
  confirm_enter_or_back || return 0
  printf '\nRunning:\n\nCAMERA_ID=%s\nMONITOR_INDEX=%s\nPHOTODIODE_DEVICE=%s\nPHOTODIODE_BAUD=%s\n%s\n\n' \
    "${LEFT_CAMERA}" "${MONITOR_INDEX}" "${PHOTODIODE_DEVICE}" "${PHOTODIODE_BAUD}" "${MEASURE_DELAY_SCRIPT}"
  log_file="$(mktemp /tmp/tooth-measure-delay.XXXXXX)"
  env CAMERA_ID="${LEFT_CAMERA}" MONITOR_INDEX="${MONITOR_INDEX}" PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE}" \
    PHOTODIODE_BAUD="${PHOTODIODE_BAUD}" "${MEASURE_DELAY_SCRIPT}" 2>&1 | tee "${log_file}"
  status=${PIPESTATUS[0]}
  if (( status == 0 )); then
    measured="$(sed -n 's/^recommended_guard_ms:[[:space:]]*\([0-9][0-9]*\)[[:space:]]*$/\1/p' "${log_file}" | tail -n 1)"
    if [[ -n "${measured}" ]]; then
      printf '\nMeasured guard: %s ms\n\nUse GUARD_MS=%s for this session? [Y/n]\n' "${measured}" "${measured}"
      read_key || true
      answer="${REPLY}"
      if [[ -z "${answer}" || "${answer}" == y || "${answer}" == Y ]]; then
        GUARD_MS="${measured}"
        printf 'Session GUARD_MS updated to %s ms.\n' "${GUARD_MS}"
      fi
    else
      printf '\nGuard value could not be imported automatically.\nCurrent GUARD_MS remains %s ms.\n' "${GUARD_MS}"
    fi
  fi
  rm -f -- "${log_file}"
  finish_child 'Photodiode Delay Measurement' "${status}"
}

photodiode_menu() {
  local key
  while true; do
    printf '\nPhotodiode\n==========\n\n[1] Connection Check\n[2] Show RED Locator + Measure Delay\n[B] Back\n'
    read_key || return
    key="${REPLY}"
    case "${key}" in
      1) run_connection_check; return ;;
      2) run_delay_measurement; return ;;
      b|B) return ;;
    esac
  done
}

run_scan() {
  local status
  if [[ ! -f "${STEREO_FILE}" ]]; then
    printf '\nStereo calibration is required first.\n\n[ ] %s\n' "$(display_path "${STEREO_FILE}")"
    pause_menu
    return
  fi
  if [[ ! -e "${PHOTODIODE_DEVICE}" ]]; then
    printf '\n[ERROR] %s was not found.\n\nCheck:\n  ls -l /dev/ttyUSB*\n' "${PHOTODIODE_DEVICE}"
    pause_menu
    return
  fi
  ensure_monitor || return
  printf '\nStereo Scan\n===========\n\n'
  printf 'Left Camera  : %s\nRight Camera : %s\nMonitor      : %s\nCalibration  : %s\nPhotodiode   : %s\nGuard        : %s ms\n\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${MONITOR_INDEX}" "$(display_path "${STEREO_FILE}")" \
    "${PHOTODIODE_DEVICE}" "${GUARD_MS}"
  printf '%s\n' 'The following pipeline will run:' '' 'Projector setup' '      ↓' \
    'RED locator / FULL WHITE' '      ↓' 'SPACE' '      ↓' 'Gray Code capture' '      ↓' \
    'Decode' '      ↓' 'Stereo reconstruction' '      ↓' 'PLY'
  printf '\nENTER : open scan session\nB     : back\n'
  read_key || return
  [[ -z "${REPLY}" ]] || return
  printf '\nRunning:\n\nLEFT_CAMERA=%s\nRIGHT_CAMERA=%s\nMONITOR_INDEX=%s\nCALIBRATION_FILE=%s\nPHOTODIODE_DEVICE=%s\nPHOTODIODE_BAUD=%s\nGUARD_MS=%s\n%s\n\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${MONITOR_INDEX}" "${STEREO_FILE}" "${PHOTODIODE_DEVICE}" \
    "${PHOTODIODE_BAUD}" "${GUARD_MS}" "${STEREO_SCAN_SCRIPT}"
  env LEFT_CAMERA="${LEFT_CAMERA}" RIGHT_CAMERA="${RIGHT_CAMERA}" MONITOR_INDEX="${MONITOR_INDEX}" \
    CALIBRATION_FILE="${STEREO_FILE}" PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE}" \
    PHOTODIODE_BAUD="${PHOTODIODE_BAUD}" GUARD_MS="${GUARD_MS}" "${STEREO_SCAN_SCRIPT}"
  status=$?
  finish_child 'Stereo Scan + Decode + Reconstruction' "${status}"
}

show_status() {
  local backend="${REPO_ROOT}/build/release-opencv-4.10-static/src/serve/tooth-backend"
  printf '\nHardware Status\n===============\n\nBackend\n  %s tooth-backend\n\n' "$([[ -x "${backend}" ]] && printf '[✓]' || printf '[ ]')"
  printf 'Camera\n  Left ID  : %s\n  Right ID : %s\n\nCalibration\n' "${LEFT_CAMERA}" "${RIGHT_CAMERA}"
  printf '  %s mono_left.yml\n  %s mono_right.yml\n  %s stereo.yml\n\n' \
    "$(mark_file "${MONO_LEFT}")" "$(mark_file "${MONO_RIGHT}")" "$(mark_file "${STEREO_FILE}")"
  printf 'Photodiode\n  %s %s\n\nProjector\n  Monitor index: %s\n\nScan\n  Guard: %s ms\n' \
    "$([[ -e "${PHOTODIODE_DEVICE}" ]] && printf '[✓]' || printf '[ ]')" "${PHOTODIODE_DEVICE}" \
    "${MONITOR_INDEX:-not set}" "${GUARD_MS}"
  printf '\nCamera IDs can be checked with:\n  v4l2-ctl --list-devices\n'
  pause_menu
}

show_help() {
  printf '\n%s\n' 'Recommended Workflow' '====================' '' \
    '1. LEFT Mono Calibration' '2. RIGHT Mono Calibration' '3. Stereo Calibration' \
    '4. Photodiode Connection Check' '5. Photodiode Locator / Delay Measurement' \
    '6. Stereo Scan + Decode + Reconstruction' '' 'Calibration後はCameraを動かさないこと。' '' \
    'Mono:' '  Checkerboardを1台のCameraで撮影します。' '' \
    'Stereo:' '  同じCheckerboardを左右Cameraへ同時に映します。' '' \
    'Photodiode:' '  RED locatorへsensorを配置します。' '' \
    'Scan:' '  SPACEを押すごとに1回scanします。' '' \
    'Q:' '  現在のsub-sessionを終了してmain menuへ戻ります。'
  pause_menu
}

confirm_quit() {
  printf '\nExit Hardware Quick Start?\n\n[Y] Yes\n[N] No\n'
  read_key || return 1
  [[ "${REPLY}" == y || "${REPLY}" == Y ]]
}

for value_name in LEFT_CAMERA RIGHT_CAMERA BOARD_X BOARD_Y PHOTODIODE_BAUD GUARD_MS; do
  value="${!value_name}"
  if [[ "${value_name}" == LEFT_CAMERA || "${value_name}" == RIGHT_CAMERA || "${value_name}" == GUARD_MS ]]; then
    is_non_negative_integer "${value}" || { printf '[ERROR] %s must be a non-negative integer.\n' "${value_name}" >&2; exit 2; }
  else
    is_positive_integer "${value}" || { printf '[ERROR] %s must be a positive integer.\n' "${value_name}" >&2; exit 2; }
  fi
done

while ! is_positive_number "${SQUARE_MM}"; do
  [[ -z "${SQUARE_MM}" ]] || printf '[ERROR] Enter a positive number.\n'
  printf 'Checkerboard square size [mm]:\n> '
  IFS= read -r SQUARE_MM || exit 1
done

while true; do
  INTERRUPTED=0
  printf '\n'
  main_menu
  if ! read_key; then
    (( INTERRUPTED == 1 )) && continue
    break
  fi
  case "${REPLY}" in
    1) run_mono LEFT ;;
    2) run_mono RIGHT ;;
    3) run_stereo ;;
    4) photodiode_menu ;;
    5) run_scan ;;
    6) show_status ;;
    h|H) show_help ;;
    q|Q) confirm_quit && exit 0 ;;
  esac
done
