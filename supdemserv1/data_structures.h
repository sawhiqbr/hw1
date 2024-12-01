#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <pthread.h>

#define MAX_DEMANDS 10000
#define MAX_SUPPLIES 10000
#define MAX_AGENTS 1000

typedef struct
{
  int agent_id;
  int x;
  int y;
  int nA;
  int nB;
  int nC;
  // Additional fields if needed
} demand_t;

typedef struct
{
  int agent_id;
  int x;
  int y;
  int nA;
  int nB;
  int nC;
  int distance;
  // Additional fields if needed
} supply_t;

typedef struct
{
  int agent_id;
  int x;
  int y;
  int distance;
  // Additional fields if needed
} watch_t;

typedef struct
{
  pthread_mutex_t mutex;
  // Shared data structures
  demand_t demands[MAX_DEMANDS];
  supply_t supplies[MAX_SUPPLIES];
  watch_t watches[MAX_AGENTS];
  // Counts
  int demand_count;
  int supply_count;
  int watch_count;
  pthread_mutex_t agent_mutexes[MAX_AGENTS];
  pthread_cond_t agent_conds[MAX_AGENTS];
  int next_agent_id;
  int agent_positions[MAX_AGENTS][2]; // [x, y] positions

} shared_data_t;

#endif // DATA_STRUCTURES_H
