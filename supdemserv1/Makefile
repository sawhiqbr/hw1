CC = gcc
CFLAGS = -Wall -Wextra -pthread

OBJS = supdemserv.o agent.o shared_memory.o

supdemserv: $(OBJS)
	$(CC) $(CFLAGS) -o supdemserv $(OBJS)

supdemserv.o: supdemserv.c agent.h shared_memory.h data_structures.h

agent.o: agent.c agent.h shared_memory.h data_structures.h

shared_memory.o: shared_memory.c shared_memory.h data_structures.h

tester.o: tester.c
	$(CC) $(CFLAGS) -o tester tester.c -pthread

clean:
	rm -f *.o supdemserv tester

.PHONY: clean
