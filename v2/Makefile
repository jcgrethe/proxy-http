CFLAGS=-pedantic -std=c99 -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wstrict-overflow=5 -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Werror -pedantic-errors -Wmissing-prototypes -D_BSD_SOURCE

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
CFLAGS += -Weverything
CC=clang
endif

ifeq ($(UNAME), Linux)
CC=gcc
endif

PROXY_OUT=proxy

MAIN=main.c
BUFFER=buffer.c
NETUTILS=netutils.c
REQUEST=request.c
SELECTOR=selector.c
SOCKS5=socks5.c
SOCKS5_NIO=socks5nio.c
STM=stm.c

all: $(PROXY_OUT)

$(PROXY_OUT):
	$(CC) -pthread $(MAIN) $(BUFFER) $(NETUTILS) $(REQUEST) $(SELECTOR) $(SOCKS5) $(SOCKS5_NIO) $(STM) -o $(PROXY_OUT)

clean:
	rm proxy
