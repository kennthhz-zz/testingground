#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <thread>
#include <string>
using namespace std;

#ifdef SCM_RIGHTS

/* It seems various versions of glibc headers (i.e.
 * /usr/include/socketbits.h) miss one or more of these */

#ifndef CMSG_DATA
#define CMSG_DATA(cmsg) ((cmsg)->cmsg_data)
#endif

#ifndef CMSG_NXTHDR
#define CMSG_NXTHDR(mhdr, cmsg) __cmsg_nxthdr(mhdr, cmsg)
#endif

#ifndef CMSG_FIRSTHDR
#define CMSG_FIRSTHDR(mhdr)                                 \
  ((size_t)(mhdr)->msg_controllen >= sizeof(struct cmsghdr) \
       ? (struct cmsghdr *)(mhdr)->msg_control              \
       : (struct cmsghdr *)NULL)
#endif

#ifndef CMSG_ALIGN
#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#endif

#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (CMSG_ALIGN(len) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#endif

#ifndef CMSG_LEN
#define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#endif

union fdmsg {
  struct cmsghdr h;
  char buf[CMSG_SPACE(sizeof(int))];
};
#endif

int sendfd(int sock_fd, int send_me_fd)
{
  int ret = 0;
  struct iovec iov[1];
  struct msghdr msg;

  iov[0].iov_base = &ret; /* Don't send any data. Note: der Mouse
				  * <mouse@Rodents.Montreal.QC.CA> says
				  * that might work better if at least one
				  * byte is sent. */
  iov[0].iov_len = 1;

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_name = 0;
  msg.msg_namelen = 0;

  {
#ifdef SCM_RIGHTS
    /* New BSD 4.4 way (ouch, why does this have to be
		 * so convoluted). */

    union fdmsg cmsg;
    struct cmsghdr *h;

    msg.msg_control = cmsg.buf;
    msg.msg_controllen = sizeof(union fdmsg);
    msg.msg_flags = 0;

    h = CMSG_FIRSTHDR(&msg);
    h->cmsg_len = CMSG_LEN(sizeof(int));
    h->cmsg_level = SOL_SOCKET;
    h->cmsg_type = SCM_RIGHTS;
    *((int *)CMSG_DATA(h)) = send_me_fd;
#else
    /* Old BSD 4.3 way. Not tested. */
    msg.msg_accrights = &send_me_fd;
    msg.msg_accrightslen = sizeof(send_me_fd);
#endif

    if (sendmsg(sock_fd, &msg, 0) < 0)
    {
      ret = 0;
    }
    else
    {
      ret = 1;
    }
  }
  /*fprintf(stderr,"send %d %d %d %d\n",sock_fd, send_me_fd, ret, errno);*/
  return ret;
}

int recvfd(int sock_fd)
{
  int count;
  int ret = 0;
  struct iovec iov[1];
  struct msghdr msg;

  iov[0].iov_base = &ret; /* don't receive any data */
  iov[0].iov_len = 1;

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_name = NULL;
  msg.msg_namelen = 0;

  {
#ifdef SCM_RIGHTS
    union fdmsg cmsg;
    struct cmsghdr *h;

    msg.msg_control = cmsg.buf;
    msg.msg_controllen = sizeof(union fdmsg);
    msg.msg_flags = 0;

    h = CMSG_FIRSTHDR(&msg);
    h->cmsg_len = CMSG_LEN(sizeof(int));
    // h->cmsg_level = SOL_SOCKET; /* Linux does not set these */
    h->cmsg_type = SCM_RIGHTS;  /* upon return */
    *((int *)CMSG_DATA(h)) = -1;

    if ((count = recvmsg(sock_fd, &msg, 0)) < 0)
    {
      ret = 0;
    }
    else
    {
      h = CMSG_FIRSTHDR(&msg); /* can realloc? */
      if (h == NULL || h->cmsg_len != CMSG_LEN(sizeof(int)) || h->cmsg_level != SOL_SOCKET || h->cmsg_type != SCM_RIGHTS)
      {
        /* This should really never happen */
        if (h)
          fprintf(stderr,
                  "%s:%d: protocol failure: %d %d %d\n",
                  __FILE__, __LINE__,
                  h->cmsg_len,
                  h->cmsg_level, h->cmsg_type);
        else
          fprintf(stderr,
                  "%s:%d: protocol failure: NULL cmsghdr*\n",
                  __FILE__, __LINE__);
        ret = 0;
      }
      else
      {
        ret = *((int *)CMSG_DATA(h));
        /*fprintf(stderr, "recv ok %d\n", ret);*/
      }
    }
#else
    int receive_fd;
    msg.msg_accrights = &receive_fd;
    msg.msg_accrightslen = sizeof(receive_fd);

    if (recvmsg(sock_fd, &msg, 0) < 0)
    {
      ret = 0;
    }
    else
    {
      ret = receive_fd;
    }
#endif
  }

  /*fprintf(stderr, "recv %d %d %d %d\n",sock_fd, ret, errno, count);*/
  return ret;
}

void process_connection(int newsockfd)
{
  char buffer[256];
  while(true)
  {
    bzero(buffer, 256);
    auto n = read(newsockfd, buffer, 255);
    if (n < 0)
      std::cout << "ERROR reading from socket";
    printf("received: %s\n", buffer);
    send(newsockfd, "received\n", 9, 0);
  }
}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno;
  struct sockaddr_in serv_addr;
  if (argc < 3)
  {
    fprintf(stderr, "usage: socketserver [master, slave] [port]\n");
    exit(1);
  }

  string isMasterString(argv[1]);
  auto isMaster = isMasterString.compare("master");

  if (isMaster == 0)
  {
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
      perror("ERROR opening TCP socket on master");

    // clear address structure
    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[2]);

    /* setup the host_addr structure for use in bind call */
    // server byte order
    serv_addr.sin_family = AF_INET;

    // automatically be filled with current host's IP address
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // convert short integer value for port must be converted into network byte order
    serv_addr.sin_port = htons(portno);

    //
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
      perror("ERROR binding TCP listening socket on master");
  }
  else
  {
    struct sockaddr_un addr;
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0)
      perror("ERROR opening TCP socket on slave");

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/home/kenneth/GitHub/testingground/src/unixsocket");

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
      perror("ERROR binding UNIX listening socket on slave");
      exit(-1);
    }
  }
  
  // This listen() call tells the socket to listen to the incoming connections.
  // The listen() function places all incoming connection into a backlog queue
  // until accept() call accepts the connection.
  // Here, we set the maximum size for the backlog queue to 5.
  listen(sockfd, 5);

  while(true)
  {
    socklen_t clilen;
    struct sockaddr_in cli_addr;
    clilen = sizeof(cli_addr);

    // This accept() function will write the connecting client's address info
    // into the the address structure and the size of that structure is clilen.
    // The accept() returns a new socket file descriptor for the accepted connection.
    // So, the original socket file descriptor can continue to be used
    // for accepting new connections while the new socker file descriptor is used for
    // communicating with the connected client.
    auto newsockfd = isMaster == 0 ? accept(sockfd, (struct sockaddr *)&cli_addr, &clilen) : accept(sockfd, NULL, NULL);
    if (newsockfd < 0)
      perror("ERROR accepting connection");

    if (isMaster == 0)
    {
      struct sockaddr_un addr;
      int unix_fd, rc;
      if ((unix_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
      {
        perror("ERROR creating unix socket on master");
        exit(-1);
      }

      memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      strcpy(addr.sun_path, "/home/kenneth/GitHub/testingground/src/unixsocket");

      if (connect(unix_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
      {
        perror("ERROR connecting to slave on master");
        exit(-1);
      }

      printf("sending tcpsocket %d through unix socket %d\n", newsockfd, unix_fd);
      if (sendfd(unix_fd, newsockfd) == 0)
      {
        perror("ERROR sending newsockfd to slave on master");
      }
    }
    else
    {
      auto newsockfd = recvfd(sockfd);
      if (newsockfd == 0)
      {
        perror("ERROR receiving newsockfd on slave from master");
      }
      else
      {
        thread t(process_connection, newsockfd);
        t.detach();
      }
    }
  }

  printf("About to exit\n");
  close(sockfd);
  return 0;
}