#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include "../selector.h"
#include "../buffer.h"

#define BUFFER_SIZE             1024 * 1024
#define MAX_DATAGRAM_MESSAGE    8 * 120

void sctp_passive_accept(struct selector_key *key);
void change_transformation();

struct datagram_struct{
	uint8_t type;
	uint8_t command;
	uint8_t argsq;
	uint8_t code;
	// Message
	uint8_t message[MAX_DATAGRAM_MESSAGE];
};

struct sctp_data{
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    buffer                        buffer_write, buffer_read;
    uint8_t                       raw_buffer_write[BUFFER_SIZE], raw_buffer_read[BUFFER_SIZE];

    struct datagram_struct		  datagram;

    int							  is_logged;


    // int                           argc;
    // char **                       cmd;
    // enum helper_errors            error;
    // char *                        user;
};

