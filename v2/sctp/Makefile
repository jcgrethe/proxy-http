#
#
#
#

CC = gcc

# CFLAGS = -Wall

all:	sctpclnt sctpsrvr

sctpclnt: sctpclnt.o
	$(CC) $(CFLAGS) -o $@ sctpclnt.c handlers.c -L/usr/local/lib -lsctp

sctpsrvr: sctpsrvr.o
	$(CC) $(CFLAGS) -o $@ sctpsrvr.c handlers.c map.c -L/usr/local/lib -lsctp

clean:
	rm -f sctpclnt sctpsrvr *.o

