CC=gcc
CFLAGS=-Wall -Wextra -I.
HOST=34.118.48.238
PORT=8080

client: client.c requests.c helpers.c buffer.c parson.c

run: client
	./client $(HOST) $(PORT)

clean:
	rm -f client
