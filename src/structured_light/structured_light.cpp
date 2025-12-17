#include "structured_light/structured_light.hpp"
#include "logger/logger_macros.hpp" // ロガーマクロのインクルード

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

// ※ 注意: ヘッダの private メンバに cv::Mat disparity_map_; を追加してください。

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

StructuredLight::~StructuredLight() {
  // cv::Ptr が自動解放するため何もしない
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

// --- Phase 2: デコード & 3D復元 ---

void StructuredLight::setCapturedImages(const std::vector<cv::Mat>& captures) {
  captures_ = captures;
  LOG_INFO("撮影画像をセット: {} 枚", captures_.size());
}

bool StructuredLight::decode() {
  if (patterns_.empty()) {
    LOG_ERROR("デコード失敗: パターンが生成されていません");
    return false;
  }
  
  if (captures_.size() != patterns_.size()) {
    LOG_ERROR("デコード失敗: 画像枚数不一致 (必要={}, 実際={})", 
              patterns_.size(), captures_.size());
    return false;
  }

  // GrayCodePattern::decode 用に vector<vector<Mat>> へ変換
  size_t pattern_count = graycode_->getNumberOfPatternImages();
  std::vector<std::vector<cv::Mat>> capture_list(pattern_count);
  
  for (size_t i = 0; i < pattern_count; ++i) {
    capture_list[i].push_back(captures_[i]);
  }

  // 末尾の2枚は 白(White) と 黒(Black)
  const cv::Mat& white_img = captures_[pattern_count];
  const cv::Mat& black_img = captures_[pattern_count + 1];

  // しきい値設定
  graycode_->setBlackThreshold(static_cast<size_t>(black_threshold_));
  graycode_->setWhiteThreshold(static_cast<size_t>(white_threshold_));

  LOG_INFO("デコード開始: threshold(B={}, W={})", black_threshold_, white_threshold_);

  // デコード実行 -> 結果は disparity_map_ に格納
  bool success = graycode_->decode(capture_list, disparity_map_, black_img, white_img,
                                   cv::structured_light::DECODE_3D_UNDERWORLD);

  if (success) {
      LOG_INFO("デコード成功: マップサイズ={}x{}", disparity_map_.cols, disparity_map_.rows);
  } else {
      LOG_ERROR("デコード処理でエラーが発生しました (OpenCV内部エラー)");
  }

  return success;
}

cv::Mat StructuredLight::reconstruct3D() {
  // 事前チェック
  if (!calib_.ready) {
    LOG_ERROR("3D復元失敗: キャリブレーションデータがセットされていません");
    return cv::Mat();
  }
  if (disparity_map_.empty()) {
    LOG_ERROR("3D復元失敗: 視差マップが空です。先に decode() を実行してください");
    return cv::Mat();
  }

  LOG_INFO("3D復元(Triangulation)を開始...");

  // 出力用 Mat (高さ x 幅, float 3チャンネル)
  cv::Mat points3d(disparity_map_.size(), CV_32FC3, cv::Scalar(NAN, NAN, NAN));

  // 有効な対応点をリストアップ
  std::vector<cv::Point2f> cam_pts;
  std::vector<cv::Point2f> proj_pts;
  std::vector<cv::Point>   pixels; // 書き戻し用インデックス

  for (int y = 0; y < disparity_map_.rows; ++y) {
    for (int x = 0; x < disparity_map_.cols; ++x) {
      cv::Point2f proj_pt;
      if (disparity_map_.type() == CV_32FC2) {
         proj_pt = disparity_map_.at<cv::Point2f>(y, x);
      } else if (disparity_map_.type() == CV_64FC2) {
         proj_pt = cv::Point2f(disparity_map_.at<cv::Point2d>(y, x));
      } else {
         continue; 
      }

      // (-1, -1) は無効な画素
      if (proj_pt.x < 0 || proj_pt.y < 0) continue;

      cam_pts.push_back(cv::Point2f(static_cast<float>(x), static_cast<float>(y)));
      proj_pts.push_back(proj_pt);
      pixels.push_back(cv::Point(x, y));
    }
  }

  if (cam_pts.empty()) {
    LOG_WARN("3D復元: 有効な対応点が1つも見つかりませんでした");
    return points3d; 
  }

  // 1. 歪み補正 (Undistort)
  std::vector<cv::Point2f> cam_norm, proj_norm;
  cv::undistortPoints(cam_pts, cam_norm, 
                      calib_.cam_intrinsics, calib_.cam_dist_coeffs);
  cv::undistortPoints(proj_pts, proj_norm, 
                      calib_.proj_intrinsics, calib_.proj_dist_coeffs);

  // 2. 投影行列 (Projection Matrices)
  cv::Mat P_cam = cv::Mat::eye(3, 4, CV_64F); 
  cv::Mat P_proj(3, 4, CV_64F);
  calib_.rotation.copyTo(P_proj(cv::Rect(0, 0, 3, 3)));
  calib_.translation.copyTo(P_proj(cv::Rect(3, 0, 1, 3)));

  // 3. 三角測量
  cv::Mat pts4d;
  cv::triangulatePoints(P_cam, P_proj, cam_norm, proj_norm, pts4d);

  // 4. 3D座標変換
  int valid_points = 0;
  for (size_t i = 0; i < pixels.size(); ++i) {
    float w = pts4d.at<float>(3, i);
    if (std::abs(w) > 1e-6) {
      float X = pts4d.at<float>(0, i) / w;
      float Y = pts4d.at<float>(1, i) / w;
      float Z = pts4d.at<float>(2, i) / w;
      
      points3d.at<cv::Vec3f>(pixels[i].y, pixels[i].x) = cv::Vec3f(X, Y, Z);
      valid_points++;
    }
  }

  LOG_INFO("3D復元完了: 有効点数={} / 全画素数={}", valid_points, disparity_map_.total());
  return points3d;
}

// --- 設定 / プロパティ ---

void StructuredLight::setCalibration(const SystemCalibration& calib) {
  calib_ = calib;
  LOG_INFO("キャリブレーションデータを設定: ready={}", calib_.ready);
}

void StructuredLight::setBlackThreshold(int value) {
  black_threshold_ = static_cast<size_t>(value);
}

void StructuredLight::setWhiteThreshold(int value) {
  white_threshold_ = static_cast<size_t>(value);
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
