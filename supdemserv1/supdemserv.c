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
  }
  else
  {
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
