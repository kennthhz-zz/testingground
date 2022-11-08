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

int main(int argc, char *argv[]) {
  int b_fd, s_fd;
  if (argc > 1) { 
    b_fd = FillFile("large.txt", big_file_size);
    s_fd = FillFile("small.txt", small_file_size);
  }

  b_fd = open("large.txt", O_RDONLY | O_DIRECT);
  s_fd = open("small.txt", O_RDONLY | O_DIRECT);

  std::srand(std::time(nullptr));
  
  void *buf;
  int ps=getpagesize();
  posix_memalign(&buf, ps, page_size);

  long long nano_seconds = 0;

  // number of random pages to read
  int iter = 256 * 100;

  for (int i = 0; i < iter; i++) {
    int page_index = std::rand() % big_file_pages;

    auto start = std::chrono::steady_clock::now();
    pread (b_fd, buf, page_size, page_index * page_size);
    auto end = std::chrono::steady_clock::now();
    nano_seconds += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  }

  std::cout<<"large file average random 1 page direct IO read in nanoseconds is:"<<nano_seconds/iter<<"\n";

 nano_seconds = 0;
 for (int i = 0; i < iter; i++) {

    auto start = std::chrono::steady_clock::now();
    pread (b_fd, buf, page_size, i);
    auto end = std::chrono::steady_clock::now();
    nano_seconds += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  }

  std::cout<<"large file average sequential 1 page direct IO read in nanoseconds is:"<<nano_seconds/iter<<"\n";

  nano_seconds = 0;

  for (int i = 0; i < iter; i++) {
    int page_index = std::rand() % small_file_pages;

    auto start = std::chrono::steady_clock::now();
    pread (s_fd, buf, page_size, page_index * page_size);
    auto end = std::chrono::steady_clock::now();
    nano_seconds += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  }

  std::cout<<"small file average random 1 page direct IO read in nanoseconds is:"<<nano_seconds/iter<<"\n";

  free(buf);
  close(b_fd);
  close(s_fd);

  return 0;
}