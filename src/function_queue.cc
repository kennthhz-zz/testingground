#include "function_queue.h"
#include <functional>

void FunctionQueue::Enqueue(std::shared_ptr<std::function<void()>> item)
{
  mtx_.lock();
  queue_.push(item);
  mtx_.unlock();
}

  std::shared_ptr<std::function<void()>> FunctionQueue::Dequeue()
  {
    std::shared_ptr<std::function<void()>> ret;
    mtx_.lock();
    if (queue_.size() == 0)
    {
      ret = nullptr;
    }
    else
    {
      ret = queue_.front();
      queue_.pop();
    }
    
    mtx_.unlock();
    return ret;
  }

