#pragma once

#include <string>
#include <vector>

namespace scan::dataset
{

struct ScanDatasetIssue
{
    /// code <std::string>: issue code。
    std::string code;

    /// message <std::string>: issue説明。
    std::string message;

    /// path <std::string>: 関連path。不要な場合は空。
    std::string path;

    /// pattern_index <int>: 関連pattern index。該当しない場合は-1。
    int pattern_index{-1};
};

struct ScanDatasetMetadata
{
    /// scan_id <std::string>: scan session識別子。
    std::string scan_id;

    /// projector_role <std::string>: 使用projector role名。
    std::string projector_role;

    /// left_role <std::string>: 左camera role名。
    std::string left_role;

    /// right_role <std::string>: 右camera role名。
    std::string right_role;

    /// output_dir <std::string>: metadata上の出力directory。
    std::string output_dir;

    /// pattern_count <int>: expected pattern count。
    int pattern_count{0};

    /// projector_width <int>: Decode用Gray Code論理解像度の幅。
    int projector_width{0};

    /// projector_height <int>: Decode用Gray Code論理解像度の高さ。
    int projector_height{0};

    /// surface_width <int>: projector surface width。
    int surface_width{0};

    /// surface_height <int>: projector surface height。
    int surface_height{0};

    /// pattern_width <int>: active pattern width。
    int pattern_width{0};

    /// pattern_height <int>: active pattern height。
    int pattern_height{0};

    /// pattern_x <int>: active pattern origin X。
    int pattern_x{0};

    /// pattern_y <int>: active pattern origin Y。
    int pattern_y{0};

    /// display_width <int>: surface内の表示領域横幅。
    int display_width{0};

    /// display_height <int>: surface内の表示領域縦幅。
    int display_height{0};

    /// display_x <int>: surface内の表示領域左上X座標。
    int display_x{0};

    /// display_y <int>: surface内の表示領域左上Y座標。
    int display_y{0};

    std::string sync;
    std::string photodiode_device;
    int photodiode_baud{0};
    int sync_timeout_ms{0};
    int sync_guard_ms{0};
};

struct ScanDatasetValidationResult
{
    /// ok <bool>: validation処理自体が成功したか。
    bool ok{true};

    /// input_dir <std::string>: 検証対象directory。
    std::string input_dir;

    /// scan_id <std::string>: metadataから読んだscan id。
    std::string scan_id;

    /// valid <bool>: datasetがdecode入力として有効か。
    bool valid{false};

    /// partial <bool>: 欠損画像があるが一部は存在する状態ならtrue。
    bool partial{false};

    /// pattern_count <int>: metadata上のexpected pattern count。
    int pattern_count{0};

    /// left_count <int>: 読み込み可能なleft画像数。
    int left_count{0};

    /// right_count <int>: 読み込み可能なright画像数。
    int right_count{0};

    /// missing_count <int>: 欠損しているexpected image pair数。
    int missing_count{0};

    /// width <int>: capture画像幅。未確定なら0。
    int width{0};

    /// height <int>: capture画像高さ。未確定なら0。
    int height{0};

    /// issues <std::vector<ScanDatasetIssue>>: 検証issue一覧。
    std::vector<ScanDatasetIssue> issues;
};

} // namespace scan::dataset
