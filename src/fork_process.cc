// todo

#include <sys/types.h>
#include <unistd.h>
#include <iostream>

int main()
{
  if (fork() == 0)
  {
    execl("./simple_child_process", "arg1", NULL);  
  }

  std::cout << "I am here1"
            << "\n";
  sleep(5);
  std::cout << "I am here2"
            << "\n";
  return 0;
}