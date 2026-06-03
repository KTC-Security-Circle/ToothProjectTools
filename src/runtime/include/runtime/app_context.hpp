#pragma once

#include "window/window_manager.hpp"
#include "video/camera_manager.hpp"
#include "structured_light/structured_light.hpp"
#include "calibration/calibrator.hpp"
#include "input/input_handler.hpp"
#include "cmd/dispatch_cmd.hpp"
#include "calibration/stereo_calibrator.hpp"

#include <vector>
#include <memory>
#include <map>
#include <deque> // ★追加

namespace runtime {

// アプリケーション全体で共有する状態
struct AppContext {
    // --- Managers & Core Systems ---
    win::WindowManager   win_mgr;
    video::CameraManager cam_mgr;
    input::InputHandler  input;
    
    // ★修正: 型を std::deque<DispatchCmd> に明示
    std::deque<DispatchCmd> cmd_que; 

    // --- Modules ---
    std::unique_ptr<sl::StructuredLight> sl_system;
    std::unique_ptr<calib::Calibrator>   calibrator;

    // ★追加: ステレオキャリブレーション機能
    std::unique_ptr<calib::StereoCalibrator> stereo_calibrator;
    
    // ★追加: 計算結果 (3D復元時に使用する)
    calib::StereoData stereo_data;

    // --- State / Flags ---
    bool running = true;
    bool show_chess_corners = true;
    bool skip_render_once = false;

    // --- IDs (Relation) ---
    win::WindowId id_preview   = win::kInvalidWindowId;
    win::WindowId id_second    = win::kInvalidWindowId;
    win::WindowId id_projector = win::kInvalidWindowId;
    win::WindowId focused_id   = win::kInvalidWindowId;

    video::CameraId id_cam1 = video::kInvalidCameraId;
    video::CameraId id_cam2 = video::kInvalidCameraId;

    // スキャンに使用するカメラID
    video::CameraId scan_cam_id_left  = video::kInvalidCameraId;
    video::CameraId scan_cam_id_right = video::kInvalidCameraId;

    // カメラID -> 表示先ウィンドウID のマップ
    std::map<video::CameraId, win::WindowId> cam_to_win;

    // マウスコールバック用コンテキスト
    struct MouseCtx {
        AppContext* ctx;
        win::WindowId wid;
    };
    std::vector<MouseCtx> mouse_contexts;

    // --- Data Buffers (Scan) ---
    std::vector<cv::Mat> scanned_imgs_left;
    std::vector<cv::Mat> scanned_imgs_right;
    int scan_interval_ms = 500;
};

} // namespace runtime