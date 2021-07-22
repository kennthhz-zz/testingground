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
#include "executor.h"

static const uint64_t RingSize = 3000;
static const uint64_t BuffSize = 4096;

struct io_w_data {
    int id;
    off_t offset;
    struct iovec *iov;
};

static int setup_iouring(unsigned entries, struct io_uring *ring) {
    int ret;

    ret = io_uring_queue_init(entries, ring, 0);
    if( ret < 0) {
        fprintf(stderr, "queue_init: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}

static void queue_iouring_write(struct io_uring *ring, struct io_w_data *data, int fd) {
  //std::cout<<"enqueue "<<data->id<<" offset:"<<data->offset<<"\n";
  io_uring_sqe* sqe = nullptr;
  while (true) {
    sqe = io_uring_get_sqe(ring);
    if (sqe == nullptr) {
      io_uring_submit(ring);
      //std::cout<<"submit "<<data->id<<"\n";
      continue;
    }

    break;
  }

  io_uring_prep_writev(sqe, fd, &data->iov[0], 1, data->offset);
  io_uring_sqe_set_data(sqe, data);
}

static void reap_iouring_completion(struct io_uring *ring) {
  struct io_uring_cqe *cqe;
  uint64_t total = 0;
  constexpr uint64_t total_size = 1000 * BuffSize * 1000;
  std::cout<<"total size should be:"<<total_size<<"\n";
  while (true) {
    auto ret = io_uring_wait_cqe(ring, &cqe);
    if (ret == 0 && cqe->res >=0) {
      struct io_w_data *rdata =(io_w_data *)io_uring_cqe_get_data(cqe);
      total += cqe->res;
      io_uring_cqe_seen(ring, cqe);
      free(rdata->iov[0].iov_base);
      if (total >= total_size) {
        std::cout<<"total:"<<total<<"\n";
        break;
      }
    }
  }

  std::cout<<"done\n";
}

static void worker(char* dir_path, io_uring* ring, Executor* executor, int vcore) {
  std::string file_path(dir_path);
  file_path += "/tmp_";

  std::stringstream ss;
  ss << std::this_thread::get_id();
  file_path += ss.str();
  // std::cout<<"file:"<<file_path<<"\n";
  auto fd = open(file_path.c_str(), O_WRONLY | O_DIRECT | O_CREAT, 0644);

  executor->AddCPUTask([ring, fd](){
        for (int j = 0; j < 1000; j++) {
            io_w_data* data = new io_w_data();
            data->iov = (iovec*)calloc(1, sizeof(struct iovec));
            void* buff;
            posix_memalign(&buff, BuffSize, BuffSize);
            memset(buff, 'a', BuffSize);
            data->iov[0].iov_base = buff;
            data->iov[0].iov_len = BuffSize;
            data->id = j;
            data->offset = j * BuffSize;

            queue_iouring_write(ring, data, fd);
        }
        io_uring_submit(ring);
      }, 
      vcore, 
      TaskPriority::kLow);
}

int main(int argc, char *argv[]) {
  Executor executor(0, 1);
  executor.Start();

  struct io_uring ring;
  setup_iouring(RingSize, &ring);

  const int t_count = 1000;
  std::thread submission_threads[t_count];

  std::thread reap_t(reap_iouring_completion, &ring);
  reap_t.detach();

  for (int i = 0; i < t_count; i++) {
    submission_threads[i] = std::thread(worker, argv[1], &ring, &executor, 0);
  }

  for (int i = 0; i < t_count; i++) {
    submission_threads[i].join();
  }

  std::this_thread::sleep_for (std::chrono::seconds(120));
  executor.Shutdown();
  io_uring_queue_exit(&ring);
}




