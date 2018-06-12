
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

all: $(PROXY_OUT)

$(PROXY_OUT):
	@echo "[Compiling...]"
	@$(CC) -pthread  $(MAIN) $(BUFFER) $(NETUTILS) $(PARSER) $(PARSER_UTILS) $(REQUEST) $(RESPONSE) $(SELECTOR) $(HTTP) $(HTTP_NIO) $(STM) $(SCTP_HANDLERS) $(PARAMETERS) $(MEDIA_TYPES) -o $(PROXY_OUT)
	@$(CC) -pthread sctp/sctpclnt.c sctp/handlers.c -L/usr/local/lib -lsctp -o sctpclnt
	@echo "[Finished]"

clean:
	@echo "[Removing...]"
	@rm -f httpd sctpclnt
	@echo "[Finished]"
