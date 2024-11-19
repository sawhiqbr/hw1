#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "agent.h"
#include "shared_memory.h"

int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    fprintf(stderr, "Usage: %s conn Width Height\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char *conn = argv[1];
  int map_width = atoi(argv[2]);
  int map_height = atoi(argv[3]);

  // Initialize shared memory
  init_shared_memory();

  // Setup listening socket based on conn
  int listen_fd;
  if (conn[0] == '@')
  {
    // Create Unix Domain Socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, conn + 1, sizeof(addr.sun_path) - 1);

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
      perror("unix socket init problem");
      exit(EXIT_FAILURE);
    }
    unlink(addr.sun_path);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1)
    {
      perror("unix socket bind problem");
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    // Create TCP socket
    char *ip_str = strtok(conn, ":");
    char *port_str = strtok(NULL, ":");
    if (!ip_str || !port_str)
    {
      fprintf(stderr, "Invalid conn format. Should be ip:port\n");
      exit(EXIT_FAILURE);
    }
    int port = atoi(port_str);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_str, &addr.sin_addr) <= 0)
    {
      perror("inet_pton error");
      exit(EXIT_FAILURE);
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
      perror("tcp socket init problem");
      exit(EXIT_FAILURE);
    }

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    {
      perror("tcp socket bind problem");
      exit(EXIT_FAILURE);
    }
  }

  if (listen(listen_fd, SOMAXCONN) == -1)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Server listening...\n");

  while (1)
  {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd == -1)
    {
      perror("accept");
      continue;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
      perror("fork");
      close(client_fd);
      continue;
    }
    else if (pid == 0)
    {
      // Child process (agent)
      close(listen_fd);
      agent_process(client_fd);
      exit(EXIT_SUCCESS);
    }
    else
    {
      // Parent process
      close(client_fd);
    }
  }

  // Cleanup shared memory (won't reach here in current code)
  destroy_shared_memory();

  return 0;
}
