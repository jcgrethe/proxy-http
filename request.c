/**
 * request.c -- parser del request de http
 */
#include <string.h> // memset
#include <arpa/inet.h>
#include <stdlib.h> // strtol
#include <regex.h>
#include <errno.h>
#include <ctype.h> // isdigit, isalpha
#include <stdio.h>

#include "request.h"

//////////////////////////////////////////////////////////////////////////////

static enum request_state
method(const uint8_t c, struct request_parser *p) {
    enum request_state next;

    // If the parser has NULL for method sub_parser, then instantiate it
    // else, use it and control return of feeding a byte.
    if (p->http_sub_parser == NULL) {
        struct parser_definition d;

        switch (c) {
            case 'G':
                d = parser_utils_strcmpi("GET");
                p->request->method = http_method_GET;
                break;
            case 'H':
                d = parser_utils_strcmpi("HEAD");
                p->request->method = http_method_HEAD;
                break;
            case 'P':
                d = parser_utils_strcmpi("POST");
                p->request->method = http_method_POST;
                break;
            default:
                p->state = request_error_unsupported_method;
                break;
        }

        if (p->state != request_error_unsupported_method) {
            p->http_sub_parser = parser_init(parser_no_classes(), &d);
        } else {
            return p->state = next;
        }
    }

    switch (parser_feed(p->http_sub_parser, c)->type) {
        case STRING_CMP_MAYEQ:

            // Method is case-sensitive. Should always be CAPS.
            if (c >= 'a' && c <= 'z') {
                next = request_error_unsupported_method;
                p->request->method = -1;
            } else {
                next = request_method;
            }

            break;
        case STRING_CMP_EQ:

            // Method is case-sensitive. Should always be CAPS.
            if (c >= 'a' && c <= 'z') {
                next = request_error_unsupported_method;
                p->request->method = -1;
            } else {
                parser_utils_strcmpi_destroy(p->http_sub_parser->def);
                parser_destroy(p->http_sub_parser);
                next = request_SP_AFTER_METHOD;
                p->http_sub_parser = NULL;
            }

            break;
        case STRING_CMP_NEQ:
        default:
            // TODO: this can fail / start
            parser_utils_strcmpi_destroy(p->http_sub_parser->def);
            parser_destroy(p->http_sub_parser);
            // TODO: this can fail / end
            next = request_error_unsupported_method;
            p->request->method = -1;
            break;
    }

    return next;
}

// When host is not empty, the proxy must ignore the following Host field.
int retrieveHostAndPort(struct request *req, char *URI_reference) {
    // char * URI_reference = "http://www.ics.uci.edu/pub/ietf/uri/#Related";
    // char * URI_reference2 = "foo://example.com:8042/over/there?name=ferret#nose";
    // char * URI_reference3 = "/over/there";
    char *regexString = "^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?";
    size_t maxMatches = 1;
    size_t maxGroups = 10;
    int ret = 0;

    regex_t regexCompiled;
    regmatch_t groupArray[maxGroups];
    char *cursor;
    if (regcomp(&regexCompiled, regexString, REG_EXTENDED)) {
        return -1;
    };

    cursor = URI_reference;
    for (int i = 0; i < maxMatches; i++) {
        regexec(&regexCompiled, cursor, maxGroups, groupArray, 0);

        unsigned int offset = 0;

        for (int g = 0; g < maxGroups; g++) {
            if (g == 0)
                offset = groupArray[g].rm_eo;

            char cursorCopy[strlen(cursor) + 1];
            strcpy(cursorCopy, cursor);
            cursorCopy[groupArray[g].rm_eo] = 0;

            // Authority is the 4th group
            if (g == 4 && strlen(cursorCopy + groupArray[g].rm_so) != 0) {
                char *authority = strdup(cursorCopy + groupArray[g].rm_so);

                req->host = strsep(&authority, ":");

                char *port = strsep(&authority, "");

                if (port != NULL) {
                    char *endptr;
                    errno = 0;
                    long val = strtol(port, &endptr, 10);
                    if (errno || endptr == port || *endptr != '\0' || val < 0 || val >= 0x10000) {
                        return -1;
                    }
                    req->port = (uint16_t) val;
                }

                free(authority);

                ret = 1;
            }
        }
        cursor += offset;
    }

    regfree(&regexCompiled);
    return ret;
}

//int parseIPV4Address(char *URI_reference) {
//    regex_t regex;
//    int reti;
//    char msgbuf[100];
//    int ret;
//
///* Compile regular expression */
//    reti = regcomp(&regex, "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$", REG_EXTENDED);
//    if (reti) {
//        fprintf(stderr, "Could not compile regex\n");
//        exit(1);
//    }
//
///* Execute regular expression */
//    reti = regexec(&regex, URI_reference, 0, NULL, 0);
//    if (!reti) {
//        ret = 0;
//    }
//    else if (reti == REG_NOMATCH) {
//        ret = 1;
//    }
//    else {
//        regerror(reti, &regex, msgbuf, sizeof(msgbuf));
//        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
//        exit(1);
//    }
//
///* Free memory allocated to the pattern buffer by regcomp() */
//    regfree(&regex);
//    return ret;
//}

// Based on 5.3.2. absolute-form of RFC 7230
// Accumulate request target upto single space.
// Then retrieve host and port.
static enum request_state
target(const uint8_t c, struct request_parser *p) {
    enum request_state next;

    if (c == ' ') {
        // Request-target is over after single space
        ((uint8_t *) &(p->request_target))[MAX_REQUEST_TARGET_SIZE] = '\0';
        p->i++;

        if (retrieveHostAndPort(p->request, p->request_target) == -1) {
//            TODO: si no te lo encontró no me devuelvas error?
            next = request_error;
        } else {
            next = request_HTTP_version;
        }

        memset(p->request_target, '\0', MAX_REQUEST_TARGET_SIZE);
        p->i = 0;
        return next;
    }

    if (p->i == MAX_REQUEST_TARGET_SIZE) {
        memset(p->request_target, '\0', MAX_REQUEST_TARGET_SIZE);
        next = request_error_too_long_request_target;
        p->i = 0;
        return next;
    }

    ((uint8_t *) &(p->request_target))[p->i++] = c;
    next = request_target;
    return next;
}

static enum request_state
HTTP_version(const uint8_t c, struct request_parser *p) {
    enum request_state next;

    // The parser should be NULL after method sub parser destruction.
    if (p->http_sub_parser == NULL) {
        struct parser_definition d = parser_utils_strcmpi("HTTP/1.1");
        p->http_sub_parser = parser_init(parser_no_classes(), &d);
    }

    switch (parser_feed(p->http_sub_parser, c)->type) {
        case STRING_CMP_MAYEQ:
            // HTTP version is case-sensitive. Should always be CAPS.
            if (c >= 'a' && c <= 'z') {
                next = request_error_unsupported_http_version;
            } else {
                next = request_HTTP_version;
            }
            break;
        case STRING_CMP_EQ:
            parser_utils_strcmpi_destroy(p->http_sub_parser->def);
            parser_destroy(p->http_sub_parser);
            next = request_CRLF;
            p->http_sub_parser = NULL;
            break;
        case STRING_CMP_NEQ:
        default:
            // TODO: this can fail / start
            parser_utils_strcmpi_destroy(p->http_sub_parser->def);
            parser_destroy(p->http_sub_parser);
            // TODO: this can fail / end
            next = request_error_unsupported_method;
            break;
    }

    return next;
}

static enum request_state
single_space(const uint8_t c, struct request_parser *p, enum request_state state) {
    enum request_state next;

    if (c != ' ') {
        next = request_error;
    } else {
        if (state == request_SP_AFTER_METHOD) {
            next = request_target;
        } else if (state == request_SP_AFTER_TARGET) {
            next = request_HTTP_version;
        } else {
            next = request_error;
        }
    }

    return next;
}

static enum request_state
CRLF(const uint8_t c, struct request_parser *p) {
    if (c == '\r') {
        return LF_end;
    }
    return request_error;

}

static enum request_state
LFEND(const uint8_t c, struct request_parser *p, buffer *b) {
    if (c == '\n') {
        buffer_write(b, '\n');
        return request_header_field_name;
    }
    return request_error;
}

char *strtolower(char *s, int size) {

    char *d = (char *) calloc(1, size);
    int i = 0;

    for (i = 0; i < size; i++) {

        d[i] = (char) tolower(s[i]);


    }
    d[size - 1] = '\0';
    return d;
}

// HTTP 1.1 requires the Host field.

// For now, it's only parsing Host header field.
static enum request_state
header_field_name(const uint8_t c, struct request_parser *p, buffer *b) {
    enum request_state next;

    // If CRLF is found, headers field are over and this is the Empty Line.
    if (strlen(p->header_field_name) == 0 && c == '\r') {
        return request_empty_line_waiting_for_LF;
    }

    if (c == '\r') {
        p->i = 0;
        memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
        return request_waiting_for_LF;
    }

    //TODO arreglar fix caso feliz
    if (c == ' ') {
        return request_header_field_name;
    }

    if (c == ':') {
        // Header field name is over after :
        ((uint8_t *) &(p->header_field_name))[MAX_HEADER_FIELD_NAME_SIZE] = '\0';
        p->i++;

        char *aux = strtolower(p->header_field_name, p->i);

        if (strcmp(aux, "host") == 0) {
            p->i = 0;
            write_buffer_string(b, p->header_field_name);
            memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);

            // Host was empty in first request line
            if (strlen(p->request->host) == 0) {
                p->i = 0;
                next = request_host_field_value;
            } else {
                next = request_no_empty_host;
            }

        } else if (strcmp(aux, "connection") == 0
            || strcmp(aux, "proxy-authorization") == 0
            || strcmp(aux, "keep-alive") == 0
            || strcmp(aux, "proxy-connection") == 0
            || strcmp(aux, "proxy-authentication") == 0) {
            p->i = 0;
            memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
            next = request_ignore_header;
        } else {
            if(strcmp(aux,"content-length")==0){
                write_buffer_string(b, p->header_field_name);
                memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
                p->i = 0;
                next= request_content_length;


            } else {
                write_buffer_string(b, p->header_field_name);
                memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
                p->i = 0;
                next = request_header_value;
            }
        }

        free(aux);

        return next;
    }

    if ((strchr("!#$%&'*+-.^_`|~", c) == NULL && !isdigit(c) && !isalpha(c))) {
        memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
        next = request_error;
        p->i = 0;
        return next;
    }

    if ((int) p->i == MAX_HEADER_FIELD_NAME_SIZE) {
        //TODO: too long header field specific error?
        memset(p->header_field_name, '\0', sizeof(MAX_HEADER_FIELD_NAME_SIZE));
        next = request_error;
        p->i = 0;
        return next;
    }

    ((uint8_t *) &(p->header_field_name))[p->i++] = c;
    next = request_header_field_name;
    return next;
}

static enum request_state
content_length(const uint8_t c,struct request_parser *p){
    if(c==' ' || c=='\t'){
        if(p->request->content_length==0){
            return request_content_length;
        } else{
           p->request->content_length= atof(p->header_field_name);
           memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
           p->i = 0;
           return request_OWS_after_value;
        }
    }
    if(c=='\r'){
        p->request->content_length= atof(p->header_field_name);
        memset(p->header_field_name, '\0', MAX_HEADER_FIELD_NAME_SIZE);
        p->i = 0;
        return request_waiting_for_LF;
    }
    if(isdigit(c)){
        ((uint8_t *) &(p->header_field_name))[p->i++] = c;
        return request_content_length;
    } else
        return request_error;
}

static enum request_state
header_field_value(const uint8_t c, struct request_parser *p) {
    enum request_state next;
    if (c == '\r') {
        p->i = 0;
        memset(p->header_field_value, '\0', MAX_HEADER_FIELD_VALUE_SIZE);
        return LF_end;
    }
    if ((int) p->i == MAX_HEADER_FIELD_VALUE_SIZE) {
        //TODO: too long header field specific error?
        memset(p->header_field_value, '\0', MAX_HEADER_FIELD_VALUE_SIZE);
        next = request_error;
        p->i = 0;
        return next;
    }
    return request_header_value;
}


static enum request_state
ignore_header(const uint8_t c, struct request_parser *p) {
    enum request_state next;
    if (c == '\r') {
        p->i = 0;
        memset(p->header_field_value, '\0', MAX_HEADER_FIELD_VALUE_SIZE);
        return LF_end;
    }
    if ((int) p->i == MAX_HEADER_FIELD_VALUE_SIZE) {
        //TODO: too long header field specific error?
        memset(p->header_field_value, '\0', MAX_HEADER_FIELD_VALUE_SIZE);
        next = request_error;
        p->i = 0;
        return next;
    }
    return request_ignore_header;
}

static enum request_state
header_host_OWS_after_value(const uint8_t c, struct request_parser *p) {


    if (c == ' ' || c == '\t') {

        return request_OWS_after_value;

    }

    if (c == '\r') {

        return request_waiting_for_LF;

    }

    return request_error;
}

static enum request_state
header_host_field_value(const uint8_t c, struct request_parser *p) {
    enum request_state next;

    if (c == ' ' || c == '\t') {

        if (strlen(p->header_field_value) == 0) {
            return request_host_field_value;
        } else {
            p->i = 0;
            //TODO: remember to free p->request->host
            size_t len = strlen(p->header_field_value);
            memcpy(p->request->host, p->header_field_value, len + 1);
            p->request->host[len] = '\0';
            memset(p->header_field_value, '\0', MAX_HEADER_FIELD_VALUE_SIZE);

            return request_OWS_after_value;
        }

    }

    if (c == '\r') {

        // If host value was empty, error.
        if (strlen(p->header_field_value) != 0) {
            p->i = 0;
            //TODO: remember to free p->request->host
            size_t len = strlen(p->header_field_value);
            memcpy(p->request->host, &p->header_field_value, len + 1);
            p->request->host[len] = '\0';

            memset(p->header_field_value, '\0', MAX_HEADER_FIELD_VALUE_SIZE);

            return request_waiting_for_LF;

        } else {
            //TODO: probablemente p->i = 0; deberia estar en varios lugares más (donde retorne a otro estado)
            p->i = 0;
            return request_error;
        }

    }

    if ((int) p->i == MAX_HEADER_FIELD_VALUE_SIZE) {
        //TODO: too long header field value specific error?
        memset(p->header_field_value, '\0', sizeof(MAX_HEADER_FIELD_VALUE_SIZE));
        next = request_error;
        p->i = 0;
        return next;
    }

    ((uint8_t *) &(p->header_field_value))[p->i++] = c;
    next = request_host_field_value;
    return next;
}

static enum request_state
empty_line(const uint8_t c, struct request_parser *p) {

    if (c == '\n') {

        return request_done;
    }

    return request_error;
}

extern void
request_parser_init(struct request_parser *p) {
    p->state = request_method;
    memset(p->request, 0, sizeof(*(p->request)));

    p->host_field_value_complete = 0;
}

//TODO: check whether case request_some_error is okay
extern enum request_state
request_parser_feed(struct request_parser *p, const uint8_t c, buffer *accum) {
    enum request_state next;

    switch (p->state) {
        case request_method:
            next = method(c, p);
            break;
        case request_target:
            next = target(c, p);
            break;
        case request_HTTP_version:
            next = HTTP_version(c, p);
            break;
        case request_SP_AFTER_METHOD:
            next = single_space(c, p, request_SP_AFTER_METHOD);
            break;
        case request_CRLF:
            next = CRLF(c, p);
            break;
        case request_header_field_name:
            next = header_field_name(c, p, accum);
            break;
        case request_host_field_value:
            next = header_host_field_value(c, p);
            break;
        case request_OWS_after_value:
            next = header_host_OWS_after_value(c, p);
            break;
        case request_waiting_for_LF:
            next = LFEND(c, p, accum);
            break;
        case request_content_length:
            next= content_length(c,p);
            break;
        case request_empty_line_waiting_for_LF:
            next = empty_line(c, p);
            break;
        case request_SP_AFTER_TARGET:
            next = single_space(c, p, request_SP_AFTER_TARGET);
            break;
        case request_header_value:
            next = header_field_value(c, p);
            break;
        case request_no_empty_host:

            if (p->host_field_value_complete == 0) {

                p->value_len = 0;

                p->host_field_value_complete = 1;

                if (buffer_can_write(accum)) {
                    buffer_write(accum, (uint8_t) ':');
                }

                int i = 0;
                while (buffer_can_write(accum) && p->request->host[i] != '\0') {
                    buffer_write(accum, (uint8_t) p->request->host[i++]);
                }

                if (!buffer_can_write(accum)) {
                    next = request_error;
                    break;
                }

            }

            if (c == '\r') {
                next = request_waiting_for_LF;
            } else {
                next = request_no_empty_host;
            }

            p->value_len++;

            if (p->value_len > MAX_HEADER_FIELD_VALUE_SIZE) {
                next = request_error;
            }

            break;
        case LF_end:
            next = LFEND(c, p, accum);
            break;
        case request_ignore_header:
            next = ignore_header(c, p);
            break;
        case request_done:
        case request_error:
        case request_error_unsupported_version:
        case request_error_too_long_request_target:
            next = p->state;
            break;
        default:
            next = request_error;
            break;
    }

    return p->state = next;
}

extern bool
request_is_done(const enum request_state st, bool *errored) {
    if (st >= request_error && errored != 0) {
        *errored = true;
    }
    return st >= request_done;
}

extern enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored, buffer *accum) {
    enum request_state st = p->state;

    while (buffer_can_read(b)) {
        const uint8_t c = buffer_read(b);

        /* save byte to request accum */
//        if (buffer_can_write(accum)) {
//            buffer_write(accum, c);
//        }

        st = request_parser_feed(p, c, accum);

        if (st != request_no_empty_host
            && st != request_header_field_name
            && st != request_ignore_header) {
            if (buffer_can_write(accum)) {
                buffer_write(accum, c);
            } else {
                st = request_error;
            }
        }

        if (request_is_done(st, errored)) {
            goto finally;
        }
    }

//    while (buffer_can_read(b)) {
//        const uint8_t c = buffer_read(b);
//        if (buffer_can_write(accum)) {
//            buffer_write(accum, c);
//        }
//    }
    finally:

    return st;
}

extern void
request_close(struct request_parser *p) {
    // nada que hacer
}


// enum response_status
// cmd_resolve(struct request* request,  struct sockaddr **originaddr,
//             socklen_t *originlen, int *domain) {
//     enum response_status ret = status_general_SOCKS_server_failure;
//
//     *domain                  = AF_INET;
//     struct sockaddr *addr    = 0x00;
//     socklen_t        addrlen = 0;
//
//     switch (request->dest_addr_type) {
//         case socks_req_addrtype_domain: {
//             struct hostent *hp = gethostbyname(request->dest_addr.fqdn);
//             if (hp == 0) {
//                 memset(&request->dest_addr, 0x00,
//                                        sizeof(request->dest_addr));
//                 break;
//             }
//             request->dest_addr.ipv4.sin_family = hp->h_addrtype;
//             memcpy((char *)&request->dest_addr.ipv4.sin_addr,
//                    *hp->h_addr_list, hp->h_length);
//
//         }
//         /* no break */
//         case socks_req_addrtype_ipv4:
//             *domain  = AF_INET;
//             addr    = (struct sockaddr *)&(request->dest_addr.ipv4);
//             addrlen = sizeof(request->dest_addr.ipv4);
//             request->dest_addr.ipv4.sin_port = request->dest_port;
//             break;
//         case socks_req_addrtype_ipv6:
//             *domain  = AF_INET6;
//             addr    = (struct sockaddr *)&(request->dest_addr.ipv6);
//             addrlen = sizeof(request->dest_addr.ipv6);
//             request->dest_addr.ipv6.sin6_port = request->dest_port;
//             break;
//         default:
//             return status_address_type_not_supported;
//     }
//
//     *originaddr = addr;
//     *originlen  = addrlen;
//
//     return ret;
// }

//enum response_status
//errno_to_socks(const int e) {
//    enum response_status ret = status_general_SOCKS_server_failure;
//    switch (e) {
//        case 0:
//            ret = status_succeeded;
//            break;
//        case ECONNREFUSED:
//            ret = status_connection_refused;
//            break;
//        case EHOSTUNREACH:
//            ret = status_host_unreachable;
//            break;
//        case ENETUNREACH:
//            ret = status_network_unreachable;
//            break;
//        case ETIMEDOUT:
//            ret = status_ttl_expired;
//            break;
//    }
//    return ret;
//}
