#include "service/decode_result.hpp"

#include <utility>

namespace service::decode
{

DecodePatternsResult DecodePatternsResult::success()
{
    DecodePatternsResult result;
    result.ok = true;
    return result;
}

DecodePatternsResult DecodePatternsResult::failure(std::string code, std::string message)
{
    DecodePatternsResult result;
    result.ok = false;
    result.error = DecodeError{std::move(code), std::move(message)};
    return result;
}

} // namespace service::decode
