CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpthread

all: server producer consumer

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

producer: producer.c common.h
	$(CC) $(CFLAGS) -o producer producer.c

consumer: consumer.c common.h
	$(CC) $(CFLAGS) -o consumer consumer.c

clean:
	rm -f server producer consumer