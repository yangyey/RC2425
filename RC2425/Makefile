CC     = g++
CFLAGS = -Wall -std=c++11

.PHONY: all clean

# Main targets
all: player GS

# Player executable
player: Client/client.cpp utils.o
	$(CC) $(CFLAGS) -o player Client/client.cpp utils.o

# Server executable
GS: Server/server.cpp utils.o 
	$(CC) $(CFLAGS) -o GS Server/server.cpp utils.o

# Shared utilities
utils.o: utils.cpp utils.hpp constant.hpp
	$(CC) $(CFLAGS) -c utils.cpp -o utils.o

clean:
	rm -f player GS *.o
