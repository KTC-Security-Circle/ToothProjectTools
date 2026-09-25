#include "window/niri_window_action_executor.hpp"
#include <cerrno>
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
namespace win {
std::optional<std::vector<std::string>> niriActionArguments(const std::string& action) {
  if (action == "move-to-monitor-left") return std::vector<std::string>{"niri","msg","action","move-window-to-monitor-left"};
  if (action == "move-to-monitor-right") return std::vector<std::string>{"niri","msg","action","move-window-to-monitor-right"};
  if (action == "move-to-monitor-up") return std::vector<std::string>{"niri","msg","action","move-window-to-monitor-up"};
  if (action == "move-to-monitor-down") return std::vector<std::string>{"niri","msg","action","move-window-to-monitor-down"};
  return std::nullopt;
}
WindowActionResult NiriWindowActionExecutor::execute(win::WindowId,
    const std::optional<std::string>& key, const std::optional<std::string>& action) {
  if (key) return {false,"window_post_open_action_unsupported",
                   "post_open_key requires a key-injection backend; use post_open_action on niri"};
  if (!action) return {true,{},{}};
  const auto arguments=niriActionArguments(*action);
  if (!arguments) return {false,"window_post_open_action_unsupported","unsupported niri window action: "+*action};
  if (std::getenv("NIRI_SOCKET")==nullptr)
    return {false,"window_post_open_action_unsupported","niri IPC is not available"};
  std::vector<char*> argv; argv.reserve(arguments->size()+1);
  for (const auto& argument:*arguments) argv.push_back(const_cast<char*>(argument.c_str()));
  argv.push_back(nullptr);
  pid_t pid{}; const int spawn_error=posix_spawnp(&pid,argv.front(),nullptr,nullptr,argv.data(),environ);
  if (spawn_error!=0) return {false,spawn_error==ENOENT?"window_post_open_action_unsupported":"window_post_open_action_failed",
                              "failed to start niri action"};
  int status{};
  if (waitpid(pid,&status,0)<0 || !WIFEXITED(status) || WEXITSTATUS(status)!=0)
    return {false,"window_post_open_action_failed","niri action returned a failure"};
  return {true,{},{}};
}
}
