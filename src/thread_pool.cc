#include "thread_pool.h"
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>
#include <algorithm>
#include <pthread.h>

using namespace std;

ThreadPool::ThreadPool(int cput_limit)
{
  shuttingDown_ = false;
  size_ = 0;
  cput_limit_ = cput_limit;
  sleepMicrosSeconds_ = 0;
  monitoring_ = false;
}

void ThreadPool::StartMonitoring()
{
  if (monitoring_)
    return;

  monitoring_ = true;
  std::thread t([this]() {
    int current = 0;
    int total = 0;
    int counter = 0;

    while (true)
    {
      total += queue_.Size();
      counter++;

      if (counter == 10)
      {
        current = total / counter;
        total = 0;
        counter = 0;

        if (current > 0)
        {
          sleepMicrosSeconds_ = 0;
        }
        else if (current == 0)
        {
          if (sleepMicrosSeconds_ == 0)
            sleepMicrosSeconds_ = 1;
          else if (sleepMicrosSeconds_ < 1000000)
            sleepMicrosSeconds_ = sleepMicrosSeconds_ * 2;
        }

        cout <<" current:" << current << " sleep:" << sleepMicrosSeconds_ << "\n";
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });

  t.detach();
}

void ThreadPool::Reset(int newSize)
{
  StartMonitoring();
  if (newSize > size_)
  {
    auto added = newSize - size_;
    for (int i = 0; i < added; i++)
    {
      std::thread t([this]() {
        auto tid = gettid();
        mtx_tids_.lock();
        if (tids_.find(tid) == tids_.end())
        {
          tids_.insert(tid);
          size_++;
        }
        else
          throw invalid_argument("not possible");

        mtx_tids_.unlock();

        cpu_set_t cpuset;
        auto thread = pthread_self();
        for (int j = 0; j < cput_limit_; j++)
          CPU_SET(j, &cpuset);
        auto s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (s != 0)
          cout << "pthread_setaffinity_np failed\n";

        /* Check the actual affinity mask assigned to the thread */
        s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (s != 0)
          cout << "pthread_getaffinity_np failed\n ";

        do
        {
          auto item = queue_.Dequeue();
          if (item != nullptr)
          {
            (*item)();
          }

          if (sleepMicrosSeconds_ > 0)
            std::this_thread::sleep_for(std::chrono::microseconds(sleepMicrosSeconds_));

          if (shuttingDown_)
          {
            mtx_tids_.lock();
            auto tid = gettid();
            tids_.erase(tid);
            size_--;
            mtx_tids_.unlock();
            break;
          }
            
        } while (true);
      });

      t.detach();
    }
  }
  else
  {
    shuttingDown_ = true;
    while (size_ > 0)
    {}

    shuttingDown_ = false;
    Reset(newSize);
  }
}

void ThreadPool::Print()
{
  mtx_tids_.lock();
  cout << "Thread count:" << size_ << " "<< tids_.size()<<"\n";
  std::for_each(tids_.cbegin(), tids_.cend(), [](pid_t x) {
    std::cout << x << ' ';
  });
  cout << "\n";
  cout << "Queue size:" << queue_.Size()<<"\n";
  mtx_tids_.unlock();
}

void ThreadPool::SetThreadAffinity()
{

}
