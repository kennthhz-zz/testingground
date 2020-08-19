#include "function_queue.h"
#include <set>
#include <mutex>
#include <atomic>

using namespace std;
class ThreadPool
{
  public:
    ThreadPool(int cpu_affinity = -1);
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    template <typename F, typename... Args>
    void Enqueue(F &&f, Args&&... args)
    {
      queue_.Enqueue(make_shared<std::function<void()>>([=] { f(args...); }));
    }

    void Reset(int newSize);
    void Shutdown()
    {
      shuttingDown_ = true;
    }

    void Print();

  private:
    void SetThreadAffinity();
    void StartMonitoring();
    FunctionQueue queue_;
    int size_;
    atomic<bool> shuttingDown_;
    atomic<int> sleepMicrosSeconds_;
    atomic<bool> monitoring_;
    set<pid_t> tids_;
    mutex mtx_tids_;
    int cput_limit_;
};