CC = gcc
CFLAGS = -Wall -c -std=c99
CFLAGS2 = -Wall -std=c99 -pthread

all: server client other

server: server.o utility.o
	$(CC) $(CFLAGS2) -Wall server.o utility.o -o server

client: client.o utility.o
	$(CC) $(CFLAGS2) client.o utility.o -o client

other: client.o utility.o
	$(CC) $(CFLAGS2) -Wall client.o utility.o -o other

server.o: server.c utility.h
	$(CC) $(CFLAGS) server.c -o server.o -pthread

client.o: client.c utility.h
	$(CC) $(CFLAGS) client.c -o client.o -pthread

utility.o: utility.c utility.h
	$(CC) $(CFLAGS) utility.c -o utility.o -pthread

clean:
	rm *.o server client other
