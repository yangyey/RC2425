CC     = g++

CFLAGS = -Wall

.PHONY: all clean

all: player DS

player: Client/client.cpp utils.o
	$(CC) $(CFLAGS) -o player Client/client.cpp utils.o

utils.o: utils.cpp constant.hpp
	$(CC) $(CFLAGS) -c -o utils.o utils.cpp

DS: Server/Server.cpp utils.o
	$(CC) $(CFLAGS) -o DS Server/server.cpp utils.o
	
clean:
	rm -f DS player *.o
