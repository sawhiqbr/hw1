#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

void init_shared_memory();
void destroy_shared_memory();

// Functions to access and modify shared data structures
int add_demand(int agent_id, int x, int y, int nA, int nB, int nC);
int remove_demand(int agent_id, int demand_id);

int add_supply(int agent_id, int x, int y, int distance, int nA, int nB, int nC);
int remove_supply(int agent_id, int supply_id);

int add_watch(int agent_id, int x, int y, int distance);
int remove_watch(int agent_id);

int move(int agent_id, int x, int y);

int *list_agent_demands(int agent_id);

int *list_agent_supplies(int agent_id);

int *list_all_demands();

int *list_all_supplies();

int check_match(int agent_id, int demand_or_supply_id, int is_demand);

void get_agent_position(int agent_id, int *x, int *y);

void get_demand_t_list(int *demand_ids, int index, demand_t *demand);

void get_supply_t_list(int *supply_ids, int index, supply_t *supply);
#endif // SHARED_MEMORY_H
