
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

int main(int argc, char *argv[])
{
  ThreadPool pool(32);
  pool.Reset(32);
  for (int i = 0; i < 10000000; i++)
  {
    pool.Enqueue(Func, i);
  }

  //pool.Reset(1);

  std::this_thread::sleep_for(std::chrono::seconds(30));

  for (int i = 0; i < 100000; i++)
  {
    pool.Enqueue(Func, i);
  }

  std::this_thread::sleep_for(std::chrono::seconds(300));
  pool.Shutdown();
  std::this_thread::sleep_for(std::chrono::seconds(1));
}