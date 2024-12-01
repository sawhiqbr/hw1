#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "agent.h"
#include "shared_memory.h"
#include "data_structures.h"
#include <ctype.h>

typedef struct
{
  int client_fd;
  int agent_id;
  // Additional fields if needed
} agent_args_t;

void *command_handler_thread(void *arg);
void *notification_thread(void *arg);
void handle_command(agent_args_t *args, char *command_str);
char *trim_whitespace(char *str);

void agent_process(int client_fd)
{
  pthread_t cmd_thread, notif_thread;
  agent_args_t *args = malloc(sizeof(agent_args_t));
  args->client_fd = client_fd;

  get_next_agent_id(&args->agent_id);

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
  size_t buffer_len = 0; // Current length of data in buffer

  while (1)
  {
    ssize_t bytes_read = read(client_fd, buffer + buffer_len, sizeof(buffer) - buffer_len - 1);
    if (bytes_read <= 0)
    {
      // Client closed connection or error
      break;
    }

    buffer_len += bytes_read;
    buffer[bytes_read] = '\0';

    char *line_start = buffer;
    char *newline_pos;
    while ((newline_pos = strchr(line_start, '\n')) != NULL)
    {
      *newline_pos = '\0'; // Replace newline with null terminator

      // Now line_start points to a complete command string
      handle_command(args, line_start);

      // Move to the next line
      line_start = newline_pos + 1;
    }
    // Move any remaining partial command to the beginning of the buffer
    buffer_len = strlen(line_start);
    memmove(buffer, line_start, buffer_len);
  }

  // Clean up and exit the thread
  close(client_fd);
  return NULL;
}

void *notification_thread(void *arg)
{
  agent_args_t *args = (agent_args_t *)arg;
  int client_fd = args->client_fd;
  int agent_id = args->agent_id;

  // TODO: Implement notification handling
  // Wait for notifications from shared memory and send to client

  while (1)
  {
    notify_agent(agent_id, client_fd);
  }

  return NULL;
}

void handle_command(agent_args_t *args, char *command_str)
{
  int client_fd = args->client_fd;
  int agent_id = args->agent_id;

  char *command = trim_whitespace(command_str);

  if (strncmp(command, "move ", 5) == 0)
  {
    int x, y;
    if (sscanf(command + 5, "%d %d", &x, &y) == 2)
    {
      // Call move function
      if (move(agent_id, x, y) == 0)
      {
        write(client_fd, "OK\n", 3);
      }
      else
      {
        write(client_fd, "Error: Move failed\n", 19);
      }
    }
    else
    {
      write(client_fd, "Error: Invalid move command\n", 28);
    }
  }
  else if (strncmp(command, "demand ", 7) == 0)
  {
    int nA, nB, nC;
    if (sscanf(command + 7, "%d %d %d", &nA, &nB, &nC) == 3)
    {
      // Get agent's current position
      int x, y;
      get_agent_position(agent_id, &x, &y);
      // Call add_demand function
      if (add_demand(agent_id, x, y, nA, nB, nC) == 0)
      {
        write(client_fd, "OK\n", 3);
      }
      else
      {
        write(client_fd, "Error: Add demand failed\n", 25);
      }
    }
    else
    {
      write(client_fd, "Error: Invalid demand command\n", 30);
    }
  }
  else if (strncmp(command, "supply ", 7) == 0)
  {
    int distance, nA, nB, nC;
    if (sscanf(command + 7, "%d %d %d %d", &distance, &nA, &nB, &nC) == 4)
    {
      // Get agent's current position
      int x, y;
      get_agent_position(agent_id, &x, &y);
      // Call add_demand function
      if (add_supply(agent_id, x, y, distance, nA, nB, nC) == 0)
      {
        write(client_fd, "OK\n", 3);
      }
      else
      {
        write(client_fd, "Error: Add supply failed\n", 25);
      }
    }
    else
    {
      write(client_fd, "Error: Invalid supply command\n", 30);
    }
  }
  else if (strncmp(command, "watch ", 6) == 0)
  {
    int distance;
    if (sscanf(command + 6, "%d", &distance) == 1)
    {
      int x, y;
      get_agent_position(agent_id, &x, &y);
      if (add_watch(agent_id, x, y, distance) == 0)
      {
        write(client_fd, "OK\n", 3);
      }
      else
      {
        write(client_fd, "Error: Add watch failed\n", 25);
      }
    }
    else
    {
      write(client_fd, "Error: Invalid watch command\n", 30);
    }
  }
  else if (strcmp(command, "unwatch") == 0)
  {
    if (remove_watch(agent_id) == 0)
    {
      write(client_fd, "OK\n", 3);
    }
    else
    {
      write(client_fd, "Error: Remove watch failed\n", 25);
    }
  }
  else if (strcmp(command, "mydemands") == 0)
  {
    int *demand_ids = list_agent_demands(agent_id);
    if (demand_ids == NULL)
    {
      write(client_fd, "Error: No demands found\n", 24);
    }
    else
    {
      // Send demands to client
      char response[1024];
      response[0] = '\0';
      // int total_demands = ...; // Get the total number of demands
      // snprintf(response, sizeof(response), "There are %d demands in total.\n", total_demands);
      // strcat(response, "X       |Y       |A    |B    |C    |\n");
      // strcat(response, "-------+-------+-----+-----+-----+\n");
      for (int i = 0; demand_ids[i] != -1; i++)
      {
        demand_t *demand;
        get_demand_t_list(demand_ids, i, demand);
        char line[128];
        // snprintf(line, sizeof(line), "%7d|%7d|%5d|%5d|%5d|\n",
        //          demand.x, demand.y, demand.nA, demand.nB, demand.nC);
        snprintf(line, sizeof(line), "%d %d %d %d %d %d\n",
                 demand_ids[i], demand->x, demand->y,
                 demand->nA, demand->nB, demand->nC);
        strcat(response, line);
      }
      write(client_fd, response, strlen(response));
      free(demand_ids);
    }
  }
  else if (strcmp(command, "mysupplies") == 0)
  {
    int *supply_ids = list_agent_supplies(agent_id);
    if (supply_ids == NULL)
    {
      write(client_fd, "Error: No supplies found\n", 24);
    }
    else
    {
      // Send supplies to client
      char response[1024];
      response[0] = '\0';
      for (int i = 0; supply_ids[i] != -1; i++)
      {
        supply_t *supply;
        get_supply_t_list(supply_ids, i, supply);
        char line[128];
        snprintf(line, sizeof(line), "%d %d %d %d %d %d\n",
                 supply_ids[i], supply->x, supply->y,
                 supply->nA, supply->nB, supply->nC);
        strcat(response, line);
      }
      write(client_fd, response, strlen(response));
      free(supply_ids);
    }
  }
  else if (strcmp(command, "listdemands") == 0)
  {
    int *demand_ids = list_all_demands(agent_id);
    if (demand_ids == NULL)
    {
      write(client_fd, "Error: No demands found\n", 24);
    }
    else
    {
      // Send demands to client
      char response[1024];
      response[0] = '\0';
      for (int i = 0; demand_ids[i] != -1; i++)
      {
        demand_t *demand;
        get_demand_t_list(demand_ids, i, demand);
        char line[128];
        snprintf(line, sizeof(line), "%d %d %d %d %d %d\n",
                 demand_ids[i], demand->x, demand->y,
                 demand->nA, demand->nB, demand->nC);
        strcat(response, line);
      }
      write(client_fd, response, strlen(response));
      free(demand_ids);
    }
  }
  else if (strcmp(command, "listsupplies") == 0)
  {
    int *supply_ids = list_all_supplies(agent_id);
    if (supply_ids == NULL)
    {
      write(client_fd, "Error: No supplies found\n", 24);
    }
    else
    {
      // Send supplies to client
      char response[1024];
      response[0] = '\0';
      for (int i = 0; supply_ids[i] != -1; i++)
      {
        supply_t *supply;
        get_supply_t_list(supply_ids, i, supply);
        char line[128];
        snprintf(line, sizeof(line), "%d %d %d %d %d %d\n",
                 supply_ids[i], supply->x, supply->y,
                 supply->nA, supply->nB, supply->nC);
        strcat(response, line);
      }
      write(client_fd, response, strlen(response));
      free(supply_ids);
    }
  }

  else if (strcmp(command, "quit") == 0)
  {
    write(client_fd, "OK\n", 3);
    pthread_exit(NULL);
  }
  else
  {
    write(client_fd, "Error: Unknown command\n", 23);
  }
}

char *trim_whitespace(char *str)
{
  char *end;

  // Trim leading space
  while (isspace((unsigned char)*str))
    str++;

  if (*str == 0) // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;

  // Write new null terminator
  *(end + 1) = '\0';

  return str;
}
