#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/http_parser.h"

#define BUFFER_SIZE 256
#define CRLF "\r\n"
#define PROTOCOL_LENGTH 8
#define MESSAGE_STATUS_SIZE 3

char *protocol_versions[] = {"HTTP/1.0", "HTTP/1.1"};

static char *concat(const char *s1, const char *s2)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char *result = malloc(len1 + len2 + 1);

    if (result == NULL)
    {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1);
    return result;
}

int status_code(struct http_parser *p, char *stat)
{
    strcpy(p->response->status, stat);
    return 1;
}

int protocol_version(struct http_parser *p, char *s)
{
    int len = sizeof(protocol_versions) / sizeof(protocol_versions[0]);
    int i;
    int sc = 1;
    for (i = 0; i < len && sc; ++i)
    {

        sc = strcmp(protocol_versions[i], s);

        if (sc == 0)
        {
            strcpy(p->response->protocol_version, protocol_versions[i]);
        }
    }

    if (sc != 0)
    {
        printf("%s", s);
        return 0;
    }

    return 1;
}

struct http_parser *http_parser_init()
{
    struct http_parser *parser = malloc(sizeof(struct http_parser));
    if (parser == NULL)
    {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    parser->response = malloc(sizeof(struct http_response));
    if (parser->response == NULL)
    {
        free(parser);
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    parser->response->status = malloc(MESSAGE_STATUS_SIZE * sizeof(char) + 1);
    if (parser->response->status == NULL)
    {
        free(parser->response);
        free(parser);
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    memset(parser->response->status, 0, sizeof(char) * MESSAGE_STATUS_SIZE + 1);

    parser->response->protocol_version = malloc(PROTOCOL_LENGTH * sizeof(char) + 1);
    if (parser->response->protocol_version == NULL)
    {
        free(parser->response->status);
        free(parser->response);
        free(parser);
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    memset(parser->response->protocol_version, 0, sizeof(char) * PROTOCOL_LENGTH + 1);

    parser->response->body = malloc(BUFFER_SIZE * BUFFER_SIZE * sizeof(char));
    if (parser->response->body == NULL)
    {
        free(parser->response->protocol_version);
        free(parser->response->status);
        free(parser->response);
        free(parser);
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    memset(parser->response->body, 0, sizeof(char) * BUFFER_SIZE * BUFFER_SIZE);

    map_init(&parser->response->header_map);

    parser->state = parser_response_line;

    return parser;
}

void free_header_fields_of_map(map_str_t map)
{
    const char *key;
    map_iter_t iter = map_iter(&map);

    while ((key = map_next(&map, &iter)))
    {

        free(*map_get(&map, key));
    }
}

void http_parser_free(struct http_parser *parser)
{

    if (parser != NULL)
    {
        if (parser->response != NULL)
        {
            free(parser->response->status);
            free(parser->response->body);
            free(parser->response->protocol_version);
            free_header_fields_of_map(parser->response->header_map);
            map_deinit_((map_base_t *)&(parser->response->header_map));
            free(parser->response);
        }

        free(parser);
    }
}

int http_parser_parse(struct http_parser *parser, FILE *fp)
{

    char buf[BUFFER_SIZE] = "";
    char *line = buf;
    int error;

    while (fgets(buf, sizeof(buf), fp) != NULL &&
           parser->state != parser_done)
    {

        error = http_parser_feed_line(parser, line, fp);
        if (error == -1)
        {
            fprintf(stderr, "Error: %s\n", parse_error(parser->state));
            return error;
        }
    }

    if (ferror(fp))
    {
        fprintf(stderr, "I/O error when reading.");
    }

    if (fp != stdin)
    {
        fclose(fp);
    }

    return 0;
}

int http_parser_feed_line(struct http_parser *parser, char *line, FILE *fp)
{
    int error;

    switch (parser->state)
    {
    case parser_response_line:
        strtok(line, CRLF);

        parser->state = parser_protocol_version;

        error = http_parser_feed_response_line(parser, line);
        if (error == -1)
        {
            fprintf(stderr, "Error: %s\n", parse_error(parser->state));
            return error;
        }
        else
        {
            parser->state = parser_header_fields;
        }

        break;

    case parser_header_fields:

        line = strtok(line, CRLF);

        /** if empty line with LF only is reached, header fields are done **/
        if (!line)
        {
            parser->state = parser_empty_line;
            break;
        }

        error = http_parser_feed_header_fields(parser, line);
        if (error == -1)
        {
            fprintf(stderr, "Error: %s\n", parse_error(parser->state));
            return error;
        }

        break;

    case parser_empty_line:

        parser->state = parser_message_body;

        break;

    case parser_message_body:

        error = http_parser_feed_body(parser, line, fp);
        if (error == -1)
        {
            fprintf(stderr, "Error: %s\n", parse_error(parser->state));
            return error;
        }
        break;

    case parser_done:
        /** never reached because EOF ends line parsing **/
        break;

    default:
        fprintf(stderr, "unknown state %d\n", parser->state);
        abort();
    }

    return 0;
}

int http_parser_feed_response_line(struct http_parser *parser, char *line)
{

    char buf[BUFFER_SIZE] = "";
    char *delimeter = " ";
    char *token = buf;

    token = strtok(line, delimeter);

    if (token == NULL)
    {
        fprintf(stderr, "Error: could not parse response line.\n");
        parser->state = parser_error_response_line;
        return -1;
    }

    while (token != NULL)
    {

        switch (parser->state)
        {
        case parser_protocol_version:
            if (protocol_version(parser, line))
            {
                parser->state = parser_status_code;
            }
            else
            {
                parser->state = parser_error_unsupported_protocol_version;
                return -1;
            }
            break;

        case parser_status_code:
            if (status_code(parser, token))
            {
                parser->state = parser_ignore_msg;
            }
            else
            {
                parser->state = parser_error_unsupported_state;
                return -1;
            }
            break;

        case parser_ignore_msg:
            while (token != NULL)
            {
                token = strtok(NULL, CRLF);
            }
            parser->state = parser_done;
            break;

        default:
            fprintf(stderr, "Unknown state: %d\n", parser->state);
            abort();
        }
        token = strtok(NULL, delimeter);

        if ((parser->state == parser_done && token != NULL) || (parser->state != parser_done && token == NULL))
        {
            fprintf(stderr, "Error: response line with wrong format\n");
            return -1;
        }
    }

    return 0;
}

int http_parser_feed_header_fields(struct http_parser *parser, char *line)
{

    int error;
    char *field_name;
    char *field_value;
    char *delimeter = ": ";

    field_name = malloc(BUFFER_SIZE);
    if (field_name == NULL)
    {
        perror("Error allocating memory");
        abort();
    }

    field_value = malloc(BUFFER_SIZE);
    if (field_value == NULL)
    {
        perror("Error allocating memory");
        abort();
    }

    memset(field_name, 0, BUFFER_SIZE);
    memset(field_value, 0, BUFFER_SIZE);

    strcpy(field_name, strtok(line, delimeter));

    if (field_name == NULL)
    {
        fprintf(stderr, "Error: could not parse header field name.\n");
        parser->state = parser_error_header_field;
        return -1;
    }

    strcpy(field_value, strtok(NULL, delimeter));

    if (field_value == NULL)
    {
        fprintf(stderr, "Error: could not parse header field value.\n");
        parser->state = parser_error_header_field;
        return -1;
    }

    error = map_set(&parser->response->header_map, field_name, field_value);

    if (error != 0)
    {
        fprintf(stderr, "Error: parsing header field.\n");
        parser->state = parser_error_header_field;
        return -1;
    }

    free(field_name);

    return 0;
}

int http_parser_feed_body(struct http_parser *parser, char *line, FILE *fp)
{

    char **encoding = map_get(&parser->response->header_map, "Transfer-Encoding");

    if (!strcmp(parser->response->protocol_version, protocol_versions[1]) &&
        *encoding != NULL)
    {
        if (!strcmp(*encoding, "chunked"))
        {
            int error = http_parser_decode_chunked(parser, line, fp);
            if (error == -1)
            {
                fprintf(stderr, "Error: %s\n", parse_error(parser->state));
                return error;
            }

            return 0;
        }
        else
        {
            fprintf(stderr, "Error: only 'Transfer-Encoding: chunked' supported.\n");
            parser->state = parser_error_transfer_encoding_not_supported;
            return -1;
        }
    }

    char *old_body = parser->response->body;
    parser->response->body = concat(parser->response->body, line);
    free(old_body);

    if (parser->response->body == NULL)
    {
        fprintf(stderr, "Error: could not parse message body.\n");
        parser->state = parser_error_body;
        return -1;
    }

    return 0;
}

int http_parser_decode_chunked(struct http_parser *parser, char *line, FILE *fp)
{
    size_t chunk_size = 0;

    /** read chunk-size, chunk-ext (if any), and CRLF **/
    chunk_size = (size_t)strtol(line, NULL, 16);

    while (chunk_size > 0)
    {
        char buffer[chunk_size + BUFFER_SIZE];
        memset(buffer, 0, chunk_size + BUFFER_SIZE);

        /** read chunk-data **/
        size_t error = fread(buffer, chunk_size, 1, fp);
        if (error != 1)
        {
            fprintf(stderr, "Error: could not read chunk.\n");
            parser->state = parser_error_chunk_decode_failed;
            return -1;
        }

        /** read CRLF **/
        char readCRLF[1];
        error = fread(readCRLF, 1, 1, fp);
        if (error != 1)
        {
            fprintf(stderr, "Error: could not read CRLF.\n");
            parser->state = parser_error_chunk_decode_failed;
            return -1;
        }

        /** append chunk-data to decoded-body **/
        char *old_body = parser->response->body;
        parser->response->body = concat(parser->response->body, buffer);
        free(old_body);
        if (parser->response->body == NULL)
        {
            fprintf(stderr, "Error: could not parse message body.\n");
            parser->state = parser_error_chunk_decode_failed;
            return -1;
        }

        char new_chunk_size[BUFFER_SIZE] = "";
        fgets(new_chunk_size, sizeof(new_chunk_size), fp);
        strtok(new_chunk_size, CRLF);

        /** read chunk-size, chunk-ext (if any), and CRLF **/
        chunk_size = (size_t)strtol(new_chunk_size, NULL, 16);
    }

    /** read trailer field **/

    /** Content-Length = length **/

    /** Remove "chunked" from Transfer-Encoding **/

    parser->state = parser_done;

    return 0;
}

const char *parse_error(enum parser_state state)
{
    char *error_message;

    switch (state)
    {
    case parser_error_response_line:
        error_message = "error on status line";
        break;
    case parser_error_header_field:
        error_message = "error on header field";
        break;
    case parser_error_body:
        error_message = "error on message body";
        break;
    case parser_error_unsupported_state:
        error_message = "unsupported state";
        break;
    case parser_error_unsupported_protocol_version:
        error_message = "unsupported protocol version";
        break;
    case parser_error_unsupported_header_fields:
        error_message = "unsupported header fields";
        break;
    case parser_error_unsupported_empty_line:
        error_message = "unsupported empty line";
        break;
    case parser_error_unsupported_message_body:
        error_message = "unsupported message body";
        break;
    case parser_error_transfer_encoding_not_supported:
        error_message = "transfer-encoding not supported";
        break;
    case parser_error_chunk_decode_failed:
        error_message = "chunk decode failed";
        break;
    default:
        error_message = "unsupported error message for state";
        break;
    }

    return error_message;
}

void http_parser_print_information(struct http_parser *parser)
{
    printf("%s", parser->response->body);
}
