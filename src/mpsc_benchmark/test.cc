
#include "MyMpScQueue.h"
#include <cstring>
#include <thread>

const int BILLION = 1000000000L;
MyMpScQueue<int> q;

void SetCpu(std::thread& thread, int cpu = 0) {
  // affinitize thread to vcore 
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  int rc = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
  }
}

void enqueue(int start, int length) {
  for (int i = start; i < start + length; i++) {
    q.enqueue(i);
  }
}

int main() {
  int length = 500000000;
  struct timespec start, end;
  int thread_count = 2;
  std::thread t[thread_count];

  clock_gettime(CLOCK_MONOTONIC, &start);	/* mark start time */
  for (int i = 0; i < thread_count; i++) {
    t[i] = std::thread(enqueue, i * length, length);
    SetCpu(t[i], i);
  }

  for (int i = 0; i < thread_count; i++) {
    t[i].join();
  }
  clock_gettime(CLOCK_MONOTONIC, &end);	/* mark the end time */
  auto diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  std::cout<<" elapsed time: "<< (float)diff/BILLION <<" secs\n";

  std::cout<<"done\n";
  q.verify();
  return 0;
}