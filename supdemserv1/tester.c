#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 4096

volatile int running = 1;

void usage(const char *prog_name)
{
  fprintf(stderr, "Usage: %s @path\n", prog_name);
  fprintf(stderr, "   or: %s ip port\n", prog_name);
}

int connect_unix_domain_socket(const char *path)
{
  int sockfd;
  struct sockaddr_un addr;

  if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
  {
    perror("socket");
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
  {
    perror("connect");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

int connect_tcp_socket(const char *ip, int port)
{
  int sockfd;
  struct sockaddr_in addr;

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("socket");
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
  {
    perror("inet_pton");
    close(sockfd);
    return -1;
  }

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
  {
    perror("connect");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

ssize_t send_command(int sockfd, const char *command)
{
  size_t len = strlen(command);
  ssize_t total_sent = 0;
  while (total_sent < len)
  {
    ssize_t sent = write(sockfd, command + total_sent, len - total_sent);
    if (sent <= 0)
    {
      perror("write");
      return -1;
    }
    total_sent += sent;
  }
  return total_sent;
}

void *receiver_thread(void *arg)
{
  int sockfd = *(int *)arg;
  char buffer[BUFFER_SIZE];
  while (running)
  {
    ssize_t n = read(sockfd, buffer, sizeof(buffer) - 1);
    if (n > 0)
    {
      buffer[n] = '\0';
      printf("Received: %s", buffer);
    }
    else if (n == 0)
    {
      printf("Server closed the connection\n");
      running = 0;
      break;
    }
    else
    {
      if (errno == EINTR)
      {
        continue;
      }
      perror("read");
      running = 0;
      break;
    }
  }
  return NULL;
}

void run_tests(int sockfd)
{
  pthread_t recv_thread;
  if (pthread_create(&recv_thread, NULL, receiver_thread, &sockfd) != 0)
  {
    perror("pthread_create");
    return;
  }

  // Allow receiver thread to start
  sleep(1);

  // Test commands

  printf("Testing 'move 10 20'\n");
  send_command(sockfd, "move 10 20\n");
  sleep(1);

  printf("Testing 'demand 5 3 2'\n");
  send_command(sockfd, "demand 5 3 2\n");
  sleep(1);

  printf("Testing 'supply 15 10 8 6'\n");
  send_command(sockfd, "supply 15 10 8 6\n");
  sleep(1);

  printf("Testing 'watch 10'\n");
  send_command(sockfd, "watch 10\n");
  sleep(1);

  printf("Testing 'listdemands'\n");
  send_command(sockfd, "listdemands\n");
  sleep(1);

  printf("Testing 'listsupplies'\n");
  send_command(sockfd, "listsupplies\n");
  sleep(1);

  printf("Testing 'unwatch'\n");
  send_command(sockfd, "unwatch\n");
  sleep(1);

  printf("Testing 'quit'\n");
  send_command(sockfd, "quit\n");

  // Wait for the receiver thread to finish
  running = 0;
  pthread_join(recv_thread, NULL);
}

int main(int argc, char *argv[])
{
  int sockfd = -1;

  if (argc != 2 && argc != 3)
  {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if (argv[1][0] == '@')
  {
    // Unix domain socket
    const char *path = argv[1] + 1; // Skip '@'
    printf("Connecting to Unix domain socket at '%s'\n", path);
    sockfd = connect_unix_domain_socket(path);
  }
  else
  {
    // TCP socket
    if (argc != 3)
    {
      usage(argv[0]);
      exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    printf("Connecting to TCP socket at %s:%d\n", ip, port);
    sockfd = connect_tcp_socket(ip, port);
  }

  if (sockfd == -1)
  {
    fprintf(stderr, "Failed to connect to the server\n");
    exit(EXIT_FAILURE);
  }

  run_tests(sockfd);

  // Close the connection
  close(sockfd);

  return 0;
}