#pragma once

#include <optional>
#include <string>

namespace service::scan
{

enum class ScanState
{
    idle,
    running,
    stopping,
    completed,
    failed,
    stopped,
};

std::string toString(ScanState state);

struct ScanError
{
    /// code <std::string>: scan command失敗時のerror code。
    std::string code;

    /// message <std::string>: scan command失敗時のerror message。
    std::string message;
};

struct ScanResult
{
    /// ok <bool>: scan commandが成功したか。
    bool ok{false};

    /// scan_id <std::string>: scan session識別子。
    std::string scan_id;

    /// status <ScanState>: scan状態。
    ScanState status{ScanState::idle};

    /// projector_role <std::string>: 使用projector role名。
    std::string projector_role;

    /// left_role <std::string>: 左camera role名。
    std::string left_role;

    /// right_role <std::string>: 右camera role名。
    std::string right_role;

    /// output_dir <std::string>: scan dataset保存先directory。
    std::string output_dir;

    /// pattern_count <int>: 撮影対象pattern数。
    int pattern_count{0};

    /// captured_count <int>: 撮影済みpattern数。
    int captured_count{0};

    /// current_index <int>: 現在処理中のpattern index。
    int current_index{-1};

    /// error <std::optional<ScanError>>: 失敗時のerror情報。
    std::optional<ScanError> error;

    static ScanResult success(std::string scan_id, ScanState status);
    static ScanResult failure(std::string scan_id, std::string code, std::string message);
};

} // namespace service::scan
