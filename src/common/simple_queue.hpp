#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace nxmount::common {

template <typename T>
class Queue {
public:
    Queue() = default;

    auto push(T&& msg) -> void {
        if (mShutdown) {
            return;
        }
        {
            const auto _ = std::scoped_lock(mMutex);
            mQueue.emplace_front(std::move(msg));
        }
        mCondVar.notify_one();
    }

    auto pop() -> std::optional<T> {
        if (mQueue.empty()) {
            return std::nullopt;
        }
        auto msg = std::move(mQueue.back());
        mQueue.pop_back();
        return std::make_optional<T>(std::move(msg));
    }

protected:
    std::mutex mMutex;
    std::condition_variable mCondVar;
    std::deque<T> mQueue;
    std::atomic<bool> mShutdown;
};

} // namespace nxmount::common