#include <time.h>	/* for clock_gettime */
#include <memory>
#include <iostream>
#include <thread>
#include <chrono> 
#include "../../../folly/folly/concurrency/UnboundedQueue.h"
#include "MpScQueue.h"

const int BILLION = 1000000000L;

struct MyNode {
  std::string s1;
  uint64_t s2;
  int s3;
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

int main() {

  const int count = 100000000;
  const std::string enqueue_op = "enqueue";
  const std::string dequeue_op = "dequeue";

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
    std::cout<<"folly enqueue elapsed time: "<< diff <<" nsec\n";
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
    std::cout<<"folly dequeue elapsed time: "<< diff <<" nsec. empty:"<<empty<<" total:"<<total<<"\n";
  };

  std::thread t1(folly_func, 0, count);
  std::thread t2(folly_func, count, count);
  std::thread t3(folly_func, 2*count, count);
  std::thread t4(folly_func, 3*count, count);
  std::this_thread::sleep_for (std::chrono::seconds(10));
  std::thread t5(folly_dequeue_func, count);
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();

  std::cout<<"Now Jiffy\n";
  auto jiffy_queue = std::make_shared<MpScQueue<int>>(10000);
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
    std::cout<<"jiffy enqueue elapsed time: "<< diff <<" nsec\n";
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
    std::cout<<"jiffy dequeue elapsed time: "<< diff <<" nsec. empty:"<<empty<<" total:"<<total<<"\n";
  };

  std::thread tt1(jiffy_func, 0, count);
  std::thread tt2(jiffy_func, count, count);
  std::thread tt3(jiffy_func, 2*count, count);
  std::thread tt4(jiffy_func, 3*count, count);
  std::this_thread::sleep_for (std::chrono::seconds(10));
  std::thread tt5(jiffy_dequeue_func, count);
  tt1.join();
  tt2.join();
  tt3.join();
  tt4.join();
  tt5.join();
}

