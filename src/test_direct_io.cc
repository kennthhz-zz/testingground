#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <iostream>
#include <unistd.h>

void FillFile(const std::string& path, int sizeInByte) {
  void *buf;
  int ps=getpagesize();
  std::cout<<"mem pagesize:"<<ps<<"\n";

  posix_memalign(&buf, ps, 4096); 
  auto fd = open(path.c_str(), O_CREAT | O_WRONLY |O_DIRECT, 00666);

  auto iter = sizeInByte / 4096;
  for (int i = 0; i < iter; i++) {
    write(fd, buf, 4096);
  }

  close(fd);
  free(buf);
}

int main() {
  FillFile("test.txt", 4096*10);
  return 0;
}