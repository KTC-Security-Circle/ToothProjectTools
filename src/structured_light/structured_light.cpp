#include "structured_light/structured_light.hpp"
#include "logger/logger_macros.hpp" // ロガーマクロのインクルード

#include <stdexcept>

namespace sl {

StructuredLight::StructuredLight(int width, int height)
    : width_(width), height_(height) {
  // GrayCodePattern の生成
  cv::structured_light::GrayCodePattern::Params params;
  params.width = width;
  params.height = height;
  graycode_ = cv::structured_light::GrayCodePattern::create(params);
  
  LOG_INFO("StructuredLight 初期化: resolution={}x{}", width, height);
}

// --- Phase 1: パターン生成 ---

void StructuredLight::generatePatterns() {
  if (!graycode_) {
      LOG_ERROR("generatePatterns 失敗: GrayCodePattern インスタンスがありません");
      return;
  }

  patterns_.clear();
  
  // 1. GrayCode 縞模様の生成
  graycode_->generate(patterns_);

  // 2. シャドウマスク（影除去）用の「全白」「全黒」画像を追加生成
  cv::Mat white_pattern(height_, width_, CV_8UC1, cv::Scalar(255));
  cv::Mat black_pattern(height_, width_, CV_8UC1, cv::Scalar(0));

  patterns_.push_back(white_pattern);
  patterns_.push_back(black_pattern);

  LOG_INFO("パターン生成完了: 合計枚数={} (縞模様+白+黒)", patterns_.size());
}

size_t StructuredLight::getPatternCount() const {
  return patterns_.size();
}

const cv::Mat& StructuredLight::getPattern(size_t index) const {
  if (index >= patterns_.size()) {
    // 例外送出前にもエラーログを残す
    LOG_ERROR("getPattern 範囲外アクセス: index={}, size={}", index, patterns_.size());
    throw std::out_of_range("Pattern index out of range");
  }
  return patterns_[index];
}

// =============================================================
// シーケンス状態管理の実装
// =============================================================

void StructuredLight::setIndex(int index) {
    if (patterns_.empty()) return;
    // 範囲内に収める
    current_pattern_index_ = std::clamp(index, 0, (int)patterns_.size() - 1);
    // 時刻リセット
    last_pattern_change_time_ = std::chrono::steady_clock::now();
}

void StructuredLight::nextPattern(bool loop) {
    if (patterns_.empty()) return;
    int sz = static_cast<int>(patterns_.size());
    int next = current_pattern_index_ + 1;
    
    if (next >= sz) {
        next = loop ? 0 : sz - 1;
    }
    setIndex(next);
}

void StructuredLight::prevPattern(bool loop) {
    if (patterns_.empty()) return;
    int sz = static_cast<int>(patterns_.size());
    int prev = current_pattern_index_ - 1;

    if (prev < 0) {
        prev = loop ? sz - 1 : 0;
    }
    setIndex(prev);
}

void StructuredLight::startScan() {
    is_scanning_ = true;
    setIndex(0); // 最初から
}

void StructuredLight::stopScan() {
    is_scanning_ = false;
}

bool StructuredLight::checkTimerAndReset(int interval_ms) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_pattern_change_time_).count();
    
    if (elapsed >= interval_ms) {
        // 時刻はここでリセットせず、次のパターンへ遷移した時にリセットするのが一般的だが、
        // 呼び出し元のロジックに合わせて調整。ここでは「判定OK」だけ返す
        return true;
    }
    return false;
}

const cv::Mat& StructuredLight::getCurrentPatternImage() const {
    if (patterns_.empty()) {
        // パターンがない場合のダミー（または例外）
        static cv::Mat dummy(100, 100, CV_8UC1, cv::Scalar(0));
        return dummy;
    }
    return getPattern(current_pattern_index_);
}

} // namespace sl
