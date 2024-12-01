CC = gcc
CFLAGS = -Wall -Wextra -pthread -lrt -g -lpthread

OBJS = supdemserv.o agent.o shared_memory.o

all: supdemserv tester

supdemserv: $(OBJS)
	$(CC) $(CFLAGS) -o supdemserv $(OBJS)

supdemserv.o: supdemserv.c agent.h shared_memory.h data_structures.h

agent.o: agent.c agent.h shared_memory.h data_structures.h

shared_memory.o: shared_memory.c shared_memory.h data_structures.h

tester: tester.o
	$(CC) $(CFLAGS) -o tester tester.c -pthread

tester.o: tester.c

clean:
	rm -f *.o supdemserv tester

.PHONY: clean all