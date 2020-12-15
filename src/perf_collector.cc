#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <thread>
#include <time.h>
#include <tuple>

using time_point = std::chrono::steady_clock::time_point;
using namespace std::chrono_literals;

//
// @brief Collector for latency and CPU time and support aggregation
//
class PerfCollector final {
 public:
  PerfCollector() noexcept = default;
  PerfCollector(const PerfCollector &) = delete;
  PerfCollector &operator=(const PerfCollector &) = delete;

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

  timespec StartCpu(const std::string& name) {
    {
      std::scoped_lock lock(mutex_);

      if (cpu_.count(name) > 0) {
        std::stringstream error;
        error <<"StartCpu failed because name:"<< name <<" already exists.";
        throw std::runtime_error(error.str());
      }

      timespec start { .tv_sec = 0, .tv_nsec = 0};
      timespec stop { .tv_sec = 0, .tv_nsec = 0};
      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
      cpu_.try_emplace(name, std::make_tuple(std::this_thread::get_id(), start, stop));
      return start;
    }
  }

  timespec StopCpu(const std::string& name) {
    {
      std::scoped_lock lock(mutex_);

      // Check if start has been called for the component.
      if (cpu_.count(name) == 0) {
        std::stringstream error;
        error <<"StopCpu failed because StartCpu on "<< name <<" hasn't been called yet.";
        throw std::runtime_error(error.str());
      }

      // start and stop for the same component must be on the same thread
      auto thread_id = std::get<0>(cpu_[name]);
      if (thread_id != std::this_thread::get_id()) {
        std::stringstream error;
        error <<"StopCpu failed with because StartCpu with name" << name <<" was called on thread:" << thread_id <<", but StopCpu called on different thread:<< name <<" <<std::this_thread::get_id();
        throw std::runtime_error(error.str());
      }

      timespec stop { .tv_sec = 0, .tv_nsec = 0};
      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &stop);
      std::get<2>(cpu_[name]) = stop;
      return stop;
    }
  }

  time_point StartLatency(const std::string& name) {
    {
      std::scoped_lock lock(mutex2_);

      if (latency_.count(name) > 0) {
        std::stringstream error;
        error <<"StartLatency failed because name:"<<name<<" already exists.";
        throw std::runtime_error(error.str());
      }

      auto start = std::chrono::steady_clock::now();
      auto stop = start;
      latency_.try_emplace(name, std::make_pair(start, stop));
      return start;
    }
  }

  time_point StopLatency(const std::string& name) {
    {
      // Check if start has been called for the component.
      if (latency_.count(name) == 0) {
        std::stringstream error;
        error <<"StopLatency failed because StartLatency on "<< name <<" hasn't been called yet.";
        throw std::runtime_error(error.str());
      }

      std::scoped_lock lock(mutex2_);
      auto stop = std::chrono::steady_clock::now();
      latency_[name].second = stop;
      return stop;
    }
  }

  void Reset() noexcept {
    {
      std::scoped_lock lock(mutex_);
      cpu_.clear();
    }
    {
      std::scoped_lock lock(mutex2_);
      latency_.clear();
    }
  }

  uint64_t GetCpuMilisec(const std::string& name = "") noexcept { return GetCpuTotal().count() / 1000000; }

  uint64_t GetCpuMicrosec(const std::string& name = "") noexcept { return GetCpuTotal().count() / 1000; }

  uint64_t GetCpuNanosec(const std::string& name = "") noexcept { return GetCpuTotal().count(); }

  uint64_t GetLatencyMilisec(const std::string& name = "") noexcept { return GetLatencyTotal().count() / 1000000; }

  uint64_t GetLatencyMicrosec(const std::string& name = "") noexcept { return GetLatencyTotal().count() / 1000; }

  uint64_t GetLatencyNanosec(const std::string& name = "") noexcept { return GetLatencyTotal().count(); }

  auto cpu_cbegin() noexcept { return cpu_.cbegin(); }

  auto cpu_cend() noexcept { return cpu_.cend(); }

  auto latency_cbegin() noexcept { return latency_.cbegin(); }

  auto latency_cend() noexcept { return latency_.cend(); }

 private:
  std::chrono::nanoseconds GetCpuTotal() {
    std::chrono::nanoseconds total = std::chrono::nanoseconds::zero();
    for(const auto& [key, val] : cpu_){
      total += DiffCpu(std::get<1>(val), std::get<2>(val)); 
    }

    return total;
  }

  std::chrono::nanoseconds GetLatencyTotal() {
    std::chrono::nanoseconds total = std::chrono::nanoseconds::zero();
    for(const auto& [key, val] : latency_){
      total += std::chrono::duration_cast<std::chrono::nanoseconds>(std::get<1>(val) - std::get<0>(val)); 
    }

    return total;
  }
  
  std::mutex mutex_;
  std::mutex mutex2_;
  std::map<std::string, std::tuple<std::thread::id, timespec, timespec>> cpu_; 
  std::map<std::string, std::pair<time_point, time_point>> latency_; 
};

void TestLatencyWrongUsage() {
  std::set<std::string> names;
  auto collector = std::make_unique<PerfCollector>();
  collector->StartLatency("com1");
  for (int i=0;i<100000;i++){}

  // 1. Test error case 1: 2 consecutive starts for same component
  auto expectedExceptionThrown = false;
  try 
  {
    collector->StartLatency("com1");
  }
  catch (std::runtime_error& e)
  {
    expectedExceptionThrown = true;
  }

  if (!expectedExceptionThrown) {
    std::cout<<"Didn't throw expected exception 1 \n";
    return;
  }

  collector->StopLatency("com1");
  expectedExceptionThrown = false;

  // 2. Test error case 2: call stop without start.
  try 
  {
    collector->StopLatency("com2");
  }
  catch (std::runtime_error& e)
  {
    expectedExceptionThrown = true;
  }

  if (!expectedExceptionThrown) {
    std::cout<<"Didn't throw expected exception 2 \n";
    return;
  }

  expectedExceptionThrown = false;
  std::cout<<"TestLatencyWrongUsage success\n";
}

void TestLatencyNormal() {
  std::set<std::string> names;
  auto collector = std::make_unique<PerfCollector>();
  collector->StartLatency("com1");
  names.emplace("com1");
  for (int i=0;i<100000;i++){}
  collector->StopLatency("com1");

  collector->StartLatency("com2");
  names.emplace("com2");
  for (int i=0;i<100000;i++){}
  collector->StopLatency("com2");

  collector->StartLatency("com3");
  names.emplace("com3");
  for (int i=0;i<100000;i++){}
  collector->StopLatency("com3");

  auto total = std::chrono::nanoseconds::zero();
  for (auto it = collector->latency_cbegin(); it != collector->latency_cend(); ++it) {
    auto fit = names.find (it->first);
    if (fit == names.end()) {
      // can't find the name, test failure
      std::cout<<"can't find name\n";
      return;
    }

    names.erase(fit);
    auto latency_start = std::get<0>(it->second);
    auto latency_end = std::get<1>(it->second);
    total += std::chrono::duration_cast<std::chrono::nanoseconds>(latency_end - latency_start);
  }

  if (names.size() != 0) {
    // names don't match, test failure
    std::cout<<"names set don't match\n";
    return;
  }

  if (collector->GetLatencyNanosec() != total.count()) {
    // test failure

    std::cout<<"GetLatencyNanosec failed "<<collector->GetLatencyNanosec()<<" "<<total.count()<<"\n";
    return;
  }
  
  if (collector->GetLatencyMicrosec() != total.count() / 1000) {
    // test failure
    std::cout<<"GetLatencyMicrosec failed\n";
    return;
  }

  if (collector->GetLatencyMilisec() != total.count() / 1000000) {
    // test failure
    std::cout<<"GetLatencyMilisec failed\n";
    return;
  }

  collector->Reset();
  auto size = 0;
  for (auto it = collector->latency_cbegin(); it != collector->latency_cend(); ++it)
    size++;

  if (size != 0) {
    // test failure
    std::cout<<"Reset failed\n";
    return;
  }

  std::cout<<"TestLatencyNormal success\n";
}

void TestCpuNormal() {
  std::set<std::string> names;
  auto collector = std::make_unique<PerfCollector>();
  collector->StartCpu("com1");
  names.emplace("com1");
  for (int i=0;i<100000;i++){}
  collector->StopCpu("com1");

  collector->StartCpu("com2");
  names.emplace("com2");
  for (int i=0;i<100000;i++){}
  collector->StopCpu("com2");

  collector->StartCpu("com3");
  names.emplace("com3");
  for (int i=0;i<100000;i++){}
  collector->StopCpu("com3");

  auto total = std::chrono::nanoseconds::zero();
  for (auto it = collector->cpu_cbegin(); it != collector->cpu_cend(); ++it) {
    auto fit = names.find (it->first);
    if (fit == names.end()) {
      // can't find the name, test failure
      std::cout<<"can't find name\n";
      return;
    }

    names.erase(fit);
    auto cpu_start = std::get<1>(it->second);
    auto cpu_end = std::get<2>(it->second);
    total += PerfCollector::DiffCpu(cpu_start, cpu_end);
  }

  if (names.size() != 0) {
    // names don't match, test failure
    std::cout<<"names set don't match\n";
    return;
  }

  if (collector->GetCpuNanosec() != total.count()) {
    // test failure

    std::cout<<"GetCpuNanosec failed\n";
    return;
  }
  
  if (collector->GetCpuMicrosec() != total.count() / 1000) {
    // test failure
    std::cout<<"GetCpuMicrosec failed\n";
    return;
  }

  if (collector->GetCpuMilisec() != total.count() / 1000000) {
    // test failure
    std::cout<<"GetCpuMilisec failed\n";
    return;
  }

  collector->Reset();
  auto size = 0;
  for (auto it = collector->cpu_cbegin(); it != collector->cpu_cend(); ++it)
    size++;

  if (size != 0) {
    // test failure
    std::cout<<"Reset failed\n";
    return;
  }

  std::cout<<"TestCpuNormal success\n";
}

void TestCpuWrongUsage() {
  std::set<std::string> names;
  auto collector = std::make_unique<PerfCollector>();
  collector->StartCpu("com1");
  for (int i=0;i<100000;i++){}

  // 1. Test error case 1: 2 consecutive starts for same component
  auto expectedExceptionThrown = false;
  try 
  {
    collector->StartCpu("com1");
  }
  catch (std::runtime_error& e)
  {
    expectedExceptionThrown = true;
  }

  if (!expectedExceptionThrown) {
    std::cout<<"Didn't throw expected exception 1 \n";
    return;
  }

  collector->StopCpu("com1");
  expectedExceptionThrown = false;

  // 2. Test error case 2: call stop without start.
  try 
  {
    collector->StopCpu("com2");
  }
  catch (std::runtime_error& e)
  {
    expectedExceptionThrown = true;
  }

  if (!expectedExceptionThrown) {
    std::cout<<"Didn't throw expected exception 2 \n";
    return;
  }

  expectedExceptionThrown = false;

  // 3. Test error case 3: start/stop on same thread
  collector->StartCpu("com3");
  for (int i=0;i<100000;i++){}

  std::thread t([&expectedExceptionThrown](PerfCollector* collector_ptr) {
    try {
      collector_ptr->StopCpu("com3");
    }
    catch (std::runtime_error& e)
    {
      expectedExceptionThrown = true;
    }
  }, collector.get());

  t.join();
  if (!expectedExceptionThrown) {
    std::cout<<"Didn't throw expected exception 2 \n";
    return;
  }

  std::cout<<"TestCpuWrongUsage success\n";
}

int main() {
  TestCpuNormal();
  TestCpuWrongUsage();
  TestLatencyNormal();
  TestLatencyWrongUsage();
  return 0;
}
