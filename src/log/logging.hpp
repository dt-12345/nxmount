#pragma once

#include <fmt/base.h>

#include <cstdint>

namespace fmt {

template <std::size_t N>
struct formatter<std::uint8_t[N]> {
    static_assert(N > 0);
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const std::uint8_t (&value)[N], format_context& ctx) const -> format_context::iterator {
        for (std::size_t i = 0; i < N - 1; ++i) {
            fmt::format_to(ctx.out(), "{:02x}", value[i]);
        }
        return fmt::format_to(ctx.out(), "{:02x}", value[N - 1]);
    }
};

} // namespace fmt

#ifdef ENABLE_LOGGING

#include <atomic>
#include <iterator>
#include <string>
#include <string_view>

namespace nxmount::logging {

class Logger {
public:
    enum Level {
        Info,
        Warning,
        Error,
        Fatal,
    };

    template <typename... Ts>
    static auto LogInfo(const fmt::format_string<Ts...>& fmt, Ts&&... args) -> void {
        if (sLogLevel > Info) return;
        std::string out{};
        fmt::format_to(std::back_inserter(out), fmt, std::forward<Ts>(args)...);
        LogImpl(out, Info);
    }

    template <typename... Ts>
    static auto LogWarning(const fmt::format_string<Ts...>& fmt, Ts&&... args) -> void {
        if (sLogLevel > Warning) return;
        std::string out{};
        fmt::format_to(std::back_inserter(out), fmt, std::forward<Ts>(args)...);
        LogImpl(out, Warning);
    }

    template <typename... Ts>
    static auto LogError(const fmt::format_string<Ts...>& fmt, Ts&&... args) -> void {
        if (sLogLevel > Error) return;
        std::string out{};
        fmt::format_to(std::back_inserter(out), fmt, std::forward<Ts>(args)...);
        LogImpl(out, Error);
    }

    template <typename... Ts>
    [[noreturn]] static auto LogFatal(const fmt::format_string<Ts...>& fmt, Ts&&... args) -> void {
        std::string out{};
        fmt::format_to(std::back_inserter(out), fmt, std::forward<Ts>(args)...);
        LogImpl(out, Fatal);
        std::abort();
    }

    static auto SetLogLevel(Level lvl) -> void { sLogLevel = lvl; }

private:
    static auto LogImpl(std::string_view msg, Level lvl) -> void;

    static std::atomic<Level> sLogLevel;
};

} // namespace nxmount::logging

#define LOG_INFO(...) do { ::nxmount::logging::Logger::LogInfo(__VA_ARGS__); } while (0)
#define LOG_WARNING(...) do { ::nxmount::logging::Logger::LogWarning(__VA_ARGS__); } while (0)
#define LOG_ERROR(...) do { ::nxmount::logging::Logger::LogError(__VA_ARGS__); } while (0)
#define LOG_FATAL(...) do { ::nxmount::logging::Logger::LogFatal(__VA_ARGS__); } while (0)
#define SET_LOG_LEVEL(lvl) do { ::nxmount::logging::Logger::SetLogLevel(lvl); } while (0)

#else

#include "common/utils.hpp"

#define LOG_INFO(...) do { ::nxmount::common::Unused(__VA_ARGS__); } while (0)
#define LOG_WARNING(...) do { ::nxmount::common::Unused(__VA_ARGS__); } while (0)
#define LOG_ERROR(...) do { ::nxmount::common::Unused(__VA_ARGS__); } while (0)
#define LOG_FATAL(...) do { ::nxmount::common::Unused(__VA_ARGS__); std::abort(); } while (0)
#define SET_LOG_LEVEL(lvl) do { ::nxmount::common::Unused(lvl); } while (0)

#endif