#include "control/json_line_reader.hpp"

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>

namespace control {
namespace {

bool readString(
    const cv::FileNode& root,
    const char* name,
    std::optional<std::string>& value,
    std::string& error_message) {
  const auto node = root[name];
  if (node.empty()) {
    return true;
  }
  if (!node.isString()) {
    error_message = std::string("field must be a string: ") + name;
    return false;
  }
  value = static_cast<std::string>(node);
  return true;
}

bool readInteger(
    const cv::FileNode& root,
    const char* name,
    std::optional<int>& value,
    std::string& error_message) {
  const auto node = root[name];
  if (node.empty()) {
    return true;
  }
  if (!node.isInt()) {
    error_message = std::string("field must be an integer: ") + name;
    return false;
  }
  value = static_cast<int>(node);
  return true;
}

bool readNumber(const cv::FileNode& root,const char* name,std::optional<double>& value,std::string& error_message) { const auto node=root[name]; if(node.empty()) return true; if(!node.isInt()&&!node.isReal()){error_message=std::string("field must be a number: ")+name;return false;} value=(double)node; return true; }

bool readBool(
    const cv::FileNode& root,
    const char* name,
    std::optional<bool>& value,
    std::string& error_message) {
  const auto node = root[name];
  if (node.empty()) {
    return true;
  }
  if (!node.isInt()) {
    error_message = std::string("field must be a boolean: ") + name;
    return false;
  }
  const auto raw = static_cast<int>(node);
  if (raw != 0 && raw != 1) {
    error_message = std::string("field must be a boolean: ") + name;
    return false;
  }
  value = raw != 0;
  return true;
}

} // namespace

JsonLineReader::JsonLineReader(std::istream& input) : input_(input) {}

ReadResult JsonLineReader::read() {
  std::string line;
  if (!std::getline(input_, line)) {
    ReadResult result;
    result.status = ReadStatus::end_of_input;
    return result;
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  if (line.empty()) {
    return ReadResult{
        ReadStatus::invalid, {}, std::nullopt, "invalid_json", "empty JSON line"};
  }

  try {
    cv::FileStorage storage(
        line,
        cv::FileStorage::READ | cv::FileStorage::MEMORY |
            cv::FileStorage::FORMAT_JSON);
    if (!storage.isOpened()) {
      return ReadResult{
          ReadStatus::invalid, {}, std::nullopt, "invalid_json", "failed to parse JSON"};
    }

    const auto root = storage.root();
    if (root.empty() || !root.isMap()) {
      return ReadResult{
          ReadStatus::invalid, {}, std::nullopt, "invalid_json", "JSON command must be an object"};
    }

    ControlMessage message;
    std::string error_message;
    if (!readString(root, "id", message.id, error_message) ||
        !readString(root, "cmd", message.cmd, error_message) ||
        !readString(root, "role", message.role, error_message) ||
        !readString(root, "output", message.output, error_message) ||
        !readString(root, "left_role", message.left_role, error_message) ||
        !readString(root, "right_role", message.right_role, error_message) ||
        !readString(root, "left_output", message.left_output, error_message) ||
        !readString(root, "right_output", message.right_output, error_message) ||
        !readString(root, "image_folder", message.image_folder, error_message) ||
        !readString(root, "left_dir", message.left_dir, error_message) ||
        !readString(root, "right_dir", message.right_dir, error_message) ||
        !readString(root, "image_folder_left", message.left_dir, error_message) ||
        !readString(root, "image_folder_right", message.right_dir, error_message) ||
        !readString(root, "left_image_folder", message.left_dir, error_message) ||
        !readString(root, "right_image_folder", message.right_dir, error_message) ||
        !readString(root, "output_file", message.output_file, error_message) ||
        !readString(root, "left_calibration_file", message.left_calibration_file, error_message) ||
        !readString(root, "right_calibration_file", message.right_calibration_file, error_message) ||
        !readString(root, "metadata_file", message.metadata_file, error_message) ||
        !readString(root, "scan_id", message.scan_id, error_message) ||
        !readString(root, "output_dir", message.output_dir, error_message) ||
        !readString(root, "input_dir", message.input_dir, error_message) ||
        !readBool(root, "allow_partial", message.allow_partial, error_message) ||
        !readBool(root, "apply_to_camera", message.apply_to_camera, error_message) ||
        !readString(root, "photodiode_device", message.photodiode_device, error_message) ||
        !readString(root, "sync_mode", message.sync_mode, error_message) ||
        !readInteger(root, "delay_ms", message.delay_ms, error_message) ||
        !readInteger(root, "guard_ms", message.guard_ms, error_message) ||
        !readInteger(root, "photodiode_baud", message.photodiode_baud, error_message) ||
        !readInteger(root, "sync_timeout_ms", message.sync_timeout_ms, error_message) ||
        !readInteger(root, "sync_guard_ms", message.sync_guard_ms, error_message) ||
        !readInteger(root, "max_patterns", message.max_patterns, error_message) ||
        !readInteger(root, "threshold", message.threshold, error_message) ||
        !readInteger(root, "projector_width", message.projector_width, error_message) ||
        !readInteger(root, "projector_height", message.projector_height, error_message) ||
        !readInteger(root, "pattern_count", message.pattern_count, error_message) ||
        !readInteger(root, "camera_id", message.camera_id, error_message) ||
        !readInteger(root, "left_camera_id", message.left_camera_id, error_message) ||
        !readInteger(root, "right_camera_id", message.right_camera_id, error_message) ||
        !readString(root, "window_role", message.window_role, error_message) ||
        !readString(root, "title", message.title, error_message) ||
        !readInteger(root, "width", message.width, error_message) ||
        !readInteger(root, "height", message.height, error_message) ||
        !readInteger(root, "monitor_index", message.monitor_index, error_message) ||
        !readInteger(root, "x", message.x, error_message) ||
        !readInteger(root, "y", message.y, error_message) ||
        !readString(root, "placement", message.placement, error_message) ||
        !readBool(root, "fullscreen", message.fullscreen, error_message) ||
        !readString(root, "post_open_key", message.post_open_key, error_message) ||
        !readString(root, "post_open_action", message.post_open_action, error_message) ||
        !readString(root, "projector_role", message.projector_role, error_message) ||
        !readInteger(root, "index", message.index, error_message) ||
        !readString(root, "decode_dir", message.decode_dir, error_message) ||
        !readInteger(root, "code_width", message.code_width, error_message) ||
        !readInteger(root, "code_height", message.code_height, error_message) ||
        !readInteger(root, "display_width", message.display_width, error_message) ||
        !readInteger(root, "display_height", message.display_height, error_message) ||
        !readString(root, "ply_file", message.ply_file, error_message) ||
        !readInteger(root, "decode_threshold", message.decode_threshold, error_message) ||
        !readString(root, "calibration_file", message.calibration_file, error_message) ||
        !readNumber(root, "max_epipolar_error_px", message.max_epipolar_error_px, error_message) ||
        !readNumber(root, "min_depth_mm", message.min_depth_mm, error_message) ||
        !readNumber(root, "max_depth_mm", message.max_depth_mm, error_message) ||
        !readString(root, "observations_dir", message.observations_dir, error_message) ||
        !readString(root, "camera_calibration_file", message.camera_calibration_file, error_message) ||
        !readInteger(root, "board_corners_x", message.board_corners_x, error_message) ||
        !readInteger(root, "board_corners_y", message.board_corners_y, error_message) ||
        !readNumber(root, "square_size_mm", message.square_size_mm, error_message) ||
        !readNumber(root, "max_mean_displacement_px", message.max_mean_displacement_px, error_message) ||
        !readNumber(root, "max_corner_displacement_px", message.max_corner_displacement_px, error_message) ||
        !readBool(root, "overwrite", message.overwrite, error_message)) {
      return ReadResult{
          ReadStatus::invalid,
          {},
          message.id,
          "invalid_json",
          std::move(error_message)};
    }

    ReadResult result;
    result.status = ReadStatus::message;
    result.response_id = message.id;
    result.message = std::move(message);
    return result;
  } catch (const cv::Exception&) {
    return ReadResult{
        ReadStatus::invalid, {}, std::nullopt, "invalid_json", "failed to parse JSON"};
  }
}

} // namespace control
