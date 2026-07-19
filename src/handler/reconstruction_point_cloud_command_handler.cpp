#include "handler/reconstruction_point_cloud_command_handler.hpp"
#include "command_result_mapper/reconstruction_command_result_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include <type_traits>
namespace handler::reconstruction_point_cloud { common::CommandResult handle(runtime::ReconstructionPointCloudHandlerContext& c,const cmd::Command& command){return std::visit([&](const auto& q)->common::CommandResult{using T=std::decay_t<decltype(q)>;if constexpr(std::is_same_v<T,cmd::CmdValidateReconstruction>){return command_result_mapper::reconstruction::toCommandResult(c.reconstruction_service.validate({q.decode_dir,q.calibration_file,{q.config.max_epipolar_error_px,q.config.min_depth_mm,q.config.max_depth_mm}}));}else if constexpr(std::is_same_v<T,cmd::CmdReconstructPointCloud>){return command_result_mapper::reconstruction::toCommandResult(c.reconstruction_service.reconstruct({{q.decode_dir,q.calibration_file,{q.config.max_epipolar_error_px,q.config.min_depth_mm,q.config.max_depth_mm}},q.output_file,q.overwrite}));}return common::notHandled();},command);} }
