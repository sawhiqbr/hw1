#ifndef COMMANDS_H
#define COMMANDS_H

void move_client(int client_id, int x, int y);
void add_demand(int client_id, int nA, int nB, int nC);
void add_supply(int client_id, int distance, int nA, int nB, int nC);
void watch(int client_id, int distance);
void unwatch(int client_id);
void list_demands();
void list_supplies();

#endif // COMMANDS_H
