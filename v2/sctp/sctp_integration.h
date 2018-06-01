#include <netinet/in.h>
#include "../selector.h"
#include "../buffer.h"

#define BUFFER_SIZE 1024*1024


void sctp_passive_accept(struct selector_key *key);

struct sctp_data{
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    buffer                        buffer_write, buffer_read;
    uint8_t                       raw_buffer_write[BUFFER_SIZE], raw_buffer_read[BUFFER_SIZE];

    // int                           argc;
    // char **                       cmd;
    // enum helper_errors            error;

    // char *                        user;
};