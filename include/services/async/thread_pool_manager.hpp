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

//TODO keep this but also implement a coroutine task system on top
//TODO pin the threads on cores using cpu affinity

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
            return priority < other.priority;
        }
    };

    template <typename F>
    struct Task
    {
        F task;
        TaskPriority priority;
        bool operator<(const Task &other) const
        {
            return priority < other.priority;
        }
    };

    class ThreadPoolManagerTest;

    class ThreadPoolManager
    {

    public:
        friend class Root;
        friend class ThreadPoolManagerTest;

        template <typename F>
        auto enqueueTask(Task<F> p_task) -> std::future<std::invoke_result_t<F>>;

        bool busy();

        size_t numThreadsWorking();

        bool isAcceptingNewJobs();

        void clearQueue();

    private:
        /*
            stops accepting new jobs through enqueueTask()
            if discardQueue is false waits for the tasks in the queue to finish
            if killSelf is true deletes itself
        */
        void stopSync(bool discardQueue = false, bool killSelf = false);
        std::future<void> stopAsync(bool discardQueue = false, bool killSelf = false); // non blocking stop call similar to stopSync()

        // wait for the current tasks to finish and pause, preserving the current queue
        void pauseSync();
        std::future<void> pauseAsync(); // non blocking pause call similar to pauseSync()

        void resume(); // resume the queue

        ThreadPoolManager(size_t num_threads = std::thread::hardware_concurrency());
        ~ThreadPoolManager();

        std::mutex availabilityMapMutex_;
        robin_hood::unordered_flat_map<std::thread::id, bool> threadAvailabilityMap_; // unordered hashmap that maps thread ids to true(thread isn't working) / false(thread is working)

        std::mutex queueMutex_;
        std::priority_queue<TaskWrapper> taskQueue_; //TODO might want to switch to a more cache friendly option

        std::condition_variable cv_; //cv to signal worker threads to wake up

        std::vector<std::thread> threads_;
        
        std::atomic<bool> pause_ = false;
        std::atomic<bool> stop_ = false;

        std::atomic<bool> acceptNewJobs_ = true; // this field is used by custom reasons to halt job accepting

        const size_t numThreads_;
    };

    /*
        enqueues a task to be done by one of the worker threads
        throws if the thread pool is not accepting new jobs (see ThreadPoolManager::isAcceptingNewJobs())
    */
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