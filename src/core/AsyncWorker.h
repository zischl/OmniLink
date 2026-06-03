#ifndef ASYNCWORKER_H
#define ASYNCWORKER_H

#pragma once
#include <array>
#include <atomic>
#include <functional>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace AsyncWorker {

template <typename ResultPoolType, size_t ResultPoolSize> class Cached
{

    static_assert(ResultPoolSize > 0 && (ResultPoolSize & (ResultPoolSize - 1)) == 0,
                  "ResultPoolSize must be a power of 2");

  public:
    std::optional<std::thread> Thread;
    std::array<ResultPoolType, ResultPoolSize> ResultPool = {};
    std::atomic_bool status;
    uint32_t head = 1;
    uint32_t tail = 0;

    template <typename func, typename... Args>
    inline void StartSpinThread(func&& Function, Args&&... Arguments)
    {

        status.store(true);

        Thread.emplace([&status = this->status,
                        &ResultPool = this->ResultPool,
                        &head = this->head,
                        &tail = this->tail,
                        function = std::forward<func>(Function),
                        ... args = std::forward<Args>(Arguments)]() mutable {
            while (status.load()) {
                ResultPool[head] = std::invoke(function, args...);
                Push(head, tail);
            }
        });
    }

    template <typename func, typename... Args>
    inline void StartWaitThread(size_t timeout, func&& Function, Args&&... Arguments)
    {

        status.store(true);

        Thread.emplace([timeout,
                        &status = this->status,
                        &ResultPool = this->ResultPool,
                        &head = this->head,
                        &tail = this->tail,
                        function = std::forward<func>(Function),
                        ... args = std::forward<Args>(Arguments)]() mutable {
            while (status.load()) {
                ResultPool[head] = std::invoke(function, args...);
                Push(head, tail);

                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            }
        });
    }

    void EndLoopedThread()
    {
        status.store(false);
        if (Thread && Thread->joinable()) {
            Thread->join();
        }
        Thread.reset();
    }

    inline static void Push(uint32_t& head, uint32_t& tail)
    {
        if (head != tail) {
            head = (head + 1) & (ResultPoolSize - 1);
        }
    }

    void Pull()
    {
        if (head != tail) {
            tail = (tail + 1) & (ResultPoolSize - 1);
        }
    }
};

class Uncached
{

  public:
    std::optional<std::thread> Thread;
    std::atomic_bool status;

    template <typename func, typename... Args>
    inline void StartSpinThread(func&& Function, Args&&... Arguments)
    {

        status.store(true);

        Thread.emplace([&status = this->status,
                        function = std::forward<func>(Function),
                        ... args = std::forward<Args>(Arguments)]() mutable {
            while (status.load()) {
                std::invoke(function, args...);
            }
        });
    }

    template <typename func, typename... Args>
    inline void StartWaitThread(size_t timeout, func&& Function, Args&&... Arguments)
    {

        status.store(true);

        Thread.emplace([timeout,
                        &status = this->status,
                        function = std::forward<func>(Function),
                        ... args = std::forward<Args>(Arguments)]() mutable {
            while (status.load()) {
                std::invoke(function, args...);

                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            }
        });
    }

    void EndLoopedThread()
    {
        status.store(false);
        if (Thread && Thread->joinable()) {
            Thread->join();
        }
        Thread.reset();
    }
};

} // namespace AsyncWorker

namespace AsyncDispatch {
class DispatchBase
{
  public:
    std::optional<std::thread> DispatchThread;
    std::atomic_bool DispatcherStatus;
    uint8_t ActiveWorkers = 0;
};

template <typename ResultPoolType, size_t ResultPoolSize> class Dynamic : public DispatchBase
{
  public:
    std::vector<AsyncWorker::Cached<ResultPoolType, ResultPoolSize>> Workers;

    Dynamic() { Workers.reserve(2); }

    template <typename function, typename... args>
    void StartThread(function&& Function, args&&... Args)
    {
        Workers.emplace_back();
    }
};

template <typename Worker, typename ResultPoolType, size_t ResultPoolSize, size_t MaxWorkerCount>
class Fixed : public DispatchBase
{
  public:
    std::array<AsyncWorker::Cached<ResultPoolType, ResultPoolSize>, MaxWorkerCount> Workers;

    template <typename function, typename... args> void StartThreadLoop() {}

    template <typename function, typename... args>
    void SetDispatchFunc(function&& Function, args&&... Arguments)
    {
        this->DispatchThread.emplace([this,
                                      Func = std::forward<function>(Function),
                                      ... Args = std::forward<args>(Arguments)]() mutable {
            while (DispatcherStatus.load()) {
            }
        });
    }
};

}; // namespace AsyncDispatch

#endif // ASYNCWORKER_H
