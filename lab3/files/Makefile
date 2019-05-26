CC = gcc -g

server: server.o
	$(CC) server.o -o server

server.o: server.c
	$(CC) -c server.c -o server.o

clean:
	rm server.o