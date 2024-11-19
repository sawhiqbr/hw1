#include "shared_memory.h"
#include "data_structures.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static shared_data_t *shared_data = NULL;

void init_shared_memory()
{
  // Allocate shared memory
  size_t shm_size = sizeof(shared_data_t);
  shared_data = mmap(
      NULL,
      shm_size,
      PROT_READ | PROT_WRITE,
      MAP_SHARED | MAP_ANONYMOUS,
      -1,
      0);
  if (shared_data == MAP_FAILED)
  {
    perror("initialize shared memory problem");
    exit(EXIT_FAILURE);
  }

  // Initialize shared data and synchronization primitives
  // This code should run only once in the master process before forking
  pthread_mutexattr_t mutexAttr;
  pthread_mutexattr_init(&mutexAttr);
  pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&shared_data->mutex, &mutexAttr);

  // Initialize other fields
  memset(shared_data->demands, 0, sizeof(shared_data->demands));
  memset(shared_data->supplies, 0, sizeof(shared_data->supplies));
  memset(shared_data->watches, 0, sizeof(shared_data->watches));
  shared_data->demand_count = 0;
  shared_data->supply_count = 0;
  shared_data->watch_count = 0;
}

void destroy_shared_memory()
{
  // Destroy mutex
  pthread_mutex_destroy(&shared_data->mutex);

  // Unmap shared memory
  size_t shm_size = sizeof(shared_data_t);
  munmap(shared_data, shm_size);
}

int add_demand(int agent_id, int x, int y, int nA, int nB, int nC)
{
  pthread_mutex_lock(&shared_data->mutex);
  if (shared_data->demand_count >= MAX_DEMANDS)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: exceeded max demands\n");
    return -1;
  }
  demand_t *demand = &shared_data->demands[shared_data->demand_count++];
  demand->agent_id = agent_id;
  demand->x = x;
  demand->y = y;
  demand->nA = nA;
  demand->nB = nB;
  demand->nC = nC;
  shared_data->demand_count++;
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int remove_demand(int agent_id, int demand_id)
{
  pthread_mutex_lock(&shared_data->mutex);
  if (shared_data->demand_count <= 0)
  {
    int debug_demand_count = shared_data->demand_count;
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: there are no demands: %d\n", debug_demand_count);
    return -1;
  }
  shared_data->demands[demand_id].agent_id = 0;
  shared_data->demands[demand_id].x = 0;
  shared_data->demands[demand_id].y = 0;
  shared_data->demands[demand_id].nA = 0;
  shared_data->demands[demand_id].nB = 0;
  shared_data->demands[demand_id].nC = 0;
  shared_data->demand_count--;
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int add_supply(int agent_id, int x, int y, int distance, int nA, int nB, int nC)
{
  pthread_mutex_lock(&shared_data->mutex);
  if (shared_data->supply_count >= MAX_SUPPLIES)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: exceeded max max_supplies\n");
    return -1;
  }
  supply_t *supply = &shared_data->supplies[shared_data->supply_count++];
  supply->agent_id = agent_id;
  supply->x = x;
  supply->y = y;
  supply->distance = distance;
  supply->nA = nA;
  supply->nB = nB;
  supply->nC = nC;
  shared_data->supply_count++;
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int remove_supply(int agent_id, int supply_id)
{
  pthread_mutex_lock(&shared_data->mutex);
  if (shared_data->supply_count <= 0)
  {
    int debug_supply_count = shared_data->supply_count;
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: there are no supplies: %d\n", debug_supply_count);
    return -1;
  }
  shared_data->supplies[supply_id].agent_id = 0;
  shared_data->supplies[supply_id].x = 0;
  shared_data->supplies[supply_id].y = 0;
  shared_data->supplies[supply_id].distance = 0;
  shared_data->supplies[supply_id].nA = 0;
  shared_data->supplies[supply_id].nB = 0;
  shared_data->supplies[supply_id].nC = 0;
  shared_data->supply_count--;
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int add_watch(int agent_id, int x, int y, int distance)
{
  pthread_mutex_lock(&shared_data->mutex);
  if (shared_data->watch_count >= MAX_AGENTS)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: exceeded max watches/agents\n");
    return -1;
  }
  watch_t *watch = &shared_data->supplies[shared_data->watch_count++];
  watch->agent_id = agent_id;
  watch->x = x;
  watch->y = y;
  watch->distance = distance;
  shared_data->watch_count++;
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int remove_watch(int agent_id)
{
  pthread_mutex_lock(&shared_data->mutex);
  if (shared_data->watch_count <= 0)
  {
    int debug_watch_count = shared_data->watch_count;
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: there are no watches: %d\n", debug_watch_count);
    return -1;
  }
  shared_data->watches[agent_id].agent_id = 0;
  shared_data->watches[agent_id].x = 0;
  shared_data->watches[agent_id].y = 0;
  shared_data->watches[agent_id].distance = 0;
  shared_data->watch_count--;
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}