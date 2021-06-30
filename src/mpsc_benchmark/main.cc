#include <time.h>	/* for clock_gettime */
#include <memory>
#include <iostream>
#include <set>
#include <thread>
#include <chrono> 
#include "../../../folly/folly/concurrency/UnboundedQueue.h"
#include "MpScQueue.h"

const int BILLION = 1000000000L;
const int count = 250000000;

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

void bench_mark_folly() {
  std::cout<<"Now folly\n";
  auto folly_queue = std::make_shared<folly::UMPMCQueue<int, false, 6>>();
  
  auto folly_func = [folly_queue](int start_num, int count) {
    uint64_t diff;
	  struct timespec start, end, current;

    clock_gettime(CLOCK_REALTIME, &current);
    std::cout<<"folly enqueue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
    clock_gettime(CLOCK_MONOTONIC, &start);	
    for (int i = start_num; i < start_num + count; i++) {
      folly_queue->enqueue(i);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"folly enqueue elapsed time: "<< diff / 1000000000 <<" sec\n";
  };

  auto folly_dequeue_func = [folly_queue](int count) {
    int val = 0;
    int total = 0;
    int empty = 0;
    uint64_t diff;
	  struct timespec start, end, current;

    clock_gettime(CLOCK_REALTIME, &current);
    std::cout<<"folly dequeue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
  
    clock_gettime(CLOCK_MONOTONIC, &start);	
    while (total < count) {
      if (folly_queue->try_dequeue(val)) {
        total++;
      } else { empty++;}
    }
    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"folly dequeue elapsed time: "<< diff / 1000000000<<" sec. empty:"<<empty<<" total:"<<total<<"\n";
  };

  std::thread t1(folly_func, 0, count);
  SetCpu(t1);
  std::thread t2(folly_func, count, count);
  SetCpu(t2);
  std::thread t3(folly_func, 2*count, count);
  SetCpu(t3);
  std::thread t4(folly_func, 3*count, count);
  SetCpu(t4);
  std::this_thread::sleep_for (std::chrono::seconds(5));
  std::thread t5(folly_dequeue_func, 4 * count);
  SetCpu(t5);
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
}

void bench_mark_jiffy() {
  std::cout<<"Now Jiffy\n";
  auto jiffy_queue = std::make_shared<MpScQueue<int>>();
  auto jiffy_func = [jiffy_queue](int start_num, int count) {
    uint64_t diff;
    struct timespec start, end, current;

    clock_gettime(CLOCK_REALTIME, &current);
    std::cout<<"Jiffy enqueue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
    
    clock_gettime(CLOCK_MONOTONIC, &start);	
    for (int i = start_num; i < start_num + count; i++) {
      jiffy_queue->enqueue(i);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"jiffy enqueue elapsed time: "<< diff / 1000000000 <<" sec\n";
  };

  auto jiffy_dequeue_func = [jiffy_queue](int count) {
    int val = 0;
    int total = 0;
    int empty = 0;
    uint64_t diff;
	  struct timespec start, end, current;

    clock_gettime(CLOCK_REALTIME, &current);
    std::cout<<"Jiffy dequeue start time: "<< current.tv_sec * BILLION +  current.tv_nsec<<" nsec\n";
  
    clock_gettime(CLOCK_MONOTONIC, &start);	
    while (total < count) {
      if (jiffy_queue->dequeue(val)) {
        total++;
      } else { empty++;}
    }

    clock_gettime(CLOCK_MONOTONIC, &end);	

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"jiffy dequeue elapsed time: "<< diff / 1000000000 <<" sec. empty:"<<empty<<" total:"<<total<<"\n";
  };

  std::thread tt1(jiffy_func, 0, count);
  SetCpu(tt1);
  std::thread tt2(jiffy_func, count, count);
  SetCpu(tt2);
  std::thread tt3(jiffy_func, 2*count, count);
  SetCpu(tt3);
  std::thread tt4(jiffy_func, 3*count, count);
  SetCpu(tt4);
  std::this_thread::sleep_for (std::chrono::seconds(10));
  std::thread tt5(jiffy_dequeue_func, 4 * count);
  SetCpu(tt5);
  tt1.join();
  tt2.join();
  tt3.join();
  tt4.join();
  tt5.join();
}

void verify() {
  struct timespec start, end;
  const int enqueue_thread_count = 4;
  const int items_per_enqueue_thread = 25000000;
  constexpr int total_items = items_per_enqueue_thread * enqueue_thread_count;
  auto jiffy_queue = std::make_shared<MpScQueue<MyNode*>>();

  auto items = new MyNode*[total_items];
  for (uint64_t i = 0; i < total_items; i++) {
    items[i] = new MyNode(i);
  }

  auto jiffy_enqueue_func = [jiffy_queue, items, items_per_enqueue_thread](int start_index) {

    for (int i = start_index; i < start_index + items_per_enqueue_thread; i++) {
      jiffy_queue->enqueue(items[i]);
    }
  };

  auto jiffy_dequeue_func = [jiffy_queue, &end](int count) {
    uint64_t total = 0;
    std::set<uint64_t> result_set;
    MyNode* val = nullptr;

    while (total < count) {
      if (jiffy_queue->dequeue(val)) {
        total++;
        result_set.insert(val->val_);
      }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);	
    assert(result_set.size() == count);
  };

  clock_gettime(CLOCK_MONOTONIC, &start);	
  std::thread enqueue_threads[enqueue_thread_count];
  for (int i = 0; i < enqueue_thread_count; i++) {
    enqueue_threads[i] = std::thread(jiffy_enqueue_func, i * items_per_enqueue_thread);
    SetCpu(enqueue_threads[i], i);
  }

  std::thread dequeue_thread(jiffy_dequeue_func, total_items);
  SetCpu(dequeue_thread, enqueue_thread_count);

  for (int i = 0; i < enqueue_thread_count; i++) { 
    enqueue_threads[i].join();
  }

  dequeue_thread.join();

  auto diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  std::cout<<"Total elapsed time: "<< diff / 1000000000 <<" seconds\n";

  for (int i = 0; i < total_items; i++) {
    delete items[i];
  }
  delete[] items;
}

int main() {
  bench_mark_folly();
  bench_mark_jiffy();
  //verify();
  return 0;
}

