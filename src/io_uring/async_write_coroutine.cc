#include <stdio.h>
#include <sstream>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "liburing.h"
#include <iostream>
#include <chrono> 
#include <time.h>
#include <thread>
#include <concepts>
#include <coroutine>
#include "executor.h"
#include "async_write_coroutine.h"

void async_result::await_suspend(std::coroutine_handle<promise_type> h) {
  if (async_) {
    file_info->promise = &h.promise();
  } else {
    handle_.promise().prev_ = &h.promise();
  }
}

static int setup_iouring(unsigned entries, struct io_uring *ring) {
    int ret;

    ret = io_uring_queue_init(entries, ring, 0);
    if( ret < 0) {
        fprintf(stderr, "queue_init: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}

static void queue_iouring_write(struct io_uring *ring, struct file_write_page *data, int fd) {
  io_uring_sqe* sqe = nullptr;
  while (true) {
    sqe = io_uring_get_sqe(ring);
    if (sqe == nullptr) {
      io_uring_submit(ring);
      continue;
    }

    break;
  }

  io_uring_prep_writev(sqe, fd, &data->iov[0], 1, data->offset);
  io_uring_sqe_set_data(sqe, data);
}

static void reap_iouring_completion(struct io_uring *ring, Executor* executor) {
  struct io_uring_cqe *cqe;
  uint64_t total = 0;
  constexpr uint64_t total_size = Files * BuffSize * Pages;
  std::cout<<"total size should be:"<<total_size<<"\n";
  while (true) {
    auto ret = io_uring_wait_cqe(ring, &cqe);
    if (ret == 0 && cqe->res >=0) {
      struct file_write_page *rdata =(file_write_page *)io_uring_cqe_get_data(cqe);
      rdata->file_info->size_written+= cqe->res;
      total += cqe->res;
      io_uring_cqe_seen(ring, cqe);
      free(rdata->iov[0].iov_base);

      if (rdata->file_info->size_written == rdata->file_info->size) {
        executor->AddCPUTask([file_info = rdata->file_info]() {
            auto h = std::coroutine_handle<async_result::promise_type>::from_promise(*file_info->promise);
            h.resume();
          }, 
          rdata->file_info->vcore, 
          TaskPriority::kHigh);
        }

      if (total >= total_size) {
        std::cout<<"total bytes written:"<<total<<"\n";
        break;
      }
    }
  }

  std::cout<<"reap_iouring_completion done\n";
}

static async_result write_one_file(io_uring* ring, file_write_info* file_info) {
  for (uint64_t j = 0; j < Pages; j++) {
    file_write_page* data = new file_write_page();
    data->file_info = file_info;
    data->iov = (iovec*)calloc(1, sizeof(struct iovec));
    void* buff;
    posix_memalign(&buff, BuffSize, BuffSize);
    memset(buff, 'a', BuffSize);
    data->iov[0].iov_base = buff;
    data->iov[0].iov_len = BuffSize;
    data->id = j;
    data->offset = j * BuffSize;

    queue_iouring_write(ring, data, file_info->fd);
  }
  
  io_uring_submit(ring);
  async_result result(true, file_info);
  co_await result;
  std::cout<<"Done writting for file:\n";
}

static void worker(char* dir_path, io_uring* ring, Executor* executor, int vcore) {
  std::string file_path(dir_path);
  file_path += "/tmp_";

  std::stringstream ss;
  ss << std::this_thread::get_id();
  file_path += ss.str();
  auto fd = open(file_path.c_str(), O_WRONLY | O_DIRECT | O_CREAT, 0644);

  executor->AddCPUTask([ring, fd, vcore]() {
        file_write_info* file_info = new file_write_info(fd, vcore);
        write_one_file(ring, file_info);
      }, 
      vcore, 
      TaskPriority::kMedium);
}

int main(int argc, char *argv[]) {
  Executor executor(0, 1);
  executor.Start();

  struct io_uring ring;
  setup_iouring(RingSize, &ring);

  std::thread submission_threads[Files];

  std::thread reap_t(reap_iouring_completion, &ring, &executor);
  reap_t.detach();

  for (uint64_t i = 0; i < Files; i++) {
    submission_threads[i] = std::thread(worker, argv[1], &ring, &executor, 0);
  }

  for (uint64_t i = 0; i < Files; i++) {
    submission_threads[i].join();
  }

  std::this_thread::sleep_for (std::chrono::seconds(120));
  executor.Shutdown();
  io_uring_queue_exit(&ring);
}




