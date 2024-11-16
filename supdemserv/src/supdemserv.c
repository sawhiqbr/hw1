#include "agent.h"
#include "shared_memory.h"
#include "commands.h"

int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    fprintf(stderr, "Usage: %s conn Width Height\n", argv[0]);
    return 1;
  }

  // Parse connection details, width, and height
  const char *conn = argv[1];
  int width = atoi(argv[2]);
  int height = atoi(argv[3]);

  // Initialize shared memory
  init_shared_memory();

  // Set up socket to listen for connections (Unix domain or TCP)

  // Accept and handle connections by forking an agent per connection

  // Clean up shared memory on termination
  return 0;
}
