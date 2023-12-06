CC=gcc
CFLAGS=-Wall -Og
DFLAGS=-g 
OBJS=relayChatClient.o relayChatServer.o
PROGS=relayChatClient relayChatServer

all: $(PROGS)

%: %.c common.c
	$(CC) $(CFLAGS) $(DFLAGS) $< -o $@

clean:
	rm -f $(PROGS) *.o ~* a.out