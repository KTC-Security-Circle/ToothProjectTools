#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  camera-control.sh <camera-index> <zoom-value> <focus-value> <exposure-value>

Example:
  camera-control.sh 0 150 10 100
  camera-control.sh 6 200 30 250

Arguments:
  camera-index    /dev/videoN の N
  zoom-value     zoom_absolute に設定する値
  focus-value    focus_absolute に設定する値
  exposure-value exposure_time_absolute に設定する値
EOF
}

if [[ $# -ne 4 ]]; then
  usage
  exit 1
fi

CAMERA_INDEX="$1"
ZOOM_VALUE="$2"
FOCUS_VALUE="$3"
EXPOSURE_VALUE="$4"
DEVICE="/dev/video${CAMERA_INDEX}"

validate_integer() {
  local name="$1"
  local value="$2"

  if [[ ! "$value" =~ ^[0-9]+$ ]]; then
    echo "Error: ${name} は0以上の整数で指定してください。" >&2
    exit 1
  fi
}

validate_integer "camera-index" "$CAMERA_INDEX"
validate_integer "zoom-value" "$ZOOM_VALUE"
validate_integer "focus-value" "$FOCUS_VALUE"
validate_integer "exposure-value" "$EXPOSURE_VALUE"

if [[ ! -e "$DEVICE" ]]; then
  echo "Error: $DEVICE が存在しません。" >&2
  exit 1
fi

if ! command -v v4l2-ctl >/dev/null 2>&1; then
  echo "Error: v4l2-ctl が見つかりません。" >&2
  echo "Arch Linux:" >&2
  echo "  sudo pacman -S v4l-utils" >&2
  exit 1
fi

CONTROLS="$(v4l2-ctl -d "$DEVICE" --list-ctrls-menus 2>/dev/null)"

has_control() {
  local control_name="$1"
  grep -qE "^[[:space:]]*${control_name}[[:space:]]" <<<"$CONTROLS"
}

set_control() {
  local control_name="$1"
  local value="$2"

  echo "Set: ${control_name}=${value}"
  v4l2-ctl -d "$DEVICE" \
    --set-ctrl="${control_name}=${value}"
}

echo "Device: $DEVICE"

#
# オートフォーカス無効化
#
if has_control "focus_automatic_continuous"; then
  set_control "focus_automatic_continuous" 0
elif has_control "focus_auto"; then
  set_control "focus_auto" 0
else
  echo "Warning: オートフォーカス制御が見つかりません。" >&2
fi

#
# 自動露光無効化
#
# UVCカメラの exposure_auto は通常:
#   1 = Manual Mode
#   3 = Aperture Priority Mode（自動露光）
#
if has_control "exposure_auto"; then
  set_control "exposure_auto" 1
elif has_control "auto_exposure"; then
  # 一部ドライバではboolとして公開される
  set_control "auto_exposure" 1
else
  echo "Warning: 自動露光制御が見つかりません。" >&2
fi

#
# ズーム設定
#
if has_control "zoom_absolute"; then
  set_control "zoom_absolute" "$ZOOM_VALUE"
else
  echo "Error: $DEVICE は zoom_absolute を公開していません。" >&2
  exit 1
fi

#
# ピント設定
#
if has_control "focus_absolute"; then
  set_control "focus_absolute" "$FOCUS_VALUE"
else
  echo "Error: $DEVICE は focus_absolute を公開していません。" >&2
  exit 1
fi

#
# 露光時間設定
#
if has_control "exposure_time_absolute"; then
  set_control "exposure_time_absolute" "$EXPOSURE_VALUE"
elif has_control "exposure_absolute"; then
  set_control "exposure_absolute" "$EXPOSURE_VALUE"
else
  echo "Error: 手動露光時間の制御が見つかりません。" >&2
  exit 1
fi

echo
echo "Applied values:"

GET_CONTROLS=()

has_control "zoom_absolute" &&
  GET_CONTROLS+=("zoom_absolute")

has_control "focus_absolute" &&
  GET_CONTROLS+=("focus_absolute")

if has_control "exposure_time_absolute"; then
  GET_CONTROLS+=("exposure_time_absolute")
elif has_control "exposure_absolute"; then
  GET_CONTROLS+=("exposure_absolute")
fi

if has_control "exposure_auto"; then
  GET_CONTROLS+=("exposure_auto")
elif has_control "auto_exposure"; then
  GET_CONTROLS+=("auto_exposure")
fi

GET_CONTROL_LIST="$(IFS=,; echo "${GET_CONTROLS[*]}")"

v4l2-ctl -d "$DEVICE" \
  --get-ctrl="$GET_CONTROL_LIST"