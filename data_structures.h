#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <pthread.h>
#include <time.h>

#define MAX_DEMANDS 10000
#define MAX_SUPPLIES 10000
#define MAX_AGENTS 1000
#define MAX_NOTIFICATIONS 1000

typedef struct
{
  int agent_id;
  int x;
  int y;
  int nA;
  int nB;
  int nC;
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
} supply_t;

typedef struct
{
  int agent_id;
  int x;
  int y;
  int distance;
} watch_t;

typedef enum
{
  DEMAND_FULFILLED,
  SUPPLY_DELIVERED,
  SUPPLY_REMOVED,
  SUPPLY_ADDED
} notification_type_t;

typedef struct
{
  notification_type_t type;
  int agent_id;
  int demand_id;
  int supply_id;
  int demandX;
  int demandY;
  int demandA;
  int demandB;
  int demandC;
  int supplyX;
  int supplyY;
  int supplyA;
  int supplyB;
  int supplyC;
  int supplyDistance;
  time_t timestamp;
} notification_t;

typedef struct
{
  notification_t notifications[MAX_NOTIFICATIONS];
  int head;
  int tail;
  pthread_mutex_t mutex;
} notification_queue_t;

typedef struct
{
  pthread_mutex_t mutex;
  demand_t demands[MAX_DEMANDS];
  supply_t supplies[MAX_SUPPLIES];
  watch_t watches[MAX_AGENTS];
  int demand_count;
  int supply_count;
  int watch_count;
  pthread_mutex_t agent_mutexes[MAX_AGENTS];
  pthread_cond_t agent_conds[MAX_AGENTS];
  int next_agent_id;
  int agent_positions[MAX_AGENTS][2];
  notification_queue_t notification_queue[MAX_AGENTS];
} shared_data_t;

#endif // DATA_STRUCTURES_H
