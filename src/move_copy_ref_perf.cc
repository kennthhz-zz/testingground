#include <string>
#include <chrono>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <thread>
#include <time.h>
#include <tuple>
 
class MyData
{
public:
  MyData() { s = "hello";}
  MyData(MyData& src) : s(src.s)
  {
    //std::cout<<"Copy C'tor\n";
  }

  MyData(MyData&& src) : s(std::move(src.s))
  {
    //std::cout<<"Move C'tor\n";
  }

  std::string s;
};

void Foo(std::shared_ptr<MyData> data)
{
}

void Foo1(const std::shared_ptr<MyData>& data)
{
}

static std::chrono::nanoseconds DiffCpu(timespec start, timespec end) noexcept {
    timespec temp {.tv_sec = 0, .tv_nsec = 0};
    if ((end.tv_nsec - start.tv_nsec) < 0) {
      temp.tv_sec = end.tv_sec - start.tv_sec - 1;
      temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
      temp.tv_sec = end.tv_sec - start.tv_sec;
      temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    std::chrono::nanoseconds duration = std::chrono::seconds{temp.tv_sec} + std::chrono::nanoseconds{temp.tv_nsec};
    return duration;
}

int main() {
  const int iterations = 1000000;

  std::thread t1([] {
  auto data = std::make_shared<MyData>();
  data->s = "hello";
  timespec start { .tv_sec = 0, .tv_nsec = 0};
  timespec stop { .tv_sec = 0, .tv_nsec = 0};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
  for (int i = 0; i < iterations; i++) 
    Foo(data);
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &stop);
  auto diff = DiffCpu(start, stop);
  std::cout<<"Copy Diff:"<<diff.count()<<"\n";});

  std::thread t2([] {
  auto data = std::make_shared<MyData>();
  data->s = "hello";
  timespec start1 { .tv_sec = 0, .tv_nsec = 0};
  timespec stop1 { .tv_sec = 0, .tv_nsec = 0};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start1);
  for (int i = 0; i < iterations; i++) 
    Foo(std::move(data));

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &stop1);
  auto diff1 = DiffCpu(start1, stop1);
  std::cout<<"Move Diff:"<<diff1.count()<<"\n";});

  std::thread t3([] {
  auto data = std::make_shared<MyData>();
  data->s = "hello";
  timespec start1 { .tv_sec = 0, .tv_nsec = 0};
  timespec stop1 { .tv_sec = 0, .tv_nsec = 0};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start1);
  for (int i = 0; i < iterations; i++) 
    Foo1(data);

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &stop1);
  auto diff1 = DiffCpu(start1, stop1);
  std::cout<<"PassByRef Diff:"<<diff1.count()<<"\n";});

  t1.join();
  t2.join();
  t3.join();
}