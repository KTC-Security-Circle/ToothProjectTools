// include/structured_light/structured_light.hpp
#pragma once

#include <opencv2/structured_light.hpp>
#include <vector>
#include <chrono>

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
    
    bool is_scanning_{false};
    int current_pattern_index_{0};
    std::chrono::steady_clock::time_point last_pattern_change_time_;
  };
}
