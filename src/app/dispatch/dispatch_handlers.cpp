// =============================================================================
//  src/app/dispatch/dispatch_handlers.cpp
//  各コマンドの実装ロジック
// =============================================================================

#include "app/dispatch.hpp"
#include "logger/logger_macros.hpp"
#include "video/camera.hpp"

#include <opencv2/imgcodecs.hpp> // ★追加: imwrite用
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm> 

namespace fs = std::filesystem;

namespace app::dispatch::handlers {

// -----------------------------------------------------------------------------
// グローバルコマンド (アプリ全体の状態変更)
// -----------------------------------------------------------------------------
bool handle_global(AppContext& ctx, const cmd::Command& command) {
    bool handled = false;

    std::visit([&](auto&& c){
        using T = std::decay_t<decltype(c)>;

        if constexpr (std::is_same_v<T, cmd::CmdFocusNext>) {
            LOG_INFO("コマンド: フォーカス移動（次）");
            
            std::vector<win::WindowId> ids;
            ctx.win_mgr.forEach([&](win::Window& w) {
                if (w.visible()) ids.push_back(w.id());
            });

            if (!ids.empty()) {
                std::sort(ids.begin(), ids.end());
                auto it = std::find(ids.begin(), ids.end(), ctx.focused_id);
                if (it == ids.end() || std::next(it) == ids.end()) {
                    ctx.focused_id = ids[0];
                } else {
                    ctx.focused_id = *std::next(it);
                }
                LOG_INFO("フォーカス切り替え: New ID={}", ctx.focused_id);
            }
            handled = true;
        }
        else if constexpr (std::is_same_v<T, cmd::CmdQuit>) {
            LOG_INFO("コマンド: 終了要求");
            ctx.running = false;
            handled = true;
        }
    }, command);

    return handled;
}

// -----------------------------------------------------------------------------
// ウィンドウレベルコマンド
// -----------------------------------------------------------------------------
void handle_window(AppContext& ctx, win::Window& target_window, const cmd::Command& command) {
    std::visit([&](auto&& c){
        using T = std::decay_t<decltype(c)>;

        if constexpr (std::is_same_v<T, cmd::CmdToggleFullscreen>) {
            target_window.setFullscreen(!target_window.fullscreen());
            LOG_INFO("FS切替: {}", target_window.name());
        }
        else if constexpr (std::is_same_v<T, cmd::CmdMoveToMonitor>) {
            target_window.setMonitorIndex(c.index);
            LOG_INFO("モニタ移動: {} -> {}", target_window.name(), c.index);
        }
        else if constexpr (std::is_same_v<T, cmd::CmdShowPattern>) {
            if (ctx.sl_system) {
                ctx.sl_system->setIndex(c.index);
                target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                LOG_INFO("パターン指定: index={}", ctx.sl_system->getCurrentIndex());
            }
        }
        else if constexpr (std::is_same_v<T, cmd::CmdNextPattern>) {
            if (ctx.sl_system) {
                ctx.sl_system->nextPattern(true);
                target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                LOG_INFO("パターン(次): index={}", ctx.sl_system->getCurrentIndex());
            }
        }
        else if constexpr (std::is_same_v<T, cmd::CmdPrevPattern>) {
            if (ctx.sl_system) {
                ctx.sl_system->prevPattern(true);
                target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                LOG_INFO("パターン(前): index={}", ctx.sl_system->getCurrentIndex());
            }
        }
        else if constexpr (std::is_same_v<T, cmd::CmdStartScan>) {
            if (!ctx.sl_system) {
                LOG_ERROR("Scan開始失敗: StructuredLight未初期化");
                return;
            }
            ctx.scan_interval_ms = c.interval_ms;
            ctx.scanned_imgs_left.clear();
            ctx.scanned_imgs_right.clear();
            
            ctx.sl_system->startScan(); 
            target_window.setImage(ctx.sl_system->getCurrentPatternImage());
            
            LOG_INFO("=== 自動スキャン開始 (間隔: {}ms) ===", ctx.scan_interval_ms);
        }
        else if constexpr (std::is_same_v<T, cmd::CmdStopScan>) {
            if (ctx.sl_system && ctx.sl_system->isScanning()) {
                ctx.sl_system->stopScan();
                LOG_INFO("=== 自動スキャン中断 ===");
            }
        }
        else if constexpr (std::is_same_v<T, cmd::CmdCalibClear>) {
            const auto& dir = c.target_directory;
            if (!dir.empty()) {
                try {
                    fs::remove_all(dir);
                    fs::create_directories(dir);
                    LOG_INFO("Calib: フォルダクリア完了 '{}'", dir);
                } catch (const std::exception& e) {
                    LOG_ERROR("Calib: クリア失敗: {}", e.what());
                }
            }
        }
        else if constexpr (std::is_same_v<T, cmd::CmdCalibCapture>) {
            if (c.camera_id == video::kInvalidCameraId) return;

            if (!fs::exists(c.target_directory)) fs::create_directories(c.target_directory);
            auto cnt = std::distance(fs::directory_iterator(c.target_directory), {});
            
            if (auto* cam = ctx.cam_mgr.get(c.camera_id)) {
                cv::Mat frame = cam->getFrame();
                if (!frame.empty()) {
                    std::stringstream ss;
                    ss << c.target_directory << "/" << c.prefix 
                       << std::setfill('0') << std::setw(3) << cnt << ".png";
                    
                    if (cv::imwrite(ss.str(), frame)) {
                        LOG_INFO("Saved[{}]: {} (Total:{})", cam->name(), ss.str(), cnt + 1);
                    } else {
                        LOG_ERROR("Save Failed: {}", ss.str());
                    }
                }
            }
        }
        else if constexpr (std::is_same_v<T, cmd::CmdCalibrate>) {
            if (!ctx.calibrator) {
                LOG_ERROR("Calib: Calibrator未初期化");
                return;
            }
            const auto& folder = c.image_folder;
            std::vector<std::string> files;
            
            if (fs::exists(folder)) {
                for (const auto& entry : fs::directory_iterator(folder)) {
                    if (entry.is_regular_file()) files.push_back(entry.path().string());
                }
            }

            if (files.empty()) {
                LOG_WARN("Calib: 画像なし ({})", folder);
                return;
            }

            cv::Mat K, D;
            double rms = ctx.calibrator->runCalibration(files, K, D);
            
            if (rms > 0 && rms < 1.0) {
                if (auto* cam = ctx.cam_mgr.get(c.target_camera_id)) {
                    cam->setIntrinsics(K);
                    cam->setDistCoeffs(D);
                    LOG_INFO("Calib: 成功 (RMS={}) -> 適用: {}", rms, cam->name());
                }
            } else {
                LOG_WARN("Calib: 精度不足 or 失敗 (RMS={})", rms);
            }
        }
        else if constexpr (std::is_same_v<T, cmd::CmdCapturePush>) {
            video::CameraId cid = video::kInvalidCameraId;
            if (c.camera_id) {
                cid = static_cast<video::CameraId>(*c.camera_id);
            } else {
                for (auto& [cam_id, win_id] : ctx.cam_to_win) {
                    if (win_id == target_window.id()) {
                        cid = cam_id;
                        break;
                    }
                }
            }

            if (cid != video::kInvalidCameraId) {
                if (auto* cam = ctx.cam_mgr.get(cid)) {
                    cv::Mat frame = cam->getFrame();
                    if (!frame.empty()) {
                        std::stringstream ss;
                        ss << "captures/snap_" << cid << "_" 
                           << std::chrono::steady_clock::now().time_since_epoch().count() << ".png";
                        cv::imwrite(ss.str(), frame);
                        LOG_INFO("Capture: Saved {}", ss.str());
                    }
                }
            }
        } else if constexpr (std::is_same_v<T, cmd::CmdStereoCalibrate>) {
            if (!ctx.stereo_calibrator) {
                LOG_ERROR("Stereo: Calibrator未初期化");
                return;
            }

            // 1. カメラ取得 (内部パラメータK, Dが必要)
            auto* camL = ctx.cam_mgr.get(c.left_cam_id);
            auto* camR = ctx.cam_mgr.get(c.right_cam_id);

            if (!camL || !camR) {
                LOG_ERROR("Stereo: カメラが見つかりません");
                return;
            }

            // 単眼パラメータの取得チェック
            cv::Mat K1 = camL->intrinsics();
            cv::Mat D1 = camL->distCoeffs();
            cv::Mat K2 = camR->intrinsics();
            cv::Mat D2 = camR->distCoeffs();

            if (K1.empty() || K2.empty()) {
                LOG_ERROR("Stereo: 事前に単眼キャリブレーション(Kキー)を実行してください");
                return;
            }

            // 2. 画像ファイルリスト作成
            auto get_files = [](const std::string& dir) {
                std::vector<std::string> files;
                if (fs::exists(dir)) {
                    for (const auto& entry : fs::directory_iterator(dir)) {
                        if (entry.is_regular_file()) files.push_back(entry.path().string());
                    }
                    std::sort(files.begin(), files.end()); // 順序合わせのためソート必須
                }
                return files;
            };

            auto filesL = get_files(c.left_dir);
            auto filesR = get_files(c.right_dir);

            if (filesL.empty() || filesR.empty() || filesL.size() != filesR.size()) {
                LOG_ERROR("Stereo: 画像枚数不一致または空 (L:{} != R:{})", filesL.size(), filesR.size());
                return;
            }

            // 3. 計算実行
            LOG_INFO("Stereo: 計算開始 ({} pairs)...", filesL.size());
            
            calib::StereoData result;
            double rms = ctx.stereo_calibrator->run(
                filesL, filesR,
                K1, D1, K2, D2,
                result
            );

            if (rms > 0 && result.valid) {
                LOG_INFO("Stereo: 成功! RMS={}", rms);
                
                // 結果をコンテキストに保存
                ctx.stereo_data = result;

                // 4. ファイルへ保存 (YAML)
                try {
                    cv::FileStorage fs(c.output_file, cv::FileStorage::WRITE);
                    if (fs.isOpened()) {
                        fs << "RMS" << rms;
                        fs << "K1" << K1 << "D1" << D1;
                        fs << "K2" << K2 << "D2" << D2;
                        fs << "R" << result.R << "T" << result.T;
                        fs << "Q" << result.Q; // 3D復元で最も重要
                        fs.release();
                        LOG_INFO("Stereo: 結果を保存しました -> {}", c.output_file);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Stereo: 保存失敗 {}", e.what());
                }

            } else {
                LOG_ERROR("Stereo: 計算失敗");
            }
        }

    }, command);
}

} // namespace