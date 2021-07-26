#include <iostream>
#include <memory>
#include <functional>
#include <time.h>
#include <chrono>
#include <stdio.h>
#include <string.h>

const int Level = 10;
const int Iter = 10000000;
const int Iter2 = 1000000;

void FooS(std::shared_ptr<int> sptr, int level)
{
  if (level == Level)
  {
    auto l = [sptr](){};
    return;
  }

  FooS(sptr, level + 1);
}

void MeasurePassSharedPtrByVaue()
{
  auto sptr = std::make_shared<int>(10);
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < Iter; i++)
  {
    auto level = 1;
    FooS(sptr, level);
  }
  auto stop = std::chrono::steady_clock::now();
  auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start); 
  std::cout<<"MeasurePassSharedPtrByVaue per pass in nsec:"<<(double)total.count()/Iter/(double)Level<<"\n";
}


void Foo(int* ptr, int level)
{
  if (level == Level)
  {
    //auto l = [ptr](){};
    return;
  }

  Foo(ptr, level + 1);
  
}

void MeasurePassRawPtr()
{
  auto ptr = new int(10);
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < Iter; i++)
  {
    auto level = 1;
    Foo(ptr, level);
  }
  auto stop = std::chrono::steady_clock::now();
  auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start); 
  std::cout<<"MeasurePassRawPtr per pass in nsec:"<<(double)total.count()/Iter/(double)Level<<"\n";
}

void FooU(std::unique_ptr<int> uptr, int level)
{
  if (level == Level)
  {
    //auto l = [p = std::move(uptr)](){};
    return;
  }

  FooU(std::forward<std::unique_ptr<int>>(uptr), level + 1);
}

void MeasurePassUniquePtr()
{
  auto uptr = std::make_unique<int>(10);
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < Iter; i++)
  {
    auto level = 1;
    FooU(std::move(uptr), level);
  }
  auto stop = std::chrono::steady_clock::now();
  auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start); 
  std::cout<<"MeasurePassUniquePtr per pass in nsec:"<<(double)total.count()/Iter/(double)Level<<"\n";
}

void FooSR(std::shared_ptr<int>& rptr, int level)
{
  if (level == Level)
  {
    //auto l = [rptr](){};
    return;
  }

  FooSR(rptr, level + 1);
}

void MeasurePassSharedPtrByRef()
{
  auto sptr = std::make_shared<int>(10);
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < Iter; i++)
  {
    auto level = 1;
    FooSR(sptr, level);
  }
  auto stop = std::chrono::steady_clock::now();
  auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start); 
  std::cout<<"MeasurePassSharedPtrByRef per pass in nsec:"<<(double)total.count()/Iter/(double)Level<<"\n";
}

void MeasureSharedPtrCreate()
{  
  auto total = 0;
  for (int i = 0; i < Iter2; i++)
  {
    auto start = std::chrono::steady_clock::now();
    auto p = std::make_shared<std::string>("Hello");
    auto stop = std::chrono::steady_clock::now();
    total += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count(); 
  }
  
  std::cout<<"MeasureSharedPtrCreate per pass in nsec:"<<total/Iter2<<"\n";
}

void MeasureRawPtrCreate()
{  
  auto total = 0;
  for (int i = 0; i < Iter2; i++)
  {
    auto start = std::chrono::steady_clock::now();
    auto p = new std::string("Hello");
    auto stop = std::chrono::steady_clock::now();
    total += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count(); 
    delete p;
  }
  
  std::cout<<"MeasureRawPtrCreate per pass in nsec:"<<total/Iter2<<"\n";
}

void MeasureUniquePtrCreate()
{  
  auto total = 0;
  for (int i = 0; i < Iter2; i++)
  {
    auto start = std::chrono::steady_clock::now();
    auto p = std::make_unique<std::string>("Hello");
    auto stop = std::chrono::steady_clock::now();
    total += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count(); 
  }
  
  std::cout<<"MeasureUniquePtrCreate per pass in nsec:"<<total/Iter2<<"\n";
}

std::string MeasureStackCreateString()
{  
  auto total = 0;
  std::string ret;
  for (int i = 0; i < Iter2; i++)
  {
    auto start = std::chrono::steady_clock::now();
    std::string s("Hello");
    auto stop = std::chrono::steady_clock::now();
    total += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count(); 
    ret = s;
  }
  
  std::cout<<"MeasureStackCreateString per pass in nsec:"<<(double)total/Iter2<<"\n";
  return ret;
}

void MeasureStackCreateChars()
{  
  auto total = 0;
  char* ret;
  for (int i = 0; i < Iter2; i++)
  {
    auto start = std::chrono::steady_clock::now();
    auto stop = std::chrono::steady_clock::now();
    total += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count(); 
  }
  
  std::cout<<"MeasureStackCreateChars per pass in nsec:"<<(double)total/Iter2<<"\n";
}

int main()
{
  MeasurePassRawPtr();
  MeasurePassSharedPtrByRef();
  MeasurePassSharedPtrByVaue();
  MeasurePassUniquePtr();
  MeasureSharedPtrCreate();
  MeasureRawPtrCreate();
  MeasureUniquePtrCreate();
  auto s = MeasureStackCreateString();
  std::cout<<s;
  MeasureStackCreateChars();
}
