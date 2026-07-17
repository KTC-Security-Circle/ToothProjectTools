#include "handler/decode_command_handler.hpp"

#include "command_result_mapper/decode_command_result_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/decode_service.hpp"

#include <filesystem>
#include <type_traits>
#include <variant>

namespace handler::decode
{

common::CommandResult handle(runtime::DecodeHandlerContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](const auto& decode_command) -> common::CommandResult
        {
            using CommandType = std::decay_t<decltype(decode_command)>;
            if constexpr (std::is_same_v<CommandType, cmd::CmdDecodePatterns>)
            {
                service::decode::DecodePatternsConfig config;
                if (!decode_command.input_dir.empty())
                {
                    config.input_dir = std::filesystem::path{decode_command.input_dir};
                }
                if (!decode_command.left_dir.empty())
                {
                    config.left_dir = std::filesystem::path{decode_command.left_dir};
                }
                if (!decode_command.right_dir.empty())
                {
                    config.right_dir = std::filesystem::path{decode_command.right_dir};
                }
                if (!decode_command.metadata_file.empty())
                {
                    config.metadata_file = std::filesystem::path{decode_command.metadata_file};
                }
                config.output_dir = std::filesystem::path{decode_command.output_dir};
                config.threshold = decode_command.threshold;
                config.allow_partial = decode_command.allow_partial;
                config.projector_width = decode_command.projector_width;
                config.projector_height = decode_command.projector_height;
                config.pattern_count = decode_command.pattern_count;
                return command_result_mapper::decode::toCommandResult(ctx.decode_service.decodePatterns(config));
            }
            return common::notHandled();
        },
        command);
}

} // namespace handler::decode
