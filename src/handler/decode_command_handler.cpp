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
                return command_result_mapper::decode::toCommandResult(
                    ctx.decode_service.decodePatterns(service::decode::DecodePatternsConfig{
                        std::filesystem::path{decode_command.input_dir},
                        std::filesystem::path{decode_command.output_dir},
                        decode_command.threshold,
                        decode_command.allow_partial,
                    }));
            }
            return common::notHandled();
        },
        command);
}

} // namespace handler::decode
