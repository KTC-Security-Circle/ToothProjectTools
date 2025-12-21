#pragma once

// まずは最低限（必要になったら増やす）
inline constexpr int KEY_ESC     = 27;
inline constexpr int KEY_Q       = 'q';
inline constexpr int KEY_Q_UPPER = 'Q';
inline constexpr int KEY_F       = 'f';
inline constexpr int KEY_TAB = '\t';

// 余裕があれば waitKeyEx に移行して修飾キー/特殊キーも扱う
// （バックエンド依存のため、必要時に別途定義を追加）
#ifndef CTRL_KEY
#define CTRL_KEY(k) ((k) & 0x1f)
#endif