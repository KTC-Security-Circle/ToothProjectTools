#pragma once
#include "camera.hpp"
#include "video_types.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace video {

class CameraManager {
public:
  CameraManager() = default;
  ~CameraManager(); // デストラクタで全カメラcloseなど

  // コピー禁止
  CameraManager(const CameraManager&) = delete;
  CameraManager& operator=(const CameraManager&) = delete;

  /**
   * @brief カメラを開き、管理下に置く
   * @param options デバイス設定
   * @param name 識別用ネーム（ログ用）
   * @return 成功時はID, 失敗時は kInvalidCameraId
   */
  CameraId createCamera(const CameraOptions& options, const std::string& name = "Camera");

  /**
   * @brief IDからカメラを取得
   */
  Camera* get(CameraId id) const;

  /**
   * @brief 指定したカメラを閉じて管理対象から外す
   */
  bool remove(CameraId id);

  /**
   * @brief 全カメラに対して処理を行う（フレーム取得ループなどで使用）
   */
  void forEach(std::function<void(Camera&)> action);

  /**
   * @brief 全カメラを閉じて破棄する
   */
  void clear();

private:
  std::vector<std::unique_ptr<Camera>> cameras_;
  CameraId next_id_{1};
};

} // namespace video
