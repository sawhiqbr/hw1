CC = gcc
CFLAGS = -Wall -lpthread -lrt
TARGET = supdemserv

SRC = src/supdemserv.c src/agent.c src/shared_memory.c src/utilities.c src/commands.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJ) $(TARGET)
