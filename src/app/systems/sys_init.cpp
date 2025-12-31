#include "app/systems.hpp"
#include "app/dispatch.hpp" // setup_bindingsのために必要
#include "logger/logger_macros.hpp"
#include "video/camera.hpp"
#include "input/bindings_default.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <filesystem>
#include <iomanip>

namespace app::sys {

void setup_app(AppContext& ctx) {
    LOG_INFO("System: 初期化を開始します");

    // 1. ウィンドウ生成
    ctx.id_preview = ctx.win_mgr.createWindow("Preview", {800, 600}, {100, 300});
    if (auto* w = ctx.win_mgr.get(ctx.id_preview)) {
        w->setMonitorIndex(1);
        ctx.focused_id = ctx.id_preview;
    }

    ctx.id_second = ctx.win_mgr.createWindow("Second", {800, 600}, {900, 300});
    if (auto* w = ctx.win_mgr.get(ctx.id_second)) {
        w->setMonitorIndex(1);
    }

    ctx.id_projector = ctx.win_mgr.createWindow("Projector", {0, 0}, {0, 0});
    
    int proj_w = 1920;
    int proj_h = 1080;
    if (auto* w = ctx.win_mgr.get(ctx.id_projector)) {
        w->setMonitorIndex(2);
        auto size = w->getMonitorSize();
        if (size.width > 0) { proj_w = size.width; proj_h = size.height; }
        w->resize({proj_w, proj_h});
    }

    // 2. Structured Light
    LOG_INFO("StructuredLight初期化: {}x{}", proj_w, proj_h);
    ctx.sl_system = std::make_unique<sl::StructuredLight>(proj_w, proj_h);
    ctx.sl_system->generatePatterns();

    // 3. 初期画像設定
    auto set_text = [&](win::WindowId wid, const std::string& txt) {
        if (auto* w = ctx.win_mgr.get(wid)) {
            cv::Mat img(w->size().height, w->size().width, CV_8UC3, cv::Scalar(30, 30, 30));
            cv::putText(img, txt, {40, 300}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {200, 200, 255}, 3);
            w->setImage(std::move(img));
        }
    };
    set_text(ctx.id_preview, "Hello Preview");
    set_text(ctx.id_second, "Hello Second");
    
    if (auto* w = ctx.win_mgr.get(ctx.id_projector)) {
        cv::Mat img(proj_h, proj_w, CV_8UC3, cv::Scalar(0,0,0));
        cv::putText(img, "Projector Ready", {100, proj_h/2}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {255, 255, 255}, 3);
        w->setImage(std::move(img));
    }

    // 4. カメラ初期化
    video::CameraOptions opt1; opt1.device_index = 4;
    ctx.id_cam1 = ctx.cam_mgr.createCamera(opt1, "CamLeft");
    if (ctx.id_cam1 != video::kInvalidCameraId) ctx.cam_to_win[ctx.id_cam1] = ctx.id_preview;

    video::CameraOptions opt2; opt2.device_index = 6;
    ctx.id_cam2 = ctx.cam_mgr.createCamera(opt2, "CamRight");
    if (ctx.id_cam2 != video::kInvalidCameraId) ctx.cam_to_win[ctx.id_cam2] = ctx.id_second;

    ctx.scan_cam_id_left  = ctx.id_cam1;
    ctx.scan_cam_id_right = ctx.id_cam2;

    // 5. Calibrator
    ctx.calibrator = std::make_unique<calib::Calibrator>();
    ctx.calibrator->setBoardConfig({cv::Size(10, 7), 10.0f});

    ctx.stereo_calibrator = std::make_unique<calib::StereoCalibrator>();
    // 単眼と同じボード設定にする
    ctx.stereo_calibrator->setBoardConfig({cv::Size(10, 7), 10.0f});

    // 6. 表示確定 & マウス設定
    ctx.win_mgr.forEach([](win::Window& w){ w.present(); });
    #if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
      cv::pollKey();
    #else
      cv::waitKey(1);
    #endif

    ctx.mouse_contexts.reserve(ctx.win_mgr.count());
    ctx.win_mgr.forEach([&](win::Window& w){
        ctx.mouse_contexts.push_back({&ctx, static_cast<win::WindowId>(w.id())});
        cv::setMouseCallback(w.name(), app::sys::on_mouse_event, &ctx.mouse_contexts.back());
    });

    const std::string dir_mono_L   = "captures/mono_L";
    const std::string dir_mono_R   = "captures/mono_R";
    const std::string dir_stereo_L = "captures/stereo_L";
    const std::string dir_stereo_R = "captures/stereo_R";

    // 設定構造体を作成
    input::CalibrationBindConfig calib_cfg;
    calib_cfg.scan_cam_left    = ctx.scan_cam_id_left;
    calib_cfg.scan_cam_right   = ctx.scan_cam_id_right;
    calib_cfg.dir_mono_left    = dir_mono_L;
    calib_cfg.dir_mono_right   = dir_mono_R;
    calib_cfg.dir_stereo_left  = dir_stereo_L;
    calib_cfg.dir_stereo_right = dir_stereo_R;

    // フォーカス連動ヘルパー (Mono用)
    // ※ ここでは dir_mono_L/R を使う
    auto get_focused_mono = [&ctx, dir_mono_L, dir_mono_R]() -> std::pair<video::CameraId, std::string> {
        if (ctx.scan_cam_id_left != video::kInvalidCameraId) {
            if (ctx.cam_to_win[ctx.scan_cam_id_left] == ctx.focused_id) return {ctx.scan_cam_id_left, dir_mono_L};
        }
        if (ctx.scan_cam_id_right != video::kInvalidCameraId) {
            if (ctx.cam_to_win[ctx.scan_cam_id_right] == ctx.focused_id) return {ctx.scan_cam_id_right, dir_mono_R};
        }
        return {video::kInvalidCameraId, ""};
    };

    // バインド登録
    input::install_default_bindings(
        ctx.input,
        ctx.cmd_que,
        ctx.id_projector,
        calib_cfg,
        get_focused_mono
    );

    // [T] Overlay Toggle
    ctx.input.bind('t', [&ctx](){
        ctx.show_chess_corners = !ctx.show_chess_corners;
        LOG_INFO("Checkers: {}", ctx.show_chess_corners ? "ON" : "OFF");
    });

    LOG_INFO("System: 初期化完了");
}

} // namespace