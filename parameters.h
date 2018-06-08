#ifndef PARAMETERS
#define PARAMETERS

#include <stdint.h>
#include <netinet/in.h>
#include <stdbool.h>
#include "media_types.h"
#define SCTP_DEFAULT_PORT 9090
#define HTTP_DEFAULT_PORT 8080
#define MAX_PORT 60000
#define MIN_PORT 1024
#define VERSION "0.0.0"
#define ERROR_FILE_DEFAULT "/var/log/pc-2018-01"
#define LOCALHOST "127.0.0.1"
#define LISTENING_ADDRESS "0.0.0.0"
#define DEFAULT_MEDIA_TYPES_RANGES "*/*"

struct options_struct {
    /* Connection Settings */
    uint16_t http_port;
    char * listen_address;
    struct addrinfo * listenadddrinfo;

    uint16_t sctp_port;
    char * management_address;
    struct addrinfo * managementaddrinfo;
    /* Logs */
    char * error_file;

    /* Response settings */
    int transformations;
    char * filter_command;
    char * media_types_input;

    struct media_types * mts;
};

typedef struct options_struct * options;

void parse_options(int argc, const char **argv);

extern options parameters;

#endif