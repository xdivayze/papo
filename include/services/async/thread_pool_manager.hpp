#pragma once
#include <mutex>
#include <queue>
#include <thread>
#include <functional>
#include <condition_variable>
#include <vector>
#include <future>
#include "../../collections/robin_hood.hpp"
#include "../../utils/exception.hpp"

namespace Engine
{
    class Root;
}

namespace async
{

    typedef enum
    {
        LOW,
        REGULAR,
        HIGH,
        ABORT_ALL_ELSE,

    } TaskPriority;

    struct TaskWrapper
    {
        std::function<void()> fn;
        TaskPriority priority;
        bool operator<(const TaskWrapper &other) const
        {
            return priority > other.priority;
        }
    };

    template <typename F>
    struct Task
    {
        F task;
        TaskPriority priority;
        bool operator<(const Task &other) const
        {
            return priority > other.priority;
        }
    };

    class ThreadPoolManager
    {

    public:
        friend class Root;

        template <typename F>
        auto enqueueTask(Task<F> p_task) -> std::future<std::invoke_result_t<F>>;

        bool busy();

        size_t numThreadsWorking();

        bool isAcceptingNewJobs();

    private:
        void stopSync(bool discardQueue = false, bool killSelf = false);               // wait for tasks to finish and calls deconstructor
        std::future<void> stopAsync(bool discardQueue = false, bool killSelf = false); // non blocking stop call similar to stopSync()

        void pauseSync();               // wait for tasks to finish and pause
        std::future<void> pauseAsync(); // non blocking pause call similar to pauseSync()

        void resume(); // resume the queue

        ThreadPoolManager(size_t num_threads = std::thread::hardware_concurrency());
        ~ThreadPoolManager();

        std::mutex availabilityMapMutex_;
        robin_hood::unordered_flat_map<std::thread::id, bool> threadAvailabilityMap_; // unordered hashmap that maps thread ids to true(thread isn't working) / false(thread is working)

        std::mutex queueMutex_;
        std::priority_queue<TaskWrapper> taskQueue_; // dynamic allocation by priority_queue is amortized

        std::condition_variable cv_;

        std::vector<std::thread> threads_; // vector rarely if ever gets larger so no runtime memory problems here

        std::atomic<bool> pause_ = false;
        std::atomic<bool> stop_ = false;

        std::atomic<bool> acceptNewJobs_ = true;

        const size_t numThreads_;
    };

    template <typename F>
    auto ThreadPoolManager::enqueueTask(Task<F> p_task) -> std::future<std::invoke_result_t<F>>
    {
        if (!isAcceptingNewJobs())
            throw Util::PapoException("Thread Pool Manager", "thread pool is not accepting new jobs");

        using ReturnType = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::move(p_task.task));
        std::future<ReturnType> fut = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            taskQueue_.push(TaskWrapper{
                [task]()
                { (*task)(); },
                p_task.priority});
        }
        cv_.notify_one();
        return fut;
    }
}