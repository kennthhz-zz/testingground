#include <queue>
#include <functional>
#include <memory>
#include <mutex>

class FunctionQueue
{
public:
  FunctionQueue() = default;
  FunctionQueue(const FunctionQueue &) = delete;
  FunctionQueue &operator=(const FunctionQueue &) = delete;

  void Enqueue(std::shared_ptr<std::function<void()>> item);

  std::shared_ptr<std::function<void()>> Dequeue();

  int Size() { return queue_.size(); }

private:
  std::queue<std::shared_ptr<std::function<void()>>> queue_;
  std::mutex mtx_;
};
