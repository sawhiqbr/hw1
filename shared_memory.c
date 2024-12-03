#define _GNU_SOURCE
#include "shared_memory.h"
#include "data_structures.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

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
  pthread_mutexattr_t mutexAttr;
  pthread_mutexattr_init(&mutexAttr);
  pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&shared_data->mutex, &mutexAttr);

  // Initialize other fields
  for (int i = 0; i < MAX_DEMANDS; i++)
  {
    shared_data->demands[i].agent_id = -1;
  }
  for (int i = 0; i < MAX_SUPPLIES; i++)
  {
    shared_data->supplies[i].agent_id = -1;
  }
  for (int i = 0; i < MAX_AGENTS; i++)
  {
    shared_data->watches[i].agent_id = -1;
  }
  memset(shared_data->demands, 0, sizeof(shared_data->demands));
  memset(shared_data->supplies, 0, sizeof(shared_data->supplies));
  memset(shared_data->watches, 0, sizeof(shared_data->watches));
  memset(shared_data->agent_mutexes, 0, sizeof(shared_data->agent_mutexes));
  memset(shared_data->agent_conds, 0, sizeof(shared_data->agent_conds));
  shared_data->demand_count = 0;
  shared_data->supply_count = 0;
  shared_data->watch_count = 0;
  shared_data->next_agent_id = 0;

  for (int i = 0; i < MAX_AGENTS; i++)
  {
    pthread_mutexattr_t agent_mutexAttr;
    pthread_mutexattr_init(&agent_mutexAttr);
    pthread_mutexattr_setpshared(&agent_mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_data->agent_mutexes[i], &agent_mutexAttr);
    pthread_mutexattr_destroy(&agent_mutexAttr);

    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shared_data->agent_conds[i], &condAttr);
    pthread_condattr_destroy(&condAttr);

    shared_data->notification_queue[i].head = 0;
    shared_data->notification_queue[i].tail = 0;
    pthread_mutexattr_t notification_mutexAttr;
    pthread_mutexattr_init(&notification_mutexAttr);
    pthread_mutexattr_setpshared(&notification_mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_data->notification_queue[i].mutex, &notification_mutexAttr);
    pthread_mutexattr_destroy(&notification_mutexAttr);
  }
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
  printf("DEBUG: Adding demand for agent %d at (%d,%d) with resources [%d,%d,%d]\n",
         agent_id, x, y, nA, nB, nC);
  pthread_mutex_lock(&shared_data->mutex);
  if (shared_data->demand_count >= MAX_DEMANDS)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: exceeded max demands\n");
    return -1;
  }
  demand_t *demand = &shared_data->demands[shared_data->demand_count];
  demand->agent_id = agent_id;
  demand->x = x;
  demand->y = y;
  demand->nA = nA;
  demand->nB = nB;
  demand->nC = nC;
  check_match(agent_id, shared_data->demand_count, 1);
  shared_data->demand_count++;
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
  printf("DEBUG: Adding supply for agent %d at (%d,%d) with distance %d and resources [%d,%d,%d]\n",
         agent_id, x, y, distance, nA, nB, nC);

  pthread_mutex_lock(&shared_data->mutex);
  if (shared_data->supply_count >= MAX_SUPPLIES)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: exceeded max max_supplies\n");
    return -1;
  }
  supply_t *supply = &shared_data->supplies[shared_data->supply_count];
  supply->agent_id = agent_id;
  supply->x = x;
  supply->y = y;
  supply->distance = distance;
  supply->nA = nA;
  supply->nB = nB;
  supply->nC = nC;
  check_match(agent_id, shared_data->supply_count, 0);
  shared_data->supply_count++;

  for (int i = 0; i < MAX_AGENTS; i++)
  {
    watch_t *watch = &shared_data->watches[i];
    if (watch->distance > 0 && watch->agent_id != -1 && watch->agent_id != agent_id)
    { // Check if the agent is watching
      int dx = abs(watch->x - x);
      int dy = abs(watch->y - y);
      int manhattan_distance = dx + dy;
      printf("DEBUG: Checking watch for agent %d - watch distance: %d, manhattan distance: %d\n",
             watch->agent_id, watch->distance, manhattan_distance);

      if (manhattan_distance <= watch->distance)
      {
        // Prepare notification
        notification_t notif;
        notif.type = SUPPLY_ADDED;
        notif.agent_id = i;
        notif.supplyX = x;
        notif.supplyY = y;
        notif.supplyA = nA;
        notif.supplyB = nB;
        notif.supplyC = nC;
        notif.supplyDistance = distance;
        notif.supply_id = shared_data->supply_count - 1; // Index of the newly added supply
        notif.timestamp = time(NULL);

        // Add notification to agent's queue
        pthread_mutex_lock(&shared_data->notification_queue[i].mutex);
        notification_queue_t *queue = &shared_data->notification_queue[i];
        queue->notifications[queue->tail] = notif;
        queue->tail = (queue->tail + 1) % MAX_NOTIFICATIONS;
        pthread_mutex_unlock(&shared_data->notification_queue[i].mutex);

        // Notify the agent
        pthread_mutex_lock(&shared_data->agent_mutexes[i]);
        pthread_cond_signal(&shared_data->agent_conds[i]);
        pthread_mutex_unlock(&shared_data->agent_mutexes[i]);
      }
    }
  }
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
  shared_data->watches[agent_id].agent_id = -1;
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
  shared_data->agent_positions[agent_id][0] = x;
  shared_data->agent_positions[agent_id][1] = y;
  pthread_mutex_unlock(&shared_data->mutex);
  return 0;
}

int *list_agent_demands(int agent_id)
{
  pthread_mutex_lock(&shared_data->mutex);

  if (agent_id >= MAX_AGENTS)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: there is no such agent: %d\n", agent_id);
    return NULL;
  }

  // Count matching demands first
  int count = 0;
  for (int i = 0; i < shared_data->demand_count; i++)
  {
    if (shared_data->demands[i].agent_id == agent_id)
    {
      count++;
    }
  }

  // Handle empty case
  if (count == 0)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    return NULL;
  }

  // Allocate space for demands plus sentinel value
  int *return_list = malloc(sizeof(int) * (count + 1));
  if (return_list == NULL)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    return NULL;
  }

  // Copy matching demand indices
  int index = 0;
  for (int i = 0; i < shared_data->demand_count; i++)
  {
    if (shared_data->demands[i].agent_id == agent_id)
    {
      return_list[index++] = i;
    }
  }

  // Add sentinel value
  return_list[count] = -1;

  pthread_mutex_unlock(&shared_data->mutex);
  return return_list;
}

int *list_agent_supplies(int agent_id)
{
  pthread_mutex_lock(&shared_data->mutex);

  if (agent_id >= MAX_AGENTS)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    printf("Debug: there is no such agent: %d\n", agent_id);
    return NULL;
  }

  // Count matching supplies first
  int count = 0;
  for (int i = 0; i < shared_data->supply_count; i++)
  {
    if (shared_data->supplies[i].agent_id == agent_id)
    {
      count++;
    }
  }

  // Handle empty case
  if (count == 0)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    return NULL;
  }

  // Allocate space for supplies plus sentinel value
  int *return_list = malloc(sizeof(int) * (count + 1));
  if (return_list == NULL)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    return NULL;
  }

  // Copy matching supply indices
  int index = 0;
  for (int i = 0; i < shared_data->supply_count; i++)
  {
    if (shared_data->supplies[i].agent_id == agent_id)
    {
      return_list[index++] = i;
    }
  }

  // Add sentinel value
  return_list[count] = -1;

  pthread_mutex_unlock(&shared_data->mutex);
  return return_list;
}

int *list_all_demands()
{
  pthread_mutex_lock(&shared_data->mutex);

  // Handle empty case
  if (shared_data->demand_count == 0)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    return NULL;
  }

  // Allocate space for demands plus sentinel value
  int *return_list = malloc(sizeof(int) * (shared_data->demand_count + 1));
  if (return_list == NULL)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    return NULL;
  }

  // Copy all demand indices
  for (int i = 0, index = 0; i < shared_data->demand_count; i++)
  {
    if (shared_data->demands[i].agent_id != -1)
    {
      return_list[index++] = i;
    }
  }

  // Add sentinel value
  return_list[shared_data->demand_count] = -1;

  pthread_mutex_unlock(&shared_data->mutex);
  return return_list;
}

int *list_all_supplies()
{
  pthread_mutex_lock(&shared_data->mutex);

  // Handle empty case
  if (shared_data->supply_count == 0)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    return NULL;
  }

  // Allocate space for supplies plus sentinel value
  int *return_list = malloc(sizeof(int) * (shared_data->supply_count + 1));
  if (return_list == NULL)
  {
    pthread_mutex_unlock(&shared_data->mutex);
    return NULL;
  }

  // Copy all supply indices
  for (int i = 0, index = 0; i < shared_data->supply_count; i++)
  {
    if (shared_data->supplies[i].agent_id != -1)
    {
      return_list[index++] = i;
    }
  }

  // Add sentinel value
  return_list[shared_data->supply_count] = -1;

  pthread_mutex_unlock(&shared_data->mutex);
  return return_list;
}

int check_match(int agent_id, int demand_or_supply_id, int is_demand)
{
  printf("DEBUG: Checking matches for %s ID %d from agent %d\n",
         is_demand ? "demand" : "supply", demand_or_supply_id, agent_id);
  int had_a_match = 0;
  int supply_removed = 0;
  int i_index = 0;
  int supplier_agent_id = -1;
  int demander_agent_id = -1;
  int demandX = 0;
  int demandY = 0;
  int demandA = 0;
  int demandB = 0;
  int demandC = 0;
  int supplyX = 0;
  int supplyY = 0;
  int supplyA = 0;
  int supplyB = 0;
  int supplyC = 0;
  int supplyDistance = 0;
  if (is_demand)
  {
    for (int i = 0; i < shared_data->supply_count; i++)
    {
      if (check_case(demand_or_supply_id, i))
      {
        shared_data->supplies[i].nA -= shared_data->demands[demand_or_supply_id].nA;
        shared_data->supplies[i].nB -= shared_data->demands[demand_or_supply_id].nB;
        shared_data->supplies[i].nC -= shared_data->demands[demand_or_supply_id].nC;
        supplier_agent_id = shared_data->supplies[i].agent_id;
        demander_agent_id = agent_id;
        supplyX = shared_data->supplies[i].x;
        supplyY = shared_data->supplies[i].y;
        supplyA = shared_data->supplies[i].nA;
        supplyB = shared_data->supplies[i].nB;
        supplyC = shared_data->supplies[i].nC;
        supplyDistance = shared_data->supplies[i].distance;
        demandX = shared_data->demands[demand_or_supply_id].x;
        demandY = shared_data->demands[demand_or_supply_id].y;
        demandA = shared_data->demands[demand_or_supply_id].nA;
        demandB = shared_data->demands[demand_or_supply_id].nB;
        demandC = shared_data->demands[demand_or_supply_id].nC;
        if (shared_data->supplies[i].nA - shared_data->demands[demand_or_supply_id].nA == 0 && shared_data->supplies[i].nB - shared_data->demands[demand_or_supply_id].nB == 0 && shared_data->supplies[i].nC - shared_data->demands[demand_or_supply_id].nC == 0)
        {
          remove_supply_nolock(shared_data->supplies[i].agent_id, i);
          supply_removed = 1;
        }
        remove_demand_nolock(agent_id, demand_or_supply_id);

        i_index = i;
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
        supplier_agent_id = agent_id;
        demander_agent_id = shared_data->demands[i].agent_id;
        supplyX = shared_data->supplies[demand_or_supply_id].x;
        supplyY = shared_data->supplies[demand_or_supply_id].y;
        supplyA = shared_data->supplies[demand_or_supply_id].nA;
        supplyB = shared_data->supplies[demand_or_supply_id].nB;
        supplyC = shared_data->supplies[demand_or_supply_id].nC;
        supplyDistance = shared_data->supplies[demand_or_supply_id].distance;
        demandX = shared_data->demands[i].x;
        demandY = shared_data->demands[i].y;
        demandA = shared_data->demands[i].nA;
        demandB = shared_data->demands[i].nB;
        demandC = shared_data->demands[i].nC;
        if (shared_data->supplies[demand_or_supply_id].nA - shared_data->demands[i].nA == 0 && shared_data->supplies[demand_or_supply_id].nB - shared_data->demands[i].nB == 0 && shared_data->supplies[demand_or_supply_id].nC - shared_data->demands[i].nC == 0)
        {
          remove_supply_nolock(shared_data->supplies[demand_or_supply_id].agent_id, demand_or_supply_id);
          supply_removed = 1;
        }
        remove_demand_nolock(agent_id, i);
        i_index = i;
        had_a_match = 1;
        break;
      }
    }
  }

  if (had_a_match)
  {
    printf("DEBUG: Found match! Supplier agent: %d, Demander agent: %d, was it demand: %d\n",
           supplier_agent_id, demander_agent_id, is_demand);

    // Prepare notification
    notification_t notif_sup;
    notif_sup.type = SUPPLY_DELIVERED;
    notif_sup.supply_id = is_demand ? demand_or_supply_id : i_index;
    notif_sup.demand_id = is_demand ? i_index : demand_or_supply_id;
    notif_sup.supplyX = supplyX;
    notif_sup.supplyY = supplyY;
    notif_sup.supplyA = supplyA;
    notif_sup.supplyB = supplyB;
    notif_sup.supplyC = supplyC;
    notif_sup.supplyDistance = supplyDistance;
    notif_sup.demandX = demandX;
    notif_sup.demandY = demandY;
    notif_sup.demandA = demandA;
    notif_sup.demandB = demandB;
    notif_sup.demandC = demandC;
    notif_sup.timestamp = time(NULL);
    notif_sup.agent_id = supplier_agent_id;
    // Notify the supplier
    pthread_mutex_lock(&shared_data->notification_queue[supplier_agent_id].mutex);
    notification_queue_t *supplier_queue = &shared_data->notification_queue[supplier_agent_id];
    supplier_queue->notifications[supplier_queue->tail] = notif_sup;
    supplier_queue->tail = (supplier_queue->tail + 1) % MAX_NOTIFICATIONS;
    pthread_mutex_unlock(&shared_data->notification_queue[supplier_agent_id].mutex);

    pthread_mutex_lock(&shared_data->agent_mutexes[supplier_agent_id]);
    pthread_cond_signal(&shared_data->agent_conds[supplier_agent_id]);
    pthread_mutex_unlock(&shared_data->agent_mutexes[supplier_agent_id]);

    notification_t notif_dem;
    notif_dem.type = DEMAND_FULFILLED;
    notif_dem.demand_id = is_demand ? demand_or_supply_id : i_index;
    notif_dem.supply_id = is_demand ? i_index : demand_or_supply_id;
    notif_dem.supplyX = supplyX;
    notif_dem.supplyY = supplyY;
    notif_dem.supplyA = supplyA;
    notif_dem.supplyB = supplyB;
    notif_dem.supplyC = supplyC;
    notif_dem.supplyDistance = supplyDistance;
    notif_dem.demandX = demandX;
    notif_dem.demandY = demandY;
    notif_dem.demandA = demandA;
    notif_dem.demandB = demandB;
    notif_dem.demandC = demandC;
    notif_dem.timestamp = time(NULL);
    notif_dem.agent_id = demander_agent_id;

    // Notify the demander
    pthread_mutex_lock(&shared_data->notification_queue[demander_agent_id].mutex);
    notification_queue_t *demander_queue = &shared_data->notification_queue[demander_agent_id];
    demander_queue->notifications[demander_queue->tail] = notif_dem;
    demander_queue->tail = (demander_queue->tail + 1) % MAX_NOTIFICATIONS;
    pthread_mutex_unlock(&shared_data->notification_queue[demander_agent_id].mutex);

    pthread_mutex_lock(&shared_data->agent_mutexes[demander_agent_id]);
    pthread_cond_signal(&shared_data->agent_conds[demander_agent_id]);
    pthread_mutex_unlock(&shared_data->agent_mutexes[demander_agent_id]);

    if (supply_removed)
    {
      printf("DEBUG: Supply removed! Supplier agent: %d, Supply ID: %d\n", supplier_agent_id, is_demand ? demand_or_supply_id : i_index);
      notification_t notif_rem;
      notif_rem.type = SUPPLY_REMOVED;
      notif_rem.supply_id = is_demand ? demand_or_supply_id : i_index;
      notif_rem.agent_id = supplier_agent_id;
      notif_rem.timestamp = time(NULL);

      // Notify the supplier
      pthread_mutex_lock(&shared_data->notification_queue[supplier_agent_id].mutex);
      notification_queue_t *supplier_queue = &shared_data->notification_queue[supplier_agent_id];
      supplier_queue->notifications[supplier_queue->tail] = notif_rem;
      supplier_queue->tail = (supplier_queue->tail + 1) % MAX_NOTIFICATIONS;
      pthread_mutex_unlock(&shared_data->notification_queue[supplier_agent_id].mutex);

      pthread_mutex_lock(&shared_data->agent_mutexes[supplier_agent_id]);
      pthread_cond_signal(&shared_data->agent_conds[supplier_agent_id]);
      pthread_mutex_unlock(&shared_data->agent_mutexes[supplier_agent_id]);
    }
  }
  return had_a_match;
}

int check_case(int demand_id, int supply_id)
{
  printf("DEBUG: Checking compatibility between demand %d and supply %d\n", demand_id, supply_id);

  int bool1 = (shared_data->supplies[supply_id].distance > (abs(shared_data->demands[demand_id].x - shared_data->supplies[supply_id].x) + abs(shared_data->demands[demand_id].y - shared_data->supplies[supply_id].y)));
  int bool2 = shared_data->demands[demand_id].nA <= shared_data->supplies[supply_id].nA;
  int bool3 = shared_data->demands[demand_id].nB <= shared_data->supplies[supply_id].nB;
  int bool4 = shared_data->demands[demand_id].nC <= shared_data->supplies[supply_id].nC;

  printf("DEBUG: Distance check: %d, Resource checks: [%d,%d,%d]\n", bool1, bool2, bool3, bool4);

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
  shared_data->demands[demand_id].agent_id = -1;
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
  shared_data->supplies[supply_id].agent_id = -1;
  shared_data->supplies[supply_id].x = 0;
  shared_data->supplies[supply_id].y = 0;
  shared_data->supplies[supply_id].distance = 0;
  shared_data->supplies[supply_id].nA = 0;
  shared_data->supplies[supply_id].nB = 0;
  shared_data->supplies[supply_id].nC = 0;
  shared_data->supply_count--;

  // After removing the supply
  notification_t notif;
  notif.type = SUPPLY_REMOVED;
  notif.supply_id = supply_id;
  notif.agent_id = agent_id;
  notif.timestamp = time(NULL);

  // Add notification to agent's queue
  int supplier_agent_id = agent_id; // or use shared_data->supplies[supply_id].agent_id
  pthread_mutex_lock(&shared_data->notification_queue[supplier_agent_id].mutex);
  notification_queue_t *queue = &shared_data->notification_queue[supplier_agent_id];
  queue->notifications[queue->tail] = notif;
  queue->tail = (queue->tail + 1) % MAX_NOTIFICATIONS;
  pthread_mutex_unlock(&shared_data->notification_queue[supplier_agent_id].mutex);

  // Notify the agent
  pthread_mutex_lock(&shared_data->agent_mutexes[supplier_agent_id]);
  pthread_cond_signal(&shared_data->agent_conds[supplier_agent_id]);
  pthread_mutex_unlock(&shared_data->agent_mutexes[supplier_agent_id]);

  return 0;
}

void get_agent_position(int agent_id, int *x, int *y)
{
  pthread_mutex_lock(&shared_data->mutex);
  *x = shared_data->watches[agent_id].x;
  *y = shared_data->watches[agent_id].y;
  *x = shared_data->agent_positions[agent_id][0];
  *y = shared_data->agent_positions[agent_id][1];
  pthread_mutex_unlock(&shared_data->mutex);
}

void get_demand_t_list(int *demand_ids, int index, demand_t *demand)
{
  pthread_mutex_lock(&shared_data->mutex);
  *demand = shared_data->demands[demand_ids[index]];
  pthread_mutex_unlock(&shared_data->mutex);
}

void get_supply_t_list(int *supply_ids, int index, supply_t *supply)
{
  pthread_mutex_lock(&shared_data->mutex);
  *supply = shared_data->supplies[supply_ids[index]];
  pthread_mutex_unlock(&shared_data->mutex);
}

void notify_client(int agent_id, int client_fd)
{
  printf("DEBUG: Notifying client (agent %d) on fd %d\n", agent_id, client_fd);

  // Wait for notification
  pthread_mutex_lock(&shared_data->agent_mutexes[agent_id]);
  pthread_cond_wait(&shared_data->agent_conds[agent_id], &shared_data->agent_mutexes[agent_id]);
  pthread_mutex_unlock(&shared_data->agent_mutexes[agent_id]);

  // Retrieve notifications from the agent's queue
  notification_queue_t *queue = &shared_data->notification_queue[agent_id];
  pthread_mutex_lock(&queue->mutex);

  while (queue->head != queue->tail)
  {
    notification_t notif = queue->notifications[queue->head];
    queue->head = (queue->head + 1) % MAX_NOTIFICATIONS;
    pthread_mutex_unlock(&queue->mutex);

    printf("DEBUG: Processing notification type %d for agent %d\n", notif.type, agent_id);

    // Process the notification
    char message[256];
    if (notif.type == DEMAND_FULFILLED)
    {
      snprintf(message, sizeof(message),
               "Your demand at (%d,%d), [%d,%d,%d] is fulfilled by a client at (%d,%d).\n",
               notif.demandX, notif.demandY, notif.demandA, notif.demandB, notif.demandC,
               notif.supplyX, notif.supplyY);
    }
    else if (notif.type == SUPPLY_DELIVERED)
    {
      snprintf(message, sizeof(message),
               "Your supply at (%d,%d), [%d,%d,%d] with distance %d is delivered to a client at (%d,%d) [%d,%d,%d].\n",
               notif.supplyX, notif.supplyY, notif.supplyA, notif.supplyB, notif.supplyC, notif.supplyDistance,
               notif.demandX, notif.demandY, notif.demandA, notif.demandB, notif.demandC);
    }
    else if (notif.type == SUPPLY_REMOVED)
    {
      snprintf(message, sizeof(message), "Your supply is removed from map.\n");
    }
    else if (notif.type == SUPPLY_ADDED)
    {
      snprintf(message, sizeof(message),
               "A supply [%d,%d,%d] is inserted at (%d,%d).\n",
               notif.supplyA, notif.supplyB, notif.supplyC, notif.supplyX, notif.supplyY);
    }
    // Send the message to the client
    write(client_fd, message, strlen(message));

    pthread_mutex_lock(&queue->mutex);
  }
  pthread_mutex_unlock(&queue->mutex);
}

void get_next_agent_id(int *agent_id)
{
  pthread_mutex_lock(&shared_data->mutex);
  *agent_id = shared_data->next_agent_id++;
  pthread_mutex_unlock(&shared_data->mutex);
}

void remove_all_demands_nolock(int agent_id)
{
  for (int i = 0; i < shared_data->demand_count; i++)
  {
    if (shared_data->demands[i].agent_id == agent_id)
    {
      shared_data->demand_count--;
      shared_data->demands[i].agent_id = -1;
      shared_data->demands[i].x = 0;
      shared_data->demands[i].y = 0;
      shared_data->demands[i].nA = 0;
      shared_data->demands[i].nB = 0;
      shared_data->demands[i].nC = 0;
    }
  }
}

void remove_all_supplies_nolock(int agent_id)
{
  for (int i = 0; i < shared_data->supply_count; i++)
  {
    if (shared_data->supplies[i].agent_id == agent_id)
    {
      shared_data->supply_count--;
      shared_data->supplies[i].agent_id = -1;
      shared_data->supplies[i].x = 0;
      shared_data->supplies[i].y = 0;
      shared_data->supplies[i].distance = 0;
      shared_data->supplies[i].nA = 0;
      shared_data->supplies[i].nB = 0;
      shared_data->supplies[i].nC = 0;
    }
  }
}

void cleanup_agent(int agent_id)
{
  pthread_mutex_lock(&shared_data->mutex);
  shared_data->watches[agent_id].agent_id = -1;
  shared_data->watches[agent_id].x = 0;
  shared_data->watches[agent_id].y = 0;
  shared_data->watches[agent_id].distance = 0;

  remove_all_demands_nolock(agent_id);
  remove_all_supplies_nolock(agent_id);

  pthread_mutex_unlock(&shared_data->mutex);
  printf("DEBUG: Agent %d cleanup complete\n", agent_id);
}