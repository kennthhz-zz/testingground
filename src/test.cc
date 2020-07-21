
#include <iostream>
#include "thread_pool.h"
#include <chrono>
#include <thread>

using namespace std;

void Func(int a)
{
  for (int i = 0; i < 100000; i++)
  {}
}


/*
template <typename F, typename... Args>
void Enqueue1(F &&f, Args &&... args)
{
  std::shared_ptr<std::function<void()>> abc = make_shared<std::function<void()>>([=] { f(args...); });
  atomic<std::shared_ptr<std::function<void()>>*> ff{&abc};
  cout << ff.is_lock_free()<<"\n";
  (*(*ff))();
}*/

int main(int argc, char *argv[])
{
  ThreadPool pool;
  pool.Reset(2);
  for (int i = 0; i < 100000; i++)
  {
    pool.Enqueue(Func, i);
  }

  pool.Print();

  pool.Reset(4);

  std::this_thread::sleep_for(std::chrono::seconds(1));

  pool.Print();

  pool.Reset(2);

  std::this_thread::sleep_for(std::chrono::seconds(30));

  for (int i = 0; i < 100000; i++)
  {
    pool.Enqueue(Func, i);
  }

  std::this_thread::sleep_for(std::chrono::seconds(30));
  pool.Shutdown();
  std::this_thread::sleep_for(std::chrono::seconds(1));
}