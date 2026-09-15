#pragma once

#include "calibration/calibration_service.hpp"
#include "calibration/stereo_calibration_service.hpp"
#include "capture/capture_result.hpp"
#include "cmd/commands.hpp"
#include "common/command_result.hpp"
#include "decode/decode_result.hpp"
#include "projector/projector_result.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include "scan/scan_dataset_result.hpp"
#include "scan/scan_result.hpp"
#include "video/camera_result.hpp"
#include "window/window_result.hpp"

#include <map>
#include <string>

namespace headless::result_adapter
{
common::CommandResult camera(const video::CameraResult& result, bool include_camera_id = true);
common::CommandResult capture(const ::capture::CaptureResult& result,
                              std::map<std::string, std::string> values);
common::CommandResult capture(const ::capture::CaptureStereoResult& result,
                              std::map<std::string, std::string> values);
common::CommandResult window(const win::WindowResult& result, bool include_size);
common::CommandResult projector(const ::projector::ProjectorResult& result, bool include_window_role,
                                bool include_size, bool include_pattern_count, bool include_pattern_index);
common::CommandResult monitorList(const ::projector::ProjectorResult& result);
common::CommandResult scan(const ::scan::ScanResult& result);
common::CommandResult scanDataset(const ::scan::dataset::ScanDatasetValidationResult& result);
common::CommandResult decode(const ::decode::DecodePatternsResult& result);
common::CommandResult calibration(const std::string& role, const cmd::CmdCalibrate& command,
                                  const calib::MonoCalibrationResult& result);
common::CommandResult stereoCalibration(const std::string& left_role, const std::string& right_role,
                                        const cmd::CmdStereoCalibrate& command,
                                        const calib::StereoCalibrationResult& result);
common::CommandResult reconstruction(const ::reconstruction::ReconstructionValidationResult& result);
common::CommandResult reconstruction(const ::reconstruction::ReconstructionResult& result);
} // namespace headless::result_adapter
