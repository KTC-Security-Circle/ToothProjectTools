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
  printf '\n操作を中断しました。安全に終了するにはQを押してください。\n'
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
  printf '\nENTER : メニューへ戻る\n'
  IFS= read -r _ || true
}
read_key() {
  REPLY=""
  IFS= read -rsn1 REPLY
}
confirm_enter_or_back() {
  local key
  printf '\nENTER : 開始\nB     : 戻る\n'
  read_key || return 1
  key="${REPLY}"
  [[ -z "${key}" ]] && return 0
  [[ "${key}" == b || "${key}" == B ]] && return 1
  return 1
}
confirm_continue_or_back() {
  local key
  printf '\nENTER : 続ける\nB     : 戻る\n'
  read_key || return 1
  key="${REPLY}"
  [[ -z "${key}" ]]
}
finish_child() {
  local label="$1" status="$2"
  if (( status == 0 )); then
    printf '\n処理が完了しました。\n'
  elif (( status == 130 )); then
    printf '\n操作を中断しました。\n'
  else
    printf '\n[エラー] 処理に失敗しました。\n終了コード: %s\n' "${status}"
  fi
  pause_menu
}
ensure_monitor() {
  local value
  while [[ -z "${MONITOR_INDEX}" ]]; do
    printf '\nProjector操作にはモニター番号が必要です。\nモニター番号:\n> '
    if ! IFS= read -r value; then return 1; fi
    if is_non_negative_integer "${value}"; then MONITOR_INDEX="${value}"
    else printf '[エラー] 0以上の整数を入力してください。\n'
    fi
  done
}

print_configuration() {
  printf '%s\n' 'ToothProjectTools - 実機クイックスタート' '========================================' '' '現在の設定'
  printf '  左カメラ       : %s\n  右カメラ       : %s\n  モニター       : %s\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${MONITOR_INDEX:-未設定}"
  printf '  ボード内部角   : %s x %s\n  1マス          : %s mm\n' "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}"
  printf '  Photodiode   : %s\n  Baud         : %s\n  Guard        : %s ms\n\n' \
    "${PHOTODIODE_DEVICE}" "${PHOTODIODE_BAUD}" "${GUARD_MS}"
  printf '%s\n' 'キャリブレーション'
  printf '  %s 左 Mono\n      %s\n\n' "$(mark_file "${MONO_LEFT}")" "$(display_path "${MONO_LEFT}")"
  printf '  %s 右 Mono\n      %s\n\n' "$(mark_file "${MONO_RIGHT}")" "$(display_path "${MONO_RIGHT}")"
  printf '  %s Stereo\n      %s\n\n' "$(mark_file "${STEREO_FILE}")" "$(display_path "${STEREO_FILE}")"
}
main_menu() {
  print_configuration
  printf '%s\n' '----------------------------------------' '' \
    '[1] 左 Monoキャリブレーション' '[2] 右 Monoキャリブレーション' '[3] Stereoキャリブレーション' \
    '[4] Photodiode確認・同期測定' '[5] Scan・Decode・3D復元' '[6] 状態確認' '' \
    '[H] ヘルプ' '[Q] 終了'
}

run_mono() {
  local side="$1" display_side camera role output status
  if [[ "${side}" == LEFT ]]; then camera="${LEFT_CAMERA}"; role=left; output="${MONO_LEFT}"; display_side=Left
  else camera="${RIGHT_CAMERA}"; role=right; output="${MONO_RIGHT}"; display_side=Right
  fi
  printf '\n%s Monoキャリブレーション\n==========================\n\n' "$([[ "${side}" == LEFT ]] && printf '左' || printf '右')"
  printf 'カメラID : %s\n役割     : %s\n内部角   : %s x %s\n1マス    : %s mm\n出力     : %s\n' \
    "${camera}" "${role}" "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}" "$(display_path "${output}")"
  confirm_enter_or_back || return 0
  printf '\n実行します:\n\nCAMERA_ID=%s\nCAMERA_ROLE=%s\nBOARD_X=%s\nBOARD_Y=%s\nSQUARE_MM=%s\nOUT_DIR=%s\n%s\n\n' \
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
    printf '\nStereoキャリブレーションを開始できません。\n\n不足ファイル:\n'
    for path in "${missing[@]}"; do printf '  [ ] %s\n' "$(display_path "${path}")"; done
    printf '\n先に不足しているMonoキャリブレーションを実行してください。\n'
    pause_menu
    return
  fi
  printf '\n重要\n\nキャリブレーション開始後は、\n\n- カメラ位置\n- カメラ角度\n- Focus\n- Zoom\n- 解像度\n\nを変更しないでください。\n\n動かすのはCheckerboardだけです。\n'
  confirm_continue_or_back || return 0
  printf '\nStereoキャリブレーション\n========================\n\n'
  printf '左カメラ : %s\n右カメラ : %s\n左Mono   : OK\n右Mono   : OK\n内部角   : %s x %s\n1マス    : %s mm\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}"
  confirm_enter_or_back || return 0
  printf '\n実行します:\n\nLEFT_CAMERA=%s\nRIGHT_CAMERA=%s\nBOARD_X=%s\nBOARD_Y=%s\nSQUARE_MM=%s\nOUT_DIR=%s\n%s\n\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${BOARD_X}" "${BOARD_Y}" "${SQUARE_MM}" "${OUT_DIR}" \
    "${STEREO_CALIBRATION_SESSION_SCRIPT}"
  env LEFT_CAMERA="${LEFT_CAMERA}" RIGHT_CAMERA="${RIGHT_CAMERA}" BOARD_X="${BOARD_X}" BOARD_Y="${BOARD_Y}" \
    SQUARE_MM="${SQUARE_MM}" OUT_DIR="${OUT_DIR}" "${STEREO_CALIBRATION_SESSION_SCRIPT}"
  status=$?
  finish_child 'Stereo Calibration' "${status}"
}

run_connection_check() {
  local status
  printf '\nPhotodiode接続確認\n==================\n\nデバイス: %s\n' "${PHOTODIODE_DEVICE}"
  if [[ ! -e "${PHOTODIODE_DEVICE}" ]]; then
    printf '\n[エラー] %s が見つかりません。\n\n確認コマンド:\n  ls -l /dev/ttyUSB*\n' "${PHOTODIODE_DEVICE}"
    pause_menu
    return
  fi
  confirm_enter_or_back || return 0
  printf '\n実行します:\n\nPHOTODIODE_DEVICE=%s\nPHOTODIODE_BAUD=%s\n%s\n\n' \
    "${PHOTODIODE_DEVICE}" "${PHOTODIODE_BAUD}" "${PHOTODIODE_CHECK_SCRIPT}"
  env PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE}" PHOTODIODE_BAUD="${PHOTODIODE_BAUD}" \
    "${PHOTODIODE_CHECK_SCRIPT}"
  status=$?
  finish_child 'Photodiode Connection Check' "${status}"
}

run_delay_measurement() {
  local status log_file measured answer
  ensure_monitor || return
  printf '\nPhotodiode同期遅延測定\n======================\n\n'
  printf 'カメラ    : 左 (%s)\nProjector : モニター %s\nデバイス  : %s\n\n' \
    "${LEFT_CAMERA}" "${MONITOR_INDEX}" "${PHOTODIODE_DEVICE}"
  printf 'Projectorに\n\n  全面白\n  + 赤locator\n\nを表示します。\n\nPhotodiodeを赤い四角の中央へ配置してください。\n'
  confirm_enter_or_back || return 0
  printf '\n実行します:\n\nCAMERA_ID=%s\nMONITOR_INDEX=%s\nPHOTODIODE_DEVICE=%s\nPHOTODIODE_BAUD=%s\n%s\n\n' \
    "${LEFT_CAMERA}" "${MONITOR_INDEX}" "${PHOTODIODE_DEVICE}" "${PHOTODIODE_BAUD}" "${MEASURE_DELAY_SCRIPT}"
  log_file="$(mktemp /tmp/tooth-measure-delay.XXXXXX)"
  env CAMERA_ID="${LEFT_CAMERA}" MONITOR_INDEX="${MONITOR_INDEX}" PHOTODIODE_DEVICE="${PHOTODIODE_DEVICE}" \
    PHOTODIODE_BAUD="${PHOTODIODE_BAUD}" "${MEASURE_DELAY_SCRIPT}" 2>&1 | tee "${log_file}"
  status=${PIPESTATUS[0]}
  if (( status == 0 )); then
    measured="$(sed -n 's/^recommended_guard_ms:[[:space:]]*\([0-9][0-9]*\)[[:space:]]*$/\1/p' "${log_file}" | tail -n 1)"
    if [[ -n "${measured}" ]]; then
      printf '\n測定したGuard: %s ms\n\nこのsessionでGUARD_MS=%sを使用しますか？ [Y/n]\n' "${measured}" "${measured}"
      read_key || true
      answer="${REPLY}"
      if [[ -z "${answer}" || "${answer}" == y || "${answer}" == Y ]]; then
        GUARD_MS="${measured}"
        printf 'このsessionのGUARD_MSを%s msへ更新しました。\n' "${GUARD_MS}"
      fi
    else
      printf '\nGuard値を自動取得できませんでした。現在のGUARD_MS=%s msを維持します。\n' "${GUARD_MS}"
    fi
  fi
  rm -f -- "${log_file}"
  finish_child 'Photodiode Delay Measurement' "${status}"
}

photodiode_menu() {
  local key
  while true; do
    printf '\nPhotodiode\n==========\n\n[1] 接続確認\n[2] 赤locator表示・同期遅延測定\n[B] 戻る\n'
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
    printf '\n先にStereoキャリブレーションが必要です。\n\n[ ] %s\n' "$(display_path "${STEREO_FILE}")"
    pause_menu
    return
  fi
  if [[ ! -e "${PHOTODIODE_DEVICE}" ]]; then
    printf '\n[エラー] %s が見つかりません。\n\n確認コマンド:\n  ls -l /dev/ttyUSB*\n' "${PHOTODIODE_DEVICE}"
    pause_menu
    return
  fi
  ensure_monitor || return
  printf '\nStereo Scan\n===========\n\n'
  printf '左カメラ          : %s\n右カメラ          : %s\nモニター          : %s\nキャリブレーション: %s\nPhotodiode        : %s\nGuard             : %s ms\n\n' \
    "${LEFT_CAMERA}" "${RIGHT_CAMERA}" "${MONITOR_INDEX}" "$(display_path "${STEREO_FILE}")" \
    "${PHOTODIODE_DEVICE}" "${GUARD_MS}"
  printf '%s\n' '次の処理を実行します:' '' 'Projector設定' '      ↓' \
    '赤locator / 全面白' '      ↓' 'SPACE' '      ↓' 'Gray Code撮影' '      ↓' \
    'Decode' '      ↓' 'Stereo 3D復元' '      ↓' 'PLY'
  printf '\nENTER : Scan sessionを開く\nB     : 戻る\n'
  read_key || return
  [[ -z "${REPLY}" ]] || return
  printf '\n実行します:\n\nLEFT_CAMERA=%s\nRIGHT_CAMERA=%s\nMONITOR_INDEX=%s\nCALIBRATION_FILE=%s\nPHOTODIODE_DEVICE=%s\nPHOTODIODE_BAUD=%s\nGUARD_MS=%s\n%s\n\n' \
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
  printf '\n実機状態\n========\n\nBackend\n  %s tooth-backend\n\n' "$([[ -x "${backend}" ]] && printf '[✓]' || printf '[ ]')"
  printf 'カメラ\n  左ID : %s\n  右ID : %s\n\nキャリブレーション\n' "${LEFT_CAMERA}" "${RIGHT_CAMERA}"
  printf '  %s mono_left.yml\n  %s mono_right.yml\n  %s stereo.yml\n\n' \
    "$(mark_file "${MONO_LEFT}")" "$(mark_file "${MONO_RIGHT}")" "$(mark_file "${STEREO_FILE}")"
  printf 'Photodiode\n  %s %s\n\nProjector\n  モニター番号: %s\n\nScan\n  Guard: %s ms\n' \
    "$([[ -e "${PHOTODIODE_DEVICE}" ]] && printf '[✓]' || printf '[ ]')" "${PHOTODIODE_DEVICE}" \
    "${MONITOR_INDEX:-未設定}" "${GUARD_MS}"
  printf '\nカメラIDの確認コマンド:\n  v4l2-ctl --list-devices\n'
  pause_menu
}

show_help() {
  printf '\n%s\n' '推奨手順' '========' '' \
    '1. 左 Monoキャリブレーション' '2. 右 Monoキャリブレーション' '3. Stereoキャリブレーション' \
    '4. Photodiode接続確認' '5. Photodiode locator・同期遅延測定' \
    '6. Stereo Scan・Decode・3D復元' '' 'キャリブレーション後はカメラを動かさないこと。' '' \
    'Mono:' '  Checkerboardを1台のCameraで撮影します。' '' \
    'Stereo:' '  同じCheckerboardを左右Cameraへ同時に映します。' '' \
    'Photodiode:' '  RED locatorへsensorを配置します。' '' \
    'Scan:' '  SPACEを押すごとに1回scanします。' '' \
    'Q:' '  現在のsub-sessionを終了してメインメニューへ戻ります。'
  pause_menu
}

confirm_quit() {
  printf '\n実機クイックスタートを終了しますか？\n\n[Y] 終了する\n[N] 戻る\n'
  read_key || return 1
  [[ "${REPLY}" == y || "${REPLY}" == Y ]]
}

for value_name in LEFT_CAMERA RIGHT_CAMERA BOARD_X BOARD_Y PHOTODIODE_BAUD GUARD_MS; do
  value="${!value_name}"
  if [[ "${value_name}" == LEFT_CAMERA || "${value_name}" == RIGHT_CAMERA || "${value_name}" == GUARD_MS ]]; then
    is_non_negative_integer "${value}" || { printf '[エラー] %sは0以上の整数にしてください。\n' "${value_name}" >&2; exit 2; }
  else
    is_positive_integer "${value}" || { printf '[エラー] %sは正の整数にしてください。\n' "${value_name}" >&2; exit 2; }
  fi
done

while ! is_positive_number "${SQUARE_MM}"; do
  [[ -z "${SQUARE_MM}" ]] || printf '[エラー] 正の数を入力してください。\n'
  printf 'Checkerboardの1マスのサイズ [mm]:\n> '
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
