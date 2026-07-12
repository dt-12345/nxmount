#ifdef ENABLE_LOGGING

#include "common/simple_queue.hpp"
#include "log/logging.hpp"

#include <fmt/chrono.h>
#include <fmt/std.h>

#include <chrono>
#include <stacktrace>
#include <thread>
#include <utility>

#if defined(WIN32)
    #include <windows.h>
#else
    #include <signal.h>
#endif

namespace nxmount::logging {

static std::atomic<bool> sPrintedStacktrace = false;

static auto PrintStacktrace() -> void {
    if (!sPrintedStacktrace) {
        fmt::println(stderr, "Stacktrace:\n{}", std::to_string(std::stacktrace::current(1)));
        sPrintedStacktrace = true;
    }
}

[[noreturn]] static auto ShutdownAndAbort() -> void {
    PrintStacktrace();
    fmt::println(stderr, "Unrecoverable error occurred - aborting!");
    std::fflush(stdout);
    std::fflush(stderr);
    std::exit(1);
}

[[noreturn]] static auto TerminateHandler() -> void {
    PrintStacktrace();
    fmt::println(stderr, "Stacktrace:\n{}", std::to_string(std::stacktrace::current(1)));
    ShutdownAndAbort();
}

[[noreturn]] static auto ExceptionHandler(int signalNumber) -> void {
    fmt::println(stderr, "Exception occurred! {:#x}", static_cast<std::uint32_t>(signalNumber));
    fmt::println(stderr, "Uncaught exceptions: {}", std::uncaught_exceptions());
    ShutdownAndAbort();
}

std::atomic<Logger::Level> Logger::sLogLevel = Logger::Info;

struct Message {
    FILE* sink;
    std::string msg;
};

class LoggerImpl : public common::Queue<Message> {
public:
    LoggerImpl() : mThread(&LoggerImpl::run, this) {
        std::set_terminate(TerminateHandler);

#if defined(WIN32)
        SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* exceptionInfo) -> LONG {
            ExceptionHandler(exceptionInfo->ExceptionRecord->ExceptionCode);
            return EXCEPTION_CONTINUE_SEARCH;
        });
#else
        #define REGISTER_HANDLER(SIGNAL_NUMBER)                             \
            {                                                               \
                struct sigaction action = {};                               \
                action.sa_handler = ExceptionHandler;                       \
                sigemptyset(std::addressof(action.sa_mask));                \
                action.sa_flags = 0;                                        \
                sigaction(SIGNAL_NUMBER, std::addressof(action), nullptr);  \
            }
        REGISTER_HANDLER(SIGILL);
        REGISTER_HANDLER(SIGABRT);
        REGISTER_HANDLER(SIGFPE);
        REGISTER_HANDLER(SIGSEGV);
        REGISTER_HANDLER(SIGTERM);
        #undef REGISTER_HANDLER
#endif
    }

    ~LoggerImpl() {
        shutdown();
        mThread.join();
        flush();
    }

    auto flush() -> void {
        const auto _ = std::scoped_lock(mMutex); // TODO: will this deadlock if the thread that throws never released the lock?
        while (!mQueue.empty()) {
            const auto msg = std::move(mQueue.back());
            mQueue.pop_back();
            fmt::println(msg.sink, "{}", msg.msg);
        }
        std::fflush(stdout);
        std::fflush(stderr);
    }

    auto shutdown() -> void {
        mShutdown = true;
        mCondVar.notify_one();
    }

    auto run() -> void {
        while (!mShutdown) {
            auto lock = std::unique_lock(mMutex);
            mCondVar.wait(lock, [this]() -> bool { return !this->mQueue.empty() || mShutdown; });
            if (mShutdown) {
                break;
            }
            const auto msg = pop();
            if (msg) {
                fmt::println(msg->sink, "{}", msg->msg);
            } else {
                break;
            }
        }
    }

private:
    std::thread mThread;
};

static auto sLogger = LoggerImpl();

static constexpr auto GetLevelString(Logger::Level lvl) -> std::string_view {
    switch (lvl) {
        case Logger::Level::Info: return "Info";
        case Logger::Level::Warning: return "Warning";
        case Logger::Level::Error: return "Error";
        case Logger::Level::Fatal: return "Fatal";
        default: std::unreachable();
    }
}

auto Logger::LogImpl(std::string_view msg, Level lvl) -> void {
    const auto now = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
    Message message {
        .sink = lvl >= Warning ? stderr : stdout,
        .msg = "",
    };

    fmt::format_to(std::back_inserter(message.msg),  "[{} {:%H:%M:%S} {}]: {}", GetLevelString(lvl), now, std::this_thread::get_id(), msg);

    if (lvl >= Fatal) {
        message.msg += "\n";
        message.msg += std::to_string(std::stacktrace::current(1));
    }

    sLogger.push(std::move(message));
}

} // namespace nxmount::logging

#endif