CC=gcc
CFLAGS=-Wall -Og
DFLAGS=-g -pthread
OBJS=relayChatClient.o relayChatServer.o
PROGS=relayChatClient relayChatServer

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) $(DFLAGS) $< -o $@

clean:
	rm -f $(PROGS) *.o ~* a.out