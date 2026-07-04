CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lpcap

all: socket_lab

socket_lab: socket_lab.c
	$(CC) $(CFLAGS) socket_lab.c -o socket_lab $(LDFLAGS)

clean:
	rm -f socket_lab
