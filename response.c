/**
 * response.c -- parser del response de http
 */
#include <string.h> // memset
#include <arpa/inet.h>
#include <stdlib.h> // strtol
#include <regex.h>
#include <errno.h>
#include <ctype.h> // isdigit, isalpha
#include <stdio.h>

#include "response.h"

//////////////////////////////////////////////////////////////////////////////
struct parser_definition d;

static enum response_state
HTTP_version(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    // The parser should be NULL after method sub parser destruction.
    if (p->http_sub_parser == NULL) {
        d = parser_utils_strcmpi("HTTP/1.1"); //TODO
        p->http_sub_parser = parser_init(parser_no_classes(), &d);
    }

    switch (parser_feed(p->http_sub_parser, c)->type) {
        case STRING_CMP_MAYEQ:
            // HTTP version is case-sensitive. Should always be CAPS.
            if (c >= 'a' && c <= 'z') {
                next = response_error_unsupported_http_version;
            } else {
                next = response_HTTP_version;
            }
            break;
        case STRING_CMP_EQ:
            parser_utils_strcmpi_destroy(p->http_sub_parser->def);
            parser_destroy(p->http_sub_parser);
            next = response_SP_after_HTTP_version;
            p->http_sub_parser = NULL;
            break;
        case STRING_CMP_NEQ:
        default:
            // TODO: this can fail / start  y si probamos con parser_init??
            parser_utils_strcmpi_destroy(p->http_sub_parser->def);
            parser_destroy(p->http_sub_parser);
            // TODO: this can fail / end
            next = response_error;
            break;
    }

    return next;
}

static enum response_state
SP_after_HTTP_version(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (c != ' ') {
        next = response_error;
    } else {
        next = response_status_code;
    }

    return next;
}

static enum response_state
status_code(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (!isdigit(c)) {
        next = response_error;
    } else {

        p->response->status_code = p->response->status_code * 10;
        p->response->status_code += c - '0';

        p->digits++;

        if (p->digits == 3) {
            next = response_SP_after_status_code;
        } else {
            next = response_status_code;
        }
    }

    return next;
}

static enum response_state
SP_after_status_code(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (c != ' ') {
        next = response_error;
    } else {
        next = response_reason_phrase;
    }

    return next;
}

static enum response_state
reason_phrase(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (c == '\r') {
        next = response_CR_after_reason_phrase;
    } else {
        next = response_reason_phrase;
    }

    return next;
}

static enum response_state
CR_after_reason_phrase(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (c == '\n') {
        next = response_header_field_name;
    } else {
        next = response_error;
    }

    return next;
}

static enum response_state
CR_after_header_value(const uint8_t c, struct response_parser *p) {

    if (c == ' ' || c == '\t') {
        return response_CR_after_header_value;
    }

    if (c == '\r') {
        return response_LF_after_header_value;
    }

    return response_error;
}

static enum response_state
LF_after_header_value(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (c == '\n') {
        return response_header_field_name;
    }

    return response_error;
}

//char *strtolower(char *s, int size) {
//
//    char *d = (char *) calloc(1, size);
//    int i = 0;
//
//    for (i = 0; i < size; i++) {
//
//        d[i] = (char) tolower(s[i]);
//
//
//    }
//    d[size - 1] = '\0';
//    return d;
//}

static enum response_state
parse_transfer_encoding(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (strlen(p->header_field_value) == 0 && c == ' ' || c == '\t') {
        return response_parse_transfer_encoding;
    }

    if (p->http_sub_parser == NULL) {
        d = parser_utils_strcmpi("chunked"); //TODO
        p->http_sub_parser = parser_init(parser_no_classes(), &d);
    }

    switch (parser_feed(p->http_sub_parser, c)->type) {
        case STRING_CMP_MAYEQ:
            next = response_parse_transfer_encoding;
            break;
        case STRING_CMP_EQ:
            parser_utils_strcmpi_destroy(p->http_sub_parser->def);
            parser_destroy(p->http_sub_parser);
            next = response_CR_after_header_value;
            p->http_sub_parser = NULL;
            p->response->transfer_enconding_chunked = true;
            break;
        case STRING_CMP_NEQ:
        default:
            // TODO: this can fail / start  y si probamos con parser_init??
            parser_utils_strcmpi_destroy(p->http_sub_parser->def);
            parser_destroy(p->http_sub_parser);
            // TODO: this can fail / end
            next = response_error_transfer_encoding_not_supported; //TODO 501 (Not Implemented)
            break;
    }

    return next;

}

static enum response_state
LF_after_header_value_content_length(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (c == '\n') {
        return response_header_field_name;
    }

    return response_error;
}

static enum response_state
parse_content_length(const uint8_t c, struct response_parser *p, buffer * accum) {
    enum response_state next;

    if (!p->response->content_length_present && (c == ' ' || c == '\t')) {
        return response_parse_content_length;
    }

    if (p->response->content_length_present && (c == ' ' || c == '\t')) {
        return response_CR_after_header_value;
    }

    if (c == '\r') {
        if (!p->response->content_length_present) {
            return response_error; //todo: MISSING LENGTH
        } else {

            if (p->response->method == http_method_HEAD) {
                write_buffer_string(accum, "Content-Length: ");

                char str[12] = "";
                snprintf(str, sizeof str, "%zu", p->response->content_length);

                write_buffer_string(accum, str);
                write_buffer_string(accum, "\r\n");
            }

            return response_LF_after_header_value_content_length;
        }
    }

    if (!isdigit(c)) {
        return response_error;
    } else {

        p->response->content_length *= 10;
        p->response->content_length += (char) c - '0';

        p->response->content_length_present = true;

        next = response_parse_content_length;
    }

    return next;

}

static enum response_state
parse_content_type(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (strlen(p->content_type_medias) == 0 && (c == ' ' || c == '\t')) {
        return response_parse_content_type;
    }

    if (c == '\r') {
        if (strlen(p->content_type_medias) == 0) {
            return response_error;
        } else {
            p->i = 0;
            return response_LF_after_header_value;
        }
    }

//    if ((strchr("!#$%&'*+-.^_`|~", c) == NULL && !isdigit(c) && !isalpha(c) && c != '/' && c != ';' && c != ' ' && c != '\t')) {
//        memset(p->content_type_medias, '\0', MAX_HEADER_FIELD_NAME_SIZE);
//        next = response_error;
//        p->i = 0;
//        return next;
//    }

    if ((int) p->i == MAX_HEADER_FIELD_NAME_SIZE) {
        memset(p->content_type_medias, '\0', MAX_HEADER_FIELD_NAME_SIZE);
        next = response_error;
        p->i = 0;
        return next;
    }

    ((uint8_t *) &(p->content_type_medias))[p->i++] = c;
    next = response_parse_content_type;
    return next;

}

static enum response_state
header_field_name(const uint8_t c, struct response_parser *p, buffer *accum) {
    enum response_state next;

    // If CRLF is found, headers field are over and this is the Empty Line.
    if (strlen(p->header_field_name) == 0 && c == '\r') {

        if (p->response->method == http_method_HEAD || p->response->method == http_method_GET) {
            write_buffer_string(accum, "Connection: close\r\n");
        }

        buffer_write(accum, '\r');

        return response_empty_line_waiting_for_LF;

    }

    if (c == ':') {
        // Header field name is over after :
        ((uint8_t *) &(p->header_field_name))[MAX_HEADER_FIELD_NAME_SIZE] = '\0';
        p->i++;

        char *aux = strtolower(p->header_field_name, p->i);


        if (strcmp(aux, "content-length") == 0) {
            p->i = 0;
            memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
            next = response_parse_content_length;

            if (p->response->method != http_method_HEAD) {
                write_buffer_string(accum, "Transfer-Encoding: chunked\r\n");
            }


        } else {

            if (strcmp(aux, "transfer-encoding") == 0) {
                next = response_parse_transfer_encoding;

            } else if (strcmp(aux, "content-type") == 0) {
                next = response_parse_content_type;

            } else {
                next = response_header_value;
            }

            write_buffer_string(accum, p->header_field_name);
            buffer_write(accum, ':');
            memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
            p->i = 0;

        }

//        memset(aux, '\0', sizeof(aux));
        free(aux);

        return next;
    }

    if ((strchr("!#$%&'*+-.^_`|~", c) == NULL && !isdigit(c) && !isalpha(c))) {
        memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
        next = response_error;
        p->i = 0;
        return next;
    }

    if ((int) p->i == MAX_HEADER_FIELD_NAME_SIZE) {
        memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
        next = response_error;
        p->i = 0;
        return next;
    }

    ((uint8_t *) &(p->header_field_name))[p->i++] = c;
    next = response_header_field_name;
    return next;
}

static enum response_state
header_field_value(const uint8_t c, struct response_parser *p) {
    enum response_state next;

    if (c == '\r') {
        p->i = 0;
        memset(p->header_field_name, '\0', sizeof(MAX_HEADER_FIELD_NAME_SIZE));
        return response_LF_after_header_value;
    }

    if ((c == ' ' || c == '\t')) {
        return response_header_value;
    }

    if ((int) p->i == MAX_HEADER_FIELD_NAME_SIZE) {
        //TODO: too long header field specific error?
        memset(p->header_field_name, '\0', sizeof(MAX_HEADER_FIELD_NAME_SIZE));
        next = response_error;
        p->i = 0;
        return next;
    }

//    if ((strchr("!#$%&'*+-.^_`|~", c) == NULL && !isdigit(c) && !isalpha(c) && (c < 0x80 || c > 0xff))) {
//        memset(p->header_field_name, '\0', sizeof(MAX_HEADER_FIELD_NAME_SIZE));
//        next = response_error;
//        p->i = 0;
//        return next;
//    }

    p->i++;

    return response_header_value;
}

static enum response_state
empty_line_waiting_for_LF(const uint8_t c, struct response_parser *p) {

    if (c == '\n') {

        return response_done;
    }

    return response_error;
}

extern void
response_parser_init(struct response_parser *p) {
    p->state = response_HTTP_version;
    p->response = calloc(1, (sizeof(*(p->response))));
    //memset(p->response, 0, sizeof(*(p->response)));

//    p->i = 0;
//    p->digits = 0;
//
//    p->response->transfer_enconding_chunked = false;
//    p->response->content_length_present = false;
//    p->response->content_length = 0;

}

extern enum response_state
response_parser_feed(struct response_parser *p, const uint8_t c, buffer *accum) {
    enum response_state next;

    switch (p->state) {
        case response_HTTP_version:
            next = HTTP_version(c, p);
            break;
        case response_SP_after_HTTP_version:
            next = SP_after_HTTP_version(c, p);
            break;
        case response_status_code:
            next = status_code(c, p);
            break;
        case response_SP_after_status_code:
            next = SP_after_status_code(c, p);
            break;
        case response_reason_phrase:
            next = reason_phrase(c, p);
            break;
        case response_CR_after_reason_phrase:
            next = CR_after_reason_phrase(c, p);
            break;
        case response_header_field_name:
            next = header_field_name(c, p, accum);
            break;
        case response_parse_transfer_encoding:
            next = parse_transfer_encoding(c, p);
            break;
        case response_parse_content_length:
            next = parse_content_length(c, p, accum);
            break;
        case response_parse_content_type:
            next = parse_content_type(c, p);
            break;
        case response_header_value:
            next = header_field_value(c, p);
            break;
        case response_CR_after_header_value:
            next = CR_after_header_value(c, p);
            break;
        case response_LF_after_header_value:
            next = LF_after_header_value(c, p);
            break;
        case response_LF_after_header_value_content_length:
            next = LF_after_header_value_content_length(c, p);
            break;
        case response_empty_line_waiting_for_LF:
            next = empty_line_waiting_for_LF(c, p);
            break;
        case response_done:
        case response_error:
        case response_error_unsupported_http_version:
        case response_error_transfer_encoding_not_supported:
        case response_error_chunk_decode_failed:
            next = p->state;
            break;
        default:
            next = response_error;
            break;
    }
    return p->state = next;
}

extern bool
response_is_done(const enum response_state st, bool *errored) {
    if (st >= response_error && errored != 0) {
        *errored = true;
    }
    return st >= response_done;
}

extern enum response_state
response_consume(buffer *b, struct response_parser *p, bool *errored, buffer *accum, ssize_t *bytesRead) {
    enum response_state st = p->state;

    // Return if response headers are already parsed
    if (st == response_done) {
        return st;
    }

    while (buffer_can_read(b)) {
        const uint8_t c = buffer_read(b);

        if (buffer_can_write(accum)) {
            if (st != response_header_field_name
                && st != response_parse_content_length && st != response_LF_after_header_value_content_length)
                buffer_write(accum, c);
        } else {
            st = response_error;
        }

        st = response_parser_feed(p, c, accum);

        // Decrease bytesRead (n from response_read) so we can know the current received body length in response_read
        *bytesRead = *bytesRead - 1;

        if (response_is_done(st, errored)) {
            break;
        }
    }

    return st;
}

extern void
response_close(struct response_parser *p) {
    // nada que hacer
}
