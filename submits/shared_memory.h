#include "data_structures.h"

#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

void init_shared_memory();
void destroy_shared_memory();

// Functions to access and modify shared data structures
int add_demand(int agent_id, int nA, int nB, int nC);
int remove_demand(int agent_id, int demand_id);

int add_supply(int agent_id, int distance, int nA, int nB, int nC);
int remove_supply(int agent_id, int supply_id);

int add_watch(int agent_id, int distance);
int remove_watch(int agent_id);

int move(int agent_id, int x, int y);

int check_match(int agent_id, int demand_or_supply_id, int is_demand);

void get_next_agent_id(int *agent_id);

void notify_client(int agent_id, int client_fd);

int remove_demand_nolock(int agent_id, int demand_id);

int remove_supply_nolock(int agent_id, int demand_id);

int check_case(int demand_id, int supply_id);

void remove_all_demands_nolock(int agent_id);

void remove_all_supplies_nolock(int agent_id);

void cleanup_agent(int agent_id);

char *create_supply_response(int agent_id, int all);

char *create_demand_response(int agent_id, int all);

int find_first_empty_supply();

int find_first_empty_demand();
#endif // SHARED_MEMORY_H
