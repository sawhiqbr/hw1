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
  check_match(agent_id, shared_data->demand_count, 1);
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int remove_demand(int agent_id, int demand_id)
{
  pthread_mutex_lock(&shared_data->mutex);
  int result = remove_demand_nolock(agent_id, demand_id);
  pthread_mutex_unlock(&shared_data->mutex);
  return result;
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
  check_match(agent_id, shared_data->supply_count, 0);
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int remove_supply(int agent_id, int supply_id)
{
  pthread_mutex_lock(&shared_data->mutex);
  int result = remove_supply_nolock(agent_id, supply_id);
  pthread_mutex_unlock(&shared_data->mutex);
  return result;
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
  watch_t *watch = &shared_data->watches[agent_id];
  // only up the watch count if new client is added, don't if update is made
  if (watch->distance == 0)
    shared_data->watch_count++;
  watch->agent_id = agent_id;
  watch->x = x;
  watch->y = y;
  watch->distance = distance;
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

int move(int agent_id, int x, int y)
{
  pthread_mutex_lock(&shared_data->mutex);
  if (agent_id >= MAX_AGENTS)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: there is no such agent: %d\n", agent_id);
    return -1;
  }
  shared_data->watches[agent_id].x = x;
  shared_data->watches[agent_id].y = y;
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int *list_agent_demands(int agent_id)
{
  int *return_list = malloc(sizeof(int));
  pthread_mutex_lock(&shared_data->mutex);
  if (agent_id >= MAX_AGENTS)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: there is no such agent: %d\n", agent_id);
    return NULL;
  }
  int index = 0;
  for (int i = 0; i < shared_data->demand_count; i++)
  {
    if (shared_data->demands[i].agent_id == agent_id)
    {
      return_list[index] = i;
      realloc(return_list, sizeof(int) * (index + 1));
    }
  }
  return return_list;
}

int *list_agent_supplies(int agent_id)
{
  int *return_list = malloc(sizeof(int));
  pthread_mutex_lock(&shared_data->mutex);
  if (agent_id >= MAX_AGENTS)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: there is no such agent: %d\n", agent_id);
    return NULL;
  }
  int index = 0;
  for (int i = 0; i < shared_data->supply_count; i++)
  {
    if (shared_data->supplies[i].agent_id == agent_id)
    {
      return_list[index] = i;
      realloc(return_list, sizeof(int) * (index + 1));
    }
  }
  return return_list;
}

int *list_all_demands()
{
  pthread_mutex_lock(&shared_data->mutex);
  int *return_list = malloc(sizeof(int) * shared_data->demand_count);
  for (int i = 0; i < shared_data->demand_count; i++)
  {
    return_list[i] = i;
  }
  pthread_mutex_unlock(&shared_data->mutex);
  return return_list;
}

int *list_all_supplies()
{
  pthread_mutex_lock(&shared_data->mutex);
  int *return_list = malloc(sizeof(int) * shared_data->supply_count);
  for (int i = 0; i < shared_data->supply_count; i++)
  {
    return_list[i] = i;
  }
  pthread_mutex_unlock(&shared_data->mutex);
  return return_list;
}

int check_match(int agent_id, int demand_or_supply_id, int is_demand)
{
  int had_a_match = 0;
  if (is_demand)
  {
    for (int i = 0; i < shared_data->supply_count; i++)
    {
      if (check_case(demand_or_supply_id, i))
      {
        shared_data->supplies[i].nA -= shared_data->demands[demand_or_supply_id].nA;
        shared_data->supplies[i].nB -= shared_data->demands[demand_or_supply_id].nB;
        shared_data->supplies[i].nC -= shared_data->demands[demand_or_supply_id].nC;
        if (shared_data->supplies[i].nA == 0 && shared_data->supplies[i].nB == 0 && shared_data->supplies[i].nC == 0)
        {
          // TODO: need a solution to double lock problem as we already have the lock
          remove_supply_nolock(shared_data->supplies[i].agent_id, i);
        }
        // TODO: need a solution to double lock problem as we already have the lock
        remove_demand_nolock(agent_id, demand_or_supply_id);

        // TODO: somehow notify both agents
        // notify_agent(agent_id);
        // notify_agent(shared_data->supplies[i].agent_id);
        had_a_match = 1;
        break;
      }
    }
  }
  else
  {
    for (int i = 0; i < shared_data->demand_count; i++)
    {
      if (check_case(i, demand_or_supply_id))
      {
        shared_data->supplies[demand_or_supply_id].nA -= shared_data->demands[i].nA;
        shared_data->supplies[demand_or_supply_id].nB -= shared_data->demands[i].nB;
        shared_data->supplies[demand_or_supply_id].nC -= shared_data->demands[i].nC;
        if (shared_data->supplies[demand_or_supply_id].nA == 0 && shared_data->supplies[demand_or_supply_id].nB == 0 && shared_data->supplies[demand_or_supply_id].nC == 0)
        {
          // TODO: need a solution to double lock problem as we already have the lock
          remove_supply_nolock(shared_data->supplies[demand_or_supply_id].agent_id, demand_or_supply_id);
        }
        // TODO: need a solution to double lock problem as we already have the lock
        remove_demand_nolock(agent_id, i);

        // TODO: somehow notify both agents
        // notify_agent(agent_id);
        // notify_agent(shared_data->supplies[i].agent_id);
        had_a_match = 1;
        break;
      }
    }
  }
  return had_a_match;
}

int check_case(int demand_id, int supply_id)
{
  int bool1 = (shared_data->supplies[supply_id].distance > (abs(shared_data->demands[demand_id].x - shared_data->supplies[supply_id].x) + abs(shared_data->demands[demand_id].y - shared_data->supplies[supply_id].y)));
  int bool2 = shared_data->demands[demand_id].nA < shared_data->supplies[supply_id].nA;
  int bool3 = shared_data->demands[demand_id].nB < shared_data->supplies[supply_id].nB;
  int bool4 = shared_data->demands[demand_id].nC < shared_data->supplies[supply_id].nC;

  return bool1 & bool2 & bool3 & bool4;
}

int remove_demand_nolock(int agent_id, int demand_id)
{
  // Assume mutex is already locked
  if (shared_data->demand_count <= 0)
  {
    printf("Debug: there are no demands\n");
    return -1;
  }
  // Remove demand logic
  shared_data->demands[demand_id].agent_id = 0;
  shared_data->demands[demand_id].x = 0;
  shared_data->demands[demand_id].y = 0;
  shared_data->demands[demand_id].nA = 0;
  shared_data->demands[demand_id].nB = 0;
  shared_data->demands[demand_id].nC = 0;
  shared_data->demand_count--;
  return 0;
}

int remove_supply_nolock(int agent_id, int supply_id)
{
  // Assume mutex is already locked
  if (shared_data->supply_count <= 0)
  {
    printf("Debug: there are no supplies\n");
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
  return 0;
}

void get_agent_position(int agent_id, int *x, int *y)
{
  pthread_mutex_lock(&shared_data->mutex);
  *x = shared_data->watches[agent_id].x;
  *y = shared_data->watches[agent_id].y;
  pthread_mutex_unlock(&shared_data->mutex);
}

void get_demand_t_list(int *demand_ids, int index, demand_t *demand)
{
  demand = &shared_data->demands[demand_ids[index]];
}

void get_supply_t_list(int *supply_ids, int index, supply_t *supply)
{
  supply = &shared_data->supplies[supply_ids[index]];
}