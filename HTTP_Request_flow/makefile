CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lpcap

all: socket_lab

socket_lab: HTTP_Request_flow.c
	$(CC) $(CFLAGS) HTTP_Request_flow.c -o socket_lab $(LDFLAGS)

clean:
	rm -f socket_lab
