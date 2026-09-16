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

} // namespace sl
