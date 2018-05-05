#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stdint.h>

#include "../map-master/src/map.h"

/* Protocol Versions */
extern char *protocol_versions[];

struct http_response
{
    /** start-line **/
    char *protocol_version;
    char *status;

    /** message body **/
    char *body;

    /** headers **/
    map_str_t header_map;
};

enum parser_state
{
    parser_response_line,
    parser_header_fields,
    parser_empty_line,
    parser_message_body,

    parser_protocol_version,
    parser_status_code,
    parser_ignore_msg,

    parser_done,

    parser_error_response_line,
    parser_error_header_field,
    parser_error_body,

    parser_error_unsupported_state,
    parser_error_unsupported_protocol_version,
    parser_error_unsupported_header_fields,
    parser_error_unsupported_empty_line,
    parser_error_unsupported_message_body,
    parser_error_transfer_encoding_not_supported,
    parser_error_chunk_decode_failed
};

struct http_response_parser
{
    enum parser_state state;
    struct http_response *response;
};

/** parse functions **/
int status_code(struct http_response_parser *p, char *stat);

int protocol_version(struct http_response_parser *p, char *s);

/** initialize parser **/
struct http_response_parser *http_parser_init(void);

/** free header field value from map **/
void free_header_fields_of_map(map_str_t map);

/** free parser **/
void http_parser_free(struct http_response_parser *parser);

/** The normal procedure for parsing an HTTP message is to read the
   start-line into a structure, read each header field into a hash table
   by field name until the empty line, and then use the parsed data to
   determine if a message body is expected.  If a message body has been
   indicated, then it is read as a stream until an amount of octets
   equal to the message body length is read or the connection is closed.  */
int http_parser_parse(struct http_response_parser *parser, FILE *fp);

int http_parser_feed_line(struct http_response_parser *parser, char *line, FILE *fp);

/** feed a request line  */
int http_parser_feed_response_line(struct http_response_parser *parser, char *line);

/** feed header fields by line **/
int http_parser_feed_header_fields(struct http_response_parser *parser, char *line);

/** feed message body by line **/
int http_parser_feed_body(struct http_response_parser *parser, char *line, FILE *fp);

/** decode chunked transfer enconding without trailer part **/
int http_parser_decode_chunked(struct http_response_parser *parser, char *line, FILE *fp);

/** get error message from enum parser_state **/
const char *parse_error(enum parser_state state);

/** print parser information **/
void http_parser_print_information(struct http_response_parser *parser);

#endif
