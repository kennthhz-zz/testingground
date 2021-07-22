// Copyright (c) 2019-present, Tencent, Inc.  All rights reserved.
#pragma once

#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include "folly/Function.h"
#include "MpScQueue.h"

using TaskFunc = folly::Function<void(void)>;

enum TaskPriority : std::int8_t {
  kLowest = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
};

static const std::int8_t TaskPriority_Count = 4;
static constexpr std::int8_t TaskPriority_TotalWeight() {
  auto total = 0;
  for (int8_t p = TaskPriority::kLowest; p <= TaskPriority::kHigh; p++)
    total += p;
  return total;
}

class Executor final {
 public:
  Executor(const Executor&) = delete;
  Executor& operator=(const Executor&) = delete;
  Executor(Executor&&) = delete;
  Executor& operator=(Executor&&) = delete;

  explicit Executor(int start_vcore, int count_vcore) {
    for (int i = start_vcore; i < start_vcore + count_vcore; i++)
      mpsc_executors_.emplace_back(std::make_unique<MpScExecutor>(i));
  }

  void Start() {
    for (auto& e : mpsc_executors_) 
      e->Start(this);
  }

  void Shutdown() {
    for (auto& e : mpsc_executors_)
        e->Shutdown();
    std::cout << "shutdown Executor" << std::endl;
  }

  bool AddCPUTask(TaskFunc&& task_func, const int vcore, TaskPriority priority) {
    try {
      mpsc_executors_[vcore]->Add(std::forward<TaskFunc>(task_func), priority);
    } catch (std::exception& ex) {
      return false;
    }

    return true;
  }

 private:
  class MpScExecutor final {
   public:
    explicit MpScExecutor(int vcore) : vcore_(vcore), counter_{0} {
    }

    void Start(Executor *executor) {
      std::thread t(Execute, this);
      std::cout<<"MpScExecutor start new thread\n";

      if (vcore_ != -1) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(vcore_, &cpuset);
        int rc = pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0)
          throw "can't bind to thread";
      }

      thread_ = std::move(t);
      // TODO: set thread name
    }

    void Shutdown() {
      Add([this](){ this->shutdown_ = true; }, TaskPriority::kHigh);
      thread_.join();
    }

    void Add(TaskFunc&& task_func, TaskPriority priority) {
      queues_[priority - 1].enqueue(std::forward<TaskFunc>(task_func));
    }

   private:
    void static Execute(MpScExecutor* e) {
      while(true) {
        if (e->shutdown_)
          break;

        auto tmp = (e->counter_++) % TaskPriority_TotalWeight() + 1;
        auto p = TaskPriority::kLowest;
        if (tmp >= 7) 
          p = TaskPriority::kHigh;
        else if (tmp >= 4)
          p = TaskPriority::kMedium;
        else if (tmp >= 2)
          p = TaskPriority::kLow;
        
        TaskFunc task;
        bool found = false;
        if (!e->queues_[p - 1].dequeue(task)) {
          for (int8_t p1 = TaskPriority::kHigh; p1 >= TaskPriority::kLowest; p1--) {
            if (e->queues_[p1 - 1].dequeue(task)) {
              found = true;
              break;
            }
          }
        } else {
          found = true;
        }

        if (found) {
          task();
          continue;
        } else {
          // Sleep
        }
      }
    }

   private:
    MpScQueue<TaskFunc> queues_[TaskPriority_Count];
    const int vcore_;
    std::thread thread_;
    uint_fast64_t counter_;
    bool shutdown_;
  };

 private:
  using MpScExecutorPtr = std::unique_ptr<MpScExecutor>;
  using MpScExecutorPtrs = std::vector<MpScExecutorPtr>;
  MpScExecutorPtrs mpsc_executors_;
  std::atomic<bool> shutdown_;
};