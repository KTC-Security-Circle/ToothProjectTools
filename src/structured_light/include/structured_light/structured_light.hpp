// include/structured_light/structured_light.hpp
#pragma once

#include <opencv2/structured_light.hpp>
#include <vector>
#include <memory>
#include <chrono>
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

    // 現在のインデックス操作
    int getCurrentIndex() const { return current_pattern_index_; }
    void setIndex(int index); // 範囲チェック付きでセット
    
    // 次/前のインデックスを計算してセット（ループするかは引数で制御可）
    void nextPattern(bool loop = true);
    void prevPattern(bool loop = true);

    // スキャン状態
    bool isScanning() const { return is_scanning_; }
    void startScan();
    void stopScan();

    // タイマー管理: 指定ミリ秒経過したか判定し、経過していれば時刻を更新してtrueを返す
    bool checkTimerAndReset(int interval_ms);
    
    // 現在表示すべきパターン画像を取得（現在のインデックスに基づき返す）
    const cv::Mat& getCurrentPatternImage() const;

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

    bool is_scanning_{false};
    int current_pattern_index_{0};
    std::chrono::steady_clock::time_point last_pattern_change_time_;
  };
}
