// include/structured_light/structured_light.hpp
#pragma once

#include <opencv2/structured_light.hpp>
#include <vector>
#include <memory>
#include "calibration_types.hpp"

namespace sl {
  class StructuredLight {
  public:
    // 初期化時はプロジェクタの解像度が必要（windowから取得）
    StructuredLight(int width, int height);
    ~StructuredLight();

    // --- Phase 1: パターン生成 ---
    // パターンを生成し、内部キャッシュする
    void generatePatterns();
    
    // 投影に必要な画像の総数（縦縞 + 横縞 + 白 + 黒）
    size_t getPatternCount() const;
    
    // 指定インデックスのパターン画像を取得（表示用）
    // index: 0 ~ (count-1)
    const cv::Mat& getPattern(size_t index) const;

    // --- Phase 2: デコード & 3D復元 ---
    // 撮影した画像リストをセット（順序は getPattern と一致させる前提）
    void setCapturedImages(const std::vector<cv::Mat>& captures);

    // デコード実行（要: 白/黒画像）
    // 結果は内部にキャッシュされる
    bool decode();

    // 3D点群生成（要: キャリブレーション済み）
    // 戻り値: xyz座標が入ったCV_32FC3のMat
    cv::Mat reconstruct3D();

    // --- 設定 / プロパティ ---
    void setCalibration(const SystemCalibration& calib);
    
    // 影とみなす黒レベルしきい値 (0-255)
    void setBlackThreshold(int value);
    // ハレーションとみなす白レベルしきい値 (0-255)
    void setWhiteThreshold(int value);

  private:
    // 解像度
    int width_;
    int height_;

    // OpenCV 実装へのポインタ
    cv::Ptr<cv::structured_light::GrayCodePattern> graycode_;

    // 生成済みパターンキャッシュ
    std::vector<cv::Mat> patterns_;
    
    // 撮影画像バッファ（デコード用）
    std::vector<cv::Mat> captures_;
    
    // 影判定用しきい値
    size_t black_threshold_{40}; // デフォルト
    size_t white_threshold_{5};  // パターン判定のロバスト性用

    // キャリブレーションデータ
    SystemCalibration calib_;

    cv::Mat disparity_map_;
  };
}
