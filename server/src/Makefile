server: server.c
	gcc -Wall -o server server.c

client: client.c
	gcc -Wall -o client client.c

s: server
	./server -port 21014

c: client
	./client 127.0.0.1 21014

clean:
	rm -f server client