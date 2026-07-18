#include "capture/capture_service.hpp"

#include "logger/logger_macros.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp"

#include <exception>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

namespace capture
{

std::string toString(CaptureErrorCode code)
{
    switch (code)
    {
    case CaptureErrorCode::CameraNotFound:
        return "CameraNotFound";
    case CaptureErrorCode::CameraNotOpen:
        return "CameraNotOpen";
    case CaptureErrorCode::EmptyFrame:
        return "EmptyFrame";
    case CaptureErrorCode::InvalidOutputPath:
        return "InvalidOutputPath";
    case CaptureErrorCode::DirectoryCreateFailed:
        return "DirectoryCreateFailed";
    case CaptureErrorCode::FileWriteFailed:
        return "FileWriteFailed";
    case CaptureErrorCode::InternalError:
        return "InternalError";
    }
    return "InternalError";
}

CaptureService::CaptureService(video::CameraManager& cameras)
    : cameras_(cameras)
{
}

CaptureResult CaptureService::captureFrame(video::CameraId camera_id, const std::filesystem::path& output_path)
{
    CaptureResult result;
    result.output_path = output_path;

    if (output_path.empty())
    {
        result.error = CaptureError{CaptureErrorCode::InvalidOutputPath, "保存先pathが空です"};
        return result;
    }

    auto* camera = cameras_.get(camera_id);
    if (!camera)
    {
        result.error = CaptureError{CaptureErrorCode::CameraNotFound, "指定cameraが見つかりません"};
        return result;
    }

    if (!camera->isOpened())
    {
        result.error = CaptureError{CaptureErrorCode::CameraNotOpen, "指定cameraがopenされていません"};
        return result;
    }

    cv::Mat frame = camera->getFrame();
    if (frame.empty())
    {
        result.error = CaptureError{CaptureErrorCode::EmptyFrame, "有効なframeがまだありません"};
        return result;
    }

    if (auto error = ensureParentDirectory(output_path))
    {
        result.error = error;
        return result;
    }

    try
    {
        if (!cv::imwrite(output_path.string(), frame))
        {
            result.error = CaptureError{CaptureErrorCode::FileWriteFailed, "画像ファイルの保存に失敗しました"};
            return result;
        }
    }
    catch (const std::exception& e)
    {
        result.error = CaptureError{CaptureErrorCode::FileWriteFailed, e.what()};
        return result;
    }

    result.ok = true;
    LOG_INFO("Capture: saved {}", output_path.string());
    return result;
}

CaptureStereoResult CaptureService::captureStereo(video::CameraId left_camera_id,
                                                  video::CameraId right_camera_id,
                                                  const std::filesystem::path& left_output_path,
                                                  const std::filesystem::path& right_output_path)
{
    CaptureStereoResult result;
    result.left_output_path = left_output_path;
    result.right_output_path = right_output_path;

    if (left_output_path.empty() || right_output_path.empty())
    {
        result.error = CaptureError{CaptureErrorCode::InvalidOutputPath, "左右いずれかの保存先pathが空です"};
        return result;
    }

    auto* left_camera = cameras_.get(left_camera_id);
    auto* right_camera = cameras_.get(right_camera_id);
    if (!left_camera || !right_camera)
    {
        result.error = CaptureError{CaptureErrorCode::CameraNotFound, "左右いずれかのcameraが見つかりません"};
        return result;
    }

    if (!left_camera->isOpened() || !right_camera->isOpened())
    {
        result.error = CaptureError{CaptureErrorCode::CameraNotOpen, "左右いずれかのcameraがopenされていません"};
        return result;
    }

    cv::Mat left_frame = left_camera->getFrame();
    cv::Mat right_frame = right_camera->getFrame();
    if (left_frame.empty() || right_frame.empty())
    {
        result.error = CaptureError{CaptureErrorCode::EmptyFrame, "左右いずれかの有効なframeがまだありません"};
        return result;
    }

    if (auto error = ensureParentDirectory(left_output_path))
    {
        result.error = error;
        return result;
    }
    if (auto error = ensureParentDirectory(right_output_path))
    {
        result.error = error;
        return result;
    }

    try
    {
        if (!cv::imwrite(left_output_path.string(), left_frame))
        {
            result.error = CaptureError{CaptureErrorCode::FileWriteFailed, "左画像ファイルの保存に失敗しました"};
            return result;
        }
        if (!cv::imwrite(right_output_path.string(), right_frame))
        {
            result.error = CaptureError{CaptureErrorCode::FileWriteFailed, "右画像ファイルの保存に失敗しました"};
            return result;
        }
    }
    catch (const std::exception& e)
    {
        result.error = CaptureError{CaptureErrorCode::FileWriteFailed, e.what()};
        return result;
    }

    result.ok = true;
    LOG_INFO("Capture: saved stereo left={} right={}", left_output_path.string(), right_output_path.string());
    return result;
}

std::optional<CaptureError> CaptureService::ensureParentDirectory(const std::filesystem::path& output_path) const
{
    const auto parent_path = output_path.parent_path();
    if (parent_path.empty())
    {
        return std::nullopt;
    }

    try
    {
        std::filesystem::create_directories(parent_path);
    }
    catch (const std::exception& e)
    {
        return CaptureError{CaptureErrorCode::DirectoryCreateFailed, e.what()};
    }

    if (!std::filesystem::exists(parent_path) || !std::filesystem::is_directory(parent_path))
    {
        return CaptureError{CaptureErrorCode::DirectoryCreateFailed, "保存先の親directoryを作成できませんでした"};
    }

    return std::nullopt;
}

} // namespace capture
