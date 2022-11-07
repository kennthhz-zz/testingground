#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <chrono>

constexpr unsigned long long int big_file_size = (unsigned long long int)1024*1024*1024*4;
constexpr int small_file_size = 1024*1024*32;
const int page_size = 4096;
constexpr long long big_file_pages = big_file_size / page_size;
constexpr int small_file_pages = small_file_size / page_size;

int FillFile(const std::string& path, long long sizeInByte) {
  void *buf;
  int ps=getpagesize();

  posix_memalign(&buf, ps, page_size); 
  auto fd = open(path.c_str(), O_CREAT | O_RDWR |O_DIRECT, 00666);

  auto iter = sizeInByte / page_size;
  for (int i = 0; i < iter; i++) {
    write(fd, buf, page_size);
  }

  free(buf);
  return fd;
}

int main() {

  auto b_fd = FillFile("large.txt", big_file_size);
  auto s_fd = FillFile("small.txt", small_file_size);

  std::srand(std::time(nullptr));
  
  void *buf;
  int ps=getpagesize();
  posix_memalign(&buf, ps, page_size);

  for (int j = 0; j<2; j++) {
    long long micro_seconds = 0;

    for (int i = 0; i < 256 * 100; i++) {
      int page_index = std::rand() % big_file_pages;

      auto start = std::chrono::steady_clock::now();
      lseek(j == 0? b_fd : s_fd, page_index * page_size, SEEK_SET);
      read(j == 0? b_fd : s_fd, buf, page_size);
      auto end = std::chrono::steady_clock::now();
      micro_seconds += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }


    std::cout<<"average random 1 page direct IO read in microseconds is:"<<micro_seconds/(256*100);
  }

  free(buf);
  close(b_fd);
  close(s_fd);

  return 0;
}