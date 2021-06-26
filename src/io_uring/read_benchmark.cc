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

int total_read = 0;

std::string convertToString(char* a, int size)
{
    int i;
    std::string s = "";
    for (i = 0; i < size; i++) {
        s = s + a[i];
    }
    return s;
}

struct io_data {
    struct iovec iov;
};

static int get_file_size(int fd, off_t *size) {
    struct stat st;

    if (fstat(fd, &st) < 0 )
        return -1;
    if(S_ISREG(st.st_mode)) {
        *size = st.st_size;
        return 0;
    } else if (S_ISBLK(st.st_mode)) {
        unsigned long long bytes;

        if (ioctl(fd, BLKGETSIZE64, &bytes) != 0)
            return -1;

        *size = bytes;
        return 0;
    }
    return -1;
}

static int setup_context(unsigned entries, struct io_uring *ring) {
    int ret;

    ret = io_uring_queue_init(entries, ring, 0);
    if( ret < 0) {
        fprintf(stderr, "queue_init: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}

static int queue_read(struct io_uring *ring, const int fd, off_t size, off_t offset) {
    struct io_uring_sqe *sqe;
    struct io_data *data;

    data = (io_data*)malloc(size + sizeof(*data));
    if (!data)
        return 1;

    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        free(data);
        return 1;
    }

    data->iov.iov_base = data + 1;
    data->iov.iov_len = size;

    io_uring_prep_readv(sqe, fd, &data->iov, 1, offset);
    io_uring_sqe_set_data(sqe, data);
    return 0;
}

static int get_read_results(struct io_uring *ring)
{
  auto got_comp = 0;
  auto ret = 0;
  struct io_uring_cqe *cqe;
  struct io_data *data;
  
  while (true) {
    if (!got_comp) {
      ret = io_uring_wait_cqe(ring, &cqe);
      if (ret != 0) {
        std::cout<<"io_uring_wait_cqe failed with:"<<ret<<"\n";
        return ret;
      }
      got_comp = 1;
    } else {
      ret = io_uring_peek_cqe(ring, &cqe);
      if (ret == -EAGAIN) {
        break;
      } else if (ret != 0) {
        std::cout<<"io_uring_peek_cqe: " <<ret <<" " << strerror(-ret) <<"-EAGAIN value is :"<<-EAGAIN<<"\n";
        return ret;
      }
    }

    if (!cqe) {
      std::cout<<"cqe empty\n";
      return -1;
    }

    data = (io_data*)io_uring_cqe_get_data(cqe);
    total_read += data->iov.iov_len;
    io_uring_cqe_seen(ring, cqe);
    delete data;
  }

  return 0;
}

int io_uring_read(
  struct io_uring *ring, 
  const int fd, 
  const int size, 
  const int batch_size, 
  const int read_buffer_size_limit)
{
  int remaining = size;
  int pending_reads = 0;
  int file_offset = 0;

  while (true) 
  {
    off_t buffer_size = remaining > read_buffer_size_limit ? read_buffer_size_limit : remaining;
    if (queue_read(ring, fd, buffer_size, file_offset) != 0) {
      return -1;
    }

    remaining -= buffer_size;
    file_offset += buffer_size;
    pending_reads++;

    if (pending_reads == batch_size || remaining == 0)
    {
      io_uring_submit(ring);
      pending_reads = 0;

      if (get_read_results(ring) != 0) {
        return -1;
      }
    }

    if (remaining == 0)
      break;
  }

  return 0;
}

int regular_read(
  const int fd, 
  const int size,  
  const int read_buffer_size_limit,
  const int align)
{
  int file_offset = 0;
  int remaining = size;

  while (remaining) {
    // Align read buffer to filesystem block size
    // https://stackoverflow.com/questions/34182535/write-error-invalid-argument-when-file-is-opened-with-o-direct
    char *buff = (char *) malloc((int)read_buffer_size_limit + align);
    auto buff_aligned = (char *)(((uintptr_t)buff + align)&~((uintptr_t)align));

    off_t buffer_size = remaining > read_buffer_size_limit ? read_buffer_size_limit : remaining;
    auto ret = read(fd, buff_aligned, buffer_size);
    if (ret == -1) {
      perror(NULL);
      return -1;
    }

    remaining -= ret;
    file_offset += ret;
    total_read += ret;
    free(buff);
  }

  return 0;
}

int main(int argc, char *argv[]) 
{
    struct io_uring ring;
    off_t size;
    int ret;
  
    if (argc < 2) 
    {
      printf("Usage: %s <file>\n", argv[0]);
      return 1;
    }

    auto fd = open(argv[1], O_RDONLY | O_DIRECT);
    if (fd < 0) 
    {
      perror("open file");
      return 1;
    }

    struct stat fstat;
    stat(argv[1], &fstat); 
    auto blksize = (int)fstat.st_blksize;
    auto align = blksize-1;

    if (setup_context(1000, &ring))
      return 1;

    if (get_file_size(fd, &size))
      return 1;

    const int BILLION = 1000000000L;
    uint64_t diff;
	  struct timespec start, end, start_cpu, end_cpu;

    clock_gettime(CLOCK_MONOTONIC, &start);	
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_cpu);	

#ifdef IO_URING
    ret = io_uring_read(&ring, fd, size, 4, blksize);
    if (ret != 0) {
      std::cout<<"io_uring_read failed\n";
    }
#else
    ret = regular_read(fd, size, blksize, align);
    if (ret != 0) {
      std::cout<<"regular_read failed\n";
    }
#endif
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_cpu);	
    clock_gettime(CLOCK_MONOTONIC, &end);

    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    std::cout<<"elapsed time: "<< diff/1000000 <<" msec\n";

    diff = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + end_cpu.tv_nsec - start_cpu.tv_nsec;
    std::cout<<"CPU elapsed time: "<< diff/1000000 <<" msec\n";
    std::cout<<"Total read in bytes:"<<total_read<<"\n";

    close(fd);
    io_uring_queue_exit(&ring);
    return ret;
}