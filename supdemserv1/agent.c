#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "agent.h"
#include "shared_memory.h"
#include "data_structures.h"

typedef struct
{
  int client_fd;
  int agent_id;
  // Additional fields if needed
} agent_args_t;

void *command_handler_thread(void *arg);
void *notification_thread(void *arg);

void agent_process(int client_fd)
{
  pthread_t cmd_thread, notif_thread;
  agent_args_t *args = malloc(sizeof(agent_args_t));
  args->client_fd = client_fd;
  args->agent_id = getpid(); // Use PID as agent ID for simplicity

  // Register agent in shared memory if needed

  // Create command handler thread
  if (pthread_create(&cmd_thread, NULL, command_handler_thread, args) != 0)
  {
    perror("pthread_create");
    close(client_fd);
    free(args);
    exit(EXIT_FAILURE);
  }

  // Create notification thread
  if (pthread_create(&notif_thread, NULL, notification_thread, args) != 0)
  {
    perror("pthread_create");
    close(client_fd);
    free(args);
    exit(EXIT_FAILURE);
  }

  // Wait for threads to finish
  pthread_join(cmd_thread, NULL);
  pthread_cancel(notif_thread); // Cancel notification thread if command handler exits
  pthread_join(notif_thread, NULL);

  close(client_fd);
  free(args);
}

void *command_handler_thread(void *arg)
{
  agent_args_t *args = (agent_args_t *)arg;
  int client_fd = args->client_fd;
  char buffer[1024];

  while (1)
  {
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0)
    {
      // Client closed connection or error
      break;
    }
    buffer[bytes_read] = '\0';

    // Parse and handle commands
    char command[100];

    if (strcmp(command, "move") == 0)
    {
      // TODO: Implement move logic
      write(client_fd, "OK\n", 3);
    }
    else if (strcmp(command, "supply") == 0)
    {
      // TODO: Implement supply logic
      write(client_fd, "OK\n", 3);
    }
    else if (strcmp(command, "watch") == 0)
    {
      // TODO: Implement watch logic
      write(client_fd, "OK\n", 3);
    }
    else if (strcmp(command, "unwatch") == 0)
    {
      // TODO: Implement unwatch logic
      write(client_fd, "OK\n", 3);
    }
    else if (strcmp(command, "mydemands") == 0)
    {
      // TODO: Implement mydemands logic
    }
    else if (strcmp(command, "mysupplies") == 0)
    {
      // TODO: Implement mysupplies logic
    }
    else if (strcmp(command, "listdemands") == 0)
    {
      // TODO: Implement listdemands logic
    }
    else if (strcmp(command, "listsupplies") == 0)
    {
      // TODO: Implement listsupplies logic
    }
    else if (strcmp(command, "quit") == 0)
    {
      break;
    }
    else
    {
      // Unknown command
      write(client_fd, "Unknown command\n", 16);
    }
  }

  return NULL;
}

void *notification_thread(void *arg)
{
  agent_args_t *args = (agent_args_t *)arg;
  int client_fd = args->client_fd;

  // TODO: Implement notification handling
  // Wait for notifications from shared memory and send to client

  while (1)
  {
    // Example placeholder for notification mechanism
    // You should replace this with actual synchronization and notification logic
    sleep(1); // Placeholder
  }

  return NULL;
}
