#include "services/async/thread_pool_manager.hpp"

using namespace std;

namespace async
{
    bool ThreadPoolManager::busy()
    {
        bool poolBusy;
        {
            unique_lock<mutex> lock(queueMutex_);
            poolBusy = !taskQueue_.empty();
        }
        return poolBusy;
    }

    void ThreadPoolManager::stop() {
        //wait for all tasks to finish before returning 
    }
}