#include "video/camera_manager.hpp"
#include "logger/logger_macros.hpp"
#include <algorithm>

namespace video {

CameraManager::~CameraManager() {
  clear();
}

CameraId CameraManager::createCamera(const CameraOptions& options, const std::string& name) {
  CameraId new_id = next_id_++;

  // Cameraインスタンス生成 (コンストラクタで options と ID を渡す想定)
  // ※ Cameraクラスのコンストラクタ署名に合わせて調整してください
  auto cam = std::make_unique<Camera>(options, new_id, name);

  if (!cam->open()) {
    LOG_ERROR("CameraManager: カメラオープン失敗 index={}, name='{}'", 
              options.device_index, name);
    return kInvalidCameraId;
  }

  LOG_INFO("CameraManager: カメラ作成成功 id={}, index={}, name='{}'", 
           new_id, options.device_index, name);

  cameras_.push_back(std::move(cam));
  return new_id;
}

Camera* CameraManager::get(CameraId id) const {
  auto it = std::find_if(cameras_.begin(), cameras_.end(),
                         [id](const auto& c) { return c->id() == id; });
  if (it != cameras_.end()) {
    return it->get();
  }
  return nullptr;
}

void CameraManager::forEach(std::function<void(Camera&)> action) {
  for (auto& c : cameras_) {
    if (c) action(*c);
  }
}

void CameraManager::clear() {
  LOG_INFO("CameraManager: 全カメラを解放します");
  for (auto& c : cameras_) {
    if (c && c->isOpened()) {
      c->close();
    }
  }
  cameras_.clear();
  // IDカウンタはリセットしても良いし、継続させても良い（ここでは継続）
}

} // namespace video
