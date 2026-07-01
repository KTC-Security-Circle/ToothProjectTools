#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace capture
{

/// @brief Capture処理の失敗理由を表すerror code。
enum class CaptureErrorCode
{
    CameraNotFound,
    CameraNotOpen,
    EmptyFrame,
    InvalidOutputPath,
    DirectoryCreateFailed,
    FileWriteFailed,
    InternalError,
};

/// @brief Capture処理の失敗内容。
struct CaptureError
{
    /// code <CaptureErrorCode>: Capture処理の失敗理由を表すerror code。
    CaptureErrorCode code{CaptureErrorCode::InternalError};

    /// message <std::string>: 失敗内容を説明するmessage。
    std::string message;
};

/// @brief 単一cameraのCapture結果。
struct CaptureResult
{
    /// ok <bool>: Captureと画像保存が成功したかどうか。
    bool ok{false};

    /// error <std::optional<CaptureError>>: 失敗時のerror情報。
    std::optional<CaptureError> error;

    /// output_path <std::filesystem::path>: 保存された画像ファイルのpath。
    std::filesystem::path output_path;
};

/// @brief stereo cameraのCapture結果。
struct CaptureStereoResult
{
    /// ok <bool>: 左右cameraのCaptureと画像保存が成功したかどうか。
    bool ok{false};

    /// error <std::optional<CaptureError>>: 失敗時のerror情報。
    std::optional<CaptureError> error;

    /// left_output_path <std::filesystem::path>: 左camera画像ファイルの保存path。
    std::filesystem::path left_output_path;

    /// right_output_path <std::filesystem::path>: 右camera画像ファイルの保存path。
    std::filesystem::path right_output_path;
};

/// @brief CaptureErrorCodeをlogやresponse用の文字列へ変換する。
///
/// Args:
///   code <CaptureErrorCode>: 文字列へ変換するCapture error code。
///
/// Return:
///   <std::string>: Capture error codeを表す文字列。
std::string toString(CaptureErrorCode code);

} // namespace capture
