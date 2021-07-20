#include <stdio.h>
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

static const uint RingSize = 3;
static const uint BuffSize = 4096;

struct io_w_data {
    uint id;
    off_t offset;
    struct iovec iov;
    char data[BuffSize] ;
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

static int prepare_sqe(struct io_uring *ring, struct io_w_data *data, int fd) {
    auto sqe = io_uring_get_sqe(ring);
    if (sqe == nullptr) 
      return -1;

    io_uring_prep_writev(sqe, fd, &data->iov, 1, data->offset);
    io_uring_sqe_set_data(sqe, data);
    return 0;
}

static void queue_iouring_write(struct io_uring *ring, struct io_w_data *data, int fd) {

    auto ret = prepare_sqe(ring, data, fd);
    std::cout<<"sprepare_sqe returned:"<<ret<<"\n";
    if  (ret == -1) {
      ret = io_uring_submit(ring);
      std::cout<<"io_uring_submit return:"<<ret<<"\n";

      struct io_uring_cqe *cqe;
      ret = io_uring_wait_cqe(ring, &cqe);
      std::cout<<"io_uring_wait_cqe return:"<<ret<<"\n";
      struct io_w_data *rdata =(io_w_data *)io_uring_cqe_get_data(cqe);
      std::cout<<"rdata.id:"<<rdata->id<<"\n";
      io_uring_cqe_seen(ring, cqe);
    }
}

int main(int argc, char *argv[]) {

  struct io_uring ring;
  setup_iouring(RingSize, &ring);

  const int t_count = 1;
  std::thread submission_threads[t_count];
  for (int i = 0; i < t_count; i++) {
    std::string file_path(argv[1]);
    file_path += "/tmp_";
    file_path += std::to_string(i);
    std::cout<<"file:"<<file_path<<"\n";
    auto fd = open(file_path.c_str(), O_WRONLY | O_CREAT, 0644);

    io_w_data* data = new io_w_data();
    data->iov.iov_base = data->data;
    data->iov.iov_len = BuffSize;
    memset(data->data, 'a', BuffSize);

    for (int j = 0; j < 10; j++) {
      data->id = j;
      data->offset = j * BuffSize;
      queue_iouring_write(&ring, data, fd);
    }

    close(fd);
    io_uring_queue_exit(&ring);
  }


}




