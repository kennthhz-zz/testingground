#include <time.h>	/* for clock_gettime */
#include <memory>
#include <iostream>
#include <set>
#include <thread>
#include <chrono> 
#include "../../../folly/folly/concurrency/UnboundedQueue.h"
#include "MpScQueue.h"
#include "MyMpScQueue.h"

const int BILLION = 1000000000L;
const int count = 300000000;
const int thread_count = 3;

struct MyNode {
  MyNode(uint64_t val) : val_ { val} {
  }

  uint64_t val_;
};

void DoWork(std::function<void(int)> func, int count, const std::string& operation) {
  uint64_t diff;
	struct timespec start, end;
  
  clock_gettime(CLOCK_MONOTONIC, &start);	/* mark start time */
  func(count);
  clock_gettime(CLOCK_MONOTONIC, &end);	/* mark the end time */

  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  std::cout<<operation <<" elapsed time: "<< diff <<" nsec\n";
}

void SetCpu(std::thread& thread, int cpu = 0) {
  // affinitize thread to vcore 
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  int rc = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
  }
}

void bench_mark_myqueue() {
  std::cout<<"Now myqueue\n";
  auto my_queue = std::make_shared<MyMpScQueue<int>>();
  
  auto myqueue_func = [my_queue](uint_fast64_t start_num, uint_fast64_t count) {
    uint64_t diff;
	  struct timespec start, end, current;

    clock_gettime(CLOCK_REALTIME, &current);
    //std::cout<<"my_queue enqueue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
    clock_gettime(CLOCK_MONOTONIC, &start);	
    for (auto i = start_num; i < start_num + count; i++) {
      my_queue->enqueue(i);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"my_queue enqueue elapsed time: "<< (float)diff / 1000000000 <<" sec\n";
  };

  std::thread t[thread_count];
  for (int i = 0; i < thread_count; i++) {
    t[i] = std::thread(myqueue_func, i * count, count);
    SetCpu(t[i], i);
  }

  for (int i = 0; i < thread_count; i++) {
    t[i].join();
  }
}

void bench_mark_folly() {
  std::cout<<"Now folly\n";
  auto folly_queue = std::make_shared<folly::UMPMCQueue<int, false, 6>>();
  
  auto folly_func = [folly_queue](uint_fast64_t start_num, uint_fast64_t count) {
    uint64_t diff;
	  struct timespec start, end, current;

    clock_gettime(CLOCK_REALTIME, &current);
    //std::cout<<"folly enqueue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
    clock_gettime(CLOCK_MONOTONIC, &start);	
    for (auto i = start_num; i < start_num + count; i++) {
      folly_queue->enqueue(i);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"folly enqueue elapsed time: "<< (float)diff / 1000000000 <<" sec\n";
  };

  auto folly_dequeue_func = [folly_queue](uint_fast64_t count) {
    int val = 0;
    uint_fast64_t total = 0;
    uint_fast64_t empty = 0;
    uint64_t diff;
	  struct timespec start, end, current;

    clock_gettime(CLOCK_REALTIME, &current);
    //std::cout<<"folly dequeue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
  
    clock_gettime(CLOCK_MONOTONIC, &start);	
    while (total < count) {
      if (folly_queue->try_dequeue(val)) {
        total++;
      } else { empty++;}
    }
    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"folly dequeue elapsed time: "<< (float)diff / 1000000000<<" sec. empty:"<<empty<<" total:"<<total<<"\n";
  };

  std::thread t[thread_count];
  for (int i = 0; i < thread_count; i++) {
    t[i] = std::thread(folly_func, i * count, count);
    SetCpu(t[i], i);
  }

  for (int i = 0; i < thread_count; i++) {
    t[i].join();
  }

  std::thread dt(folly_dequeue_func, thread_count * count);
  dt.join();
}

void bench_mark_jiffy() {
  std::cout<<"Now jiffy\n";
  auto jiffy_queue = std::make_shared<MpScQueue<int>>();

  auto jiffy_enqueue_func = [jiffy_queue](uint_fast64_t start_num, uint_fast64_t count) {
    uint64_t diff;
	  struct timespec start, end, current;
    clock_gettime(CLOCK_REALTIME, &current);
    //std::cout<<"jiffy enqueue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
    clock_gettime(CLOCK_MONOTONIC, &start);	
    for (auto i = start_num; i < start_num + count; i++) {
      jiffy_queue->enqueue(i);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"jiffy enqueue elapsed time: "<< (float)diff / 1000000000 <<" sec\n";
  };

  auto jiffy_dequeue_func = [jiffy_queue](int count) {
    int val = 0;
    uint_fast64_t total = 0;
    uint_fast64_t empty = 0;
    uint64_t diff;
	  struct timespec start, end, current;

    clock_gettime(CLOCK_REALTIME, &current);
    //std::cout<<"jiffy dequeue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
  
    clock_gettime(CLOCK_MONOTONIC, &start);	
    while (total < count) {
      if (jiffy_queue->dequeue(val)) {
        total++;
      } else { empty++;}
    }
    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"jiffy dequeue elapsed time: "<< (float)diff / 1000000000<<" sec. empty:"<<empty<<" total:"<<total<<"\n";
  };

  std::thread t[thread_count];
  for (int i = 0; i < thread_count; i++) {
    t[i] = std::thread(jiffy_enqueue_func, i * count, count);
    SetCpu(t[i], i);
  }

  for (int i = 0; i < thread_count; i++) {
    t[i].join();
  }

  std::thread dt(jiffy_dequeue_func, thread_count * count);
  dt.join();
}

int main() {
  bench_mark_myqueue();
  bench_mark_folly();
  bench_mark_jiffy();
  return 0;
}

