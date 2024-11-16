# Supply-Demand Server Project Starter Pack

This starter pack provides the basic structure and preliminary files to begin developing the Supply-Demand server as per your homework description.

## Project Structure

- `Makefile`: Build instructions for compiling the project.
- `supdemserv.c`: Main server program. Sets up the listening socket and accepts connections.
- `agent.c`, `agent.h`: Handles client communication and processing of commands. Each agent process handles one client.
- `shared_memory.c`, `shared_memory.h`: Manages the shared memory where demands, supplies, and watches are stored.
- `data_structures.h`: Defines the data structures used in shared memory.
- `README.md`: Provides an overview and instructions.

## How to Build

Run `make` in the project directory to compile the `supdemserv` executable.

```bash
make
