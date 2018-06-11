CFLAGS = -pedantic -std=c99 -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wstrict-overflow=5 -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Werror -pedantic-errors -Wmissing-prototypes -D_BSD_SOURCE

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
CFLAGS += -Weverything
CC=clang
endif

ifeq ($(UNAME), Linux)
CC=gcc
endif

PROXY_OUT=httpd

MAIN=main.c
BUFFER=buffer.c
NETUTILS=netutils.c
REQUEST=request.c
RESPONSE=response.c
SELECTOR=selector.c
HTTP=http.c
HTTP_NIO=httpnio.c
STM=stm.c
PARSER=parser.c
PARSER_UTILS=parser_utils.c
SCTP_HANDLERS=sctp/sctp_integration.c -lsctp
PARAMETERS=parameters.c
MEDIA_TYPES=media_types.c

FSANITIZE=

FLAGS =

all: $(PROXY_OUT)

$(PROXY_OUT):
	@echo "[Compiling...]"
	@$(CC) -pthread  $(FSANITIZE) $(FLAGS) $(MAIN) $(BUFFER) $(NETUTILS) $(PARSER) $(PARSER_UTILS) $(REQUEST) $(RESPONSE) $(SELECTOR) $(HTTP) $(HTTP_NIO) $(STM) $(SCTP_HANDLERS) $(PARAMETERS) $(MEDIA_TYPES) -o $(PROXY_OUT)
	@$(CC) -pthread sctp/sctpclnt.c sctp/handlers.c -L/usr/local/lib -lsctp -o sctpclnt
	@echo "[Finished]"

clean:
	@echo "[Removing...]"
	@rm -f httpd sctpclnt
	@echo "[Finished]"