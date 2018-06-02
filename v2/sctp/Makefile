#
#
#
#

CC = gcc

# CFLAGS = -Wall

all:	sctpclnt

sctpclnt: sctpclnt.o
	$(CC) $(CFLAGS) -o $@ sctpclnt.c handlers.c -L/usr/local/lib -lsctp

clean:
	rm -f sctpclnt sctpsrvr *.o

