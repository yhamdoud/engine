#pragma once

#include <array>
#include <chrono>
#include <string>
#include <string_view>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>

namespace engine
{

enum class LogType
{
    info,
    warning,
    error,
    debug,
};

constexpr std::array<std::string_view, 4> entry_type_strings{
    "Info",
    "Warning",
    "Error",
    "Debug",
};

std::ostream &operator<<(std::ostream &os, LogType type);

class Logger
{
    std::string pattern = "[{:%H:%M:%S}] [{type}] {msg}\n";

  public:
    template <typename... Args>
    void log(LogType type, std::string_view format, Args &&...args)
    {
        using namespace fmt::literals;

        auto now = std::chrono::system_clock::now();

        const auto color = [&type]
        {
            switch (type)
            {
            case LogType::info:
                return fmt::color::gray;
            case LogType::warning:
                return fmt::color::yellow;
            case LogType::error:
                return fmt::color::red;
            case LogType::debug:
                return fmt::color::blue;
            }
        }();

        // FIXME: breaks compile time checking.
        // https://stackoverflow.com/questions/68675303/how-to-create-a-function-that-forwards-its-arguments-to-fmtformat-keeping-the
        const auto &msg = fmt::vformat(
            format, fmt::make_format_args(std::forward<Args>(args)...));

        fmt::print(fmt::fg(color), pattern, fmt::localtime(now),
                   "type"_a = entry_type_strings[static_cast<int>(type)],
                   "msg"_a = msg);
    }

    void info(auto &&...args) { log(LogType::warning, args...); }
    void warn(auto &&...args) { log(LogType::info, args...); }
    void error(auto &&...args) { log(LogType::error, args...); }
    void debug(auto &&...args) { log(LogType::debug, args...); }
};

extern Logger logger;

} // namespace engine