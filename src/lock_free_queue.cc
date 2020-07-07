#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <queue>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>
#include <unistd.h>

using namespace std;

struct Item
{
  int value;
  int b;
};

static const Item zero{0};

template <class T>
class SinglePCQueue
{
  public:
    SinglePCQueue() : size_{0}, head_{0}, tail_{0}
    {
      memset(queue_, 0, sizeof(queue_));
    }

    bool TryEnqueue(const T& value)
    {
      if (size_ >= limit_)
        return false;
        
      atomic_exchange_explicit(&queue_[tail_], value, memory_order_release);

      if (tail_ == limit_ - 1)
        tail_ = 0;
      else
        tail_++;
      
      size_++;
      return true;
    }

    bool TryDequeue(T& item)
    {
      if (size_ > 0)
      {
        auto item = atomic_exchange_explicit(&queue_[head_], zero, memory_order_release);
        if (head_== limit_-1)
          head_ = 0;
        else
          head_++;
        
        size_--;
        return true;
      }
      else
      {
        return false;
      }
    }

  private:
    static const int limit_ = 500000;
    atomic<T> queue_[limit_];
    atomic<int> size_;
    atomic<int> head_;
    atomic<int> tail_;
};

SinglePCQueue<Item> q;
vector<Item> popped;
vector<Item> added;
vector<Item> erradd;
int a{0}, p{0}, e{0};

int main()
{
  popped.resize(1000000);
  added.resize(1000000);
  erradd.resize(1000000);

  std::thread t1([]() {
    do
    {
      Item t;
      if(q.TryDequeue(t))
      {
        //popped.emplace_back(t);
        p++;
      }
    } while (true);
  });

  std::thread t2([]() {
    auto start = chrono::steady_clock::now();
    for (int i = 1; i < 1000000; i++)
    {
      Item t;
      if (!q.TryEnqueue(t))
      {
        //erradd.emplace_back(t);
        e++;
      }
      else
      {
        //added.emplace_back(t);
        a++;
      }
    }
    auto end = chrono::steady_clock::now();
    cout << "Elapsed time in milliseconds : "
         << chrono::duration_cast<chrono::milliseconds>(end - start).count()
         << " ms" << endl;
  });

  t2.detach();
  t1.detach();

  std::this_thread::sleep_for(std::chrono::seconds(1));
  cout << "added size " << a<< " popped size " << p << " erradd size "<< e<< " total "<< e+a<<endl;
}