#pragma once
#include <mutex>
#include <queue>
#include <thread>
#include <functional>
#include <condition_variable>
#include <vector>
#include <future>

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
        template <typename F>
        auto enqueueTask(Task<F> p_task) -> std::future<std::invoke_result_t<F>>;

        bool busy();

        friend class Root;

    private:
        void stopSync();               // wait for tasks to finish and call the deconstructor
        std::future<void> stopAsync(); // non blocking stop call similar to stopSync()

        void pauseSync();               // wait for tasks to finish and pause
        std::future<void> pauseAsync(); // non blocking pause call similar to pauseSync()

        void resume(); // resume the queue

        ThreadPoolManager(size_t num_threads = std::thread::hardware_concurrency());
        ~ThreadPoolManager();

        std::mutex queueMutex_;
        std::priority_queue<TaskWrapper> taskQueue_; // dynamic allocation by priority_queue is amortized

        std::condition_variable cv_;

        std::vector<std::thread> threads_;

        std::atomic<bool> pause_ = false;
        std::atomic<bool> stop_ = false;
    };

    template <typename F>
    auto ThreadPoolManager::enqueueTask(Task<F> p_task) -> std::future<std::invoke_result_t<F>>
    {
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