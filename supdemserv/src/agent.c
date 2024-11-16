#include "agent.h"
#include "shared_memory.h"
#include "commands.h"

void handle_client(int client_socket)
{
  // Infinite loop to handle commands from the client
  while (1)
  {
    char command[256];
    // Read command from client socket

    // Parse command and invoke appropriate handler from commands.c
  }
}
