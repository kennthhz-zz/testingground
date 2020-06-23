#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <sys/un.h>

using namespace std;
void error(const char *msg)
{
  perror(msg);
  exit(0);
}

int main(int argc, char *argv[])
{
  int sockfd, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char buffer[256];
  if (argc < 3)
  {
    fprintf(stderr, "usage %s tcp hostname port. or %s unix path\n", argv[0], argv[0]);
    exit(0);
  }

  string socket_type(argv[1]);
  auto isTcp = socket_type.compare("tcp");

  if (isTcp == 0)
  {
    portno = atoi(argv[3]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
      error("ERROR opening socket");
    server = gethostbyname(argv[2]);
    if (server == NULL)
    {
      fprintf(stderr, "ERROR, no such host\n");
      exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
      error("ERROR connecting");

    while(true)
    {
      printf("Please enter the message: ");
      bzero(buffer, 256);
      fgets(buffer, 255, stdin);
      n = write(sockfd, buffer, strlen(buffer));
      if (n < 0)
        error("ERROR writing to socket");
      bzero(buffer, 256);
      n = read(sockfd, buffer, 255);
      if (n < 0)
        error("ERROR reading from socket");
      printf("%s\n", buffer);
    }
    close(sockfd);
    return 0;
  }
  else
  {
    struct sockaddr_un addr;
    int fd, rc;

    char *socket_path = argv[2];

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
      perror("socket error");
      exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (*socket_path == '\0')
    {
      *addr.sun_path = '\0';
      strncpy(addr.sun_path + 1, socket_path + 1, sizeof(addr.sun_path) - 2);
    }
    else
    {
      strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
      perror("connect error");
      exit(-1);
    }

      while (true)
      {
        printf("Please enter the message: ");
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        printf("write %s\n", buffer);
        n = write(fd, buffer, strlen(buffer));
        if (n < 0)
          printf("ERROR writing to socket");
        printf("write complete %s\n", buffer);
        bzero(buffer, 256);
        n = read(fd, buffer, 255);
        if (n < 0)
          printf("ERROR reading from socket");
        printf("read %s\n", buffer);
      }

      close(fd);
      return 0;
    }
}
