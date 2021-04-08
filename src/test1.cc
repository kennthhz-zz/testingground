#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <exception>
#include <algorithm>
#include <vector>
#include <functional>
#include <time.h>
#include <chrono>
#include <thread>


class MyObj final 
{
public:

  MyObj()
  {
    std::cout<<"Default c'tor "<<(void*)this<<"\n";
  }

  MyObj(const MyObj& src)
  {
    std::cout<<"Copy c'tor "<<(void*)this<<"\n";
  }

  MyObj(MyObj&& src)
  {
    std::cout<<"Move c'tor "<<(void*)this<<"\n";
  }

  ~MyObj() 
  {
    std::cout<<"destructor "<<(void*)this<<"\n";
  }
};

void Foo1(MyObj* po)
{
  auto l =[po](){ auto u = std::unique_ptr<MyObj>(po); };
  std::thread t(l);
  t.join();
}

void Fo2(MyObj* po)
{
  auto u = std::unique_ptr<MyObj>(po);
}

int main() 
{
  auto p1 = std::make_unique<MyObj>();
  Foo1(p1.get());
  std::cout<<"i am here\n";
  p1.release();
}