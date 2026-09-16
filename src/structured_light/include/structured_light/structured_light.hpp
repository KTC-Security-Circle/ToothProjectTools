// include/structured_light/structured_light.hpp
#pragma once

#include <opencv2/structured_light.hpp>
#include <vector>

namespace sl {
  class StructuredLight {
  public:
    // 初期化時はプロジェクタの解像度が必要（windowから取得）
    StructuredLight(int width, int height);
    ~StructuredLight() = default;

    // --- Phase 1: パターン生成 ---
    // パターンを生成し、内部キャッシュする
    void generatePatterns();
    
    // 投影に必要な画像の総数（縦縞 + 横縞 + 白 + 黒）
    size_t getPatternCount() const;
    
    // 指定インデックスのパターン画像を取得（表示用）
    // index: 0 ~ (count-1)
    const cv::Mat& getPattern(size_t index) const;

  private:
    // 解像度
    int width_;
    int height_;

    // OpenCV 実装へのポインタ
    cv::Ptr<cv::structured_light::GrayCodePattern> graycode_;

    // 生成済みパターンキャッシュ
    std::vector<cv::Mat> patterns_;
    
  };
}
