//todo
#include <iostream>
#include <unistd.h>

int main(int argc, char* argv[])
{
  std::cout << "in child process with arg " << argv[0]<<" "<<argv[1]<<"\n";
  return 0;
}