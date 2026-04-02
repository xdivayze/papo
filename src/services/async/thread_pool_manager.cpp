#include "services/async/thread_pool_manager.hpp"
constexpr const long long int THREADPOOL_STOP_SLEEP_SEC = 1;

namespace async
{

    ThreadPoolManager::~ThreadPoolManager()
    {
        if (!stop_)
            stopSync(true, false);
    }

    ThreadPoolManager::ThreadPoolManager(size_t num_threads) : numThreads_(num_threads)
    {
        for (size_t i = 0; i < num_threads; i++)
        {
            threads_.emplace_back([this]
                                  {
            while (1) {
                {
                    std::unique_lock<std::mutex> lock(availabilityMapMutex_);
                    threadAvailabilityMap_.insert_or_assign(std::this_thread::get_id(), true);
                }
                TaskWrapper task; 
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);

                    cv_.wait(lock, [this] {
                        return stop_ || (!pause_ && !taskQueue_.empty());
                    });

                    if (stop_ && taskQueue_.empty()) {
                        return;
                    }

                    task = std::move(taskQueue_.top());
                    taskQueue_.pop();
                }

                {
                    std::unique_lock<std::mutex> lock(availabilityMapMutex_);
                    threadAvailabilityMap_.insert_or_assign(std::this_thread::get_id(), false);

                }

                task.fn();

            } });
        }
    }

    bool ThreadPoolManager::busy()
    {
        bool poolBusy;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            poolBusy = !taskQueue_.empty();
        }
        return poolBusy;
    }

    size_t ThreadPoolManager::numThreadsWorking()
    {
        std::lock_guard<std::mutex> lock(availabilityMapMutex_);
        return std::count_if(threadAvailabilityMap_.begin(), threadAvailabilityMap_.end(),
                             [](const auto &pair)
                             {
                                 return !pair.second;
                             });
    }

    void ThreadPoolManager::resume()
    {
        pause_ = false;
        cv_.notify_all();
    }

    std::future<void> ThreadPoolManager::pauseAsync()
    {
        return std::async(std::launch::async, &ThreadPoolManager::pauseSync, this);
    }

    void ThreadPoolManager::pauseSync()
    {
        pause_ = true;

        while (numThreadsWorking())
        {
            sleep(THREADPOOL_STOP_SLEEP_SEC);
        }

        return;
    }

    std::future<void> ThreadPoolManager::stopAsync(bool discardQueue, bool killSelf)
    {
        return std::async(std::launch::async, &ThreadPoolManager::stopSync, this, discardQueue, killSelf);
    }

    void ThreadPoolManager::stopSync(bool discardQueue, bool killSelf)
    {
        stop_ = true;
        acceptNewJobs_ = false;
        if (discardQueue)
        {
            {
                std::lock_guard lock(queueMutex_);
                while (!taskQueue_.empty())
                    taskQueue_.pop();
            }
        }

        cv_.notify_all();

        while (busy() || numThreadsWorking())
        {
            sleep(THREADPOOL_STOP_SLEEP_SEC);
        }

        for (auto &thread : threads_)
            thread.join();

        if (killSelf)
            delete this;

        return;
    }

    bool ThreadPoolManager::isAcceptingNewJobs()
    {
        return !stop_ && acceptNewJobs_;
    }
}