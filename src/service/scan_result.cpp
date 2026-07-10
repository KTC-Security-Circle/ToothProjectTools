#include "service/scan_result.hpp"

#include <utility>

namespace service::scan
{

std::string toString(ScanState state)
{
    switch (state)
    {
    case ScanState::idle:
        return "idle";
    case ScanState::running:
        return "running";
    case ScanState::stopping:
        return "stopping";
    case ScanState::completed:
        return "completed";
    case ScanState::failed:
        return "failed";
    case ScanState::stopped:
        return "stopped";
    }
    return "idle";
}

ScanResult ScanResult::success(std::string scan_id, ScanState status)
{
    ScanResult result;
    result.ok = true;
    result.scan_id = std::move(scan_id);
    result.status = status;
    return result;
}

ScanResult ScanResult::failure(std::string scan_id, std::string code, std::string message)
{
    ScanResult result;
    result.scan_id = std::move(scan_id);
    result.error = ScanError{std::move(code), std::move(message)};
    return result;
}

} // namespace service::scan
