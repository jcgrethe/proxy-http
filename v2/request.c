/**
 * request.c -- parser del request de http
 */
#include <string.h> // memset
#include <arpa/inet.h>

#include "request.h"

static void
remaining_set(struct request_parser* p, const int n) {
    p->i = 0;
    p->n = n;
}

static int
remaining_is_done(struct request_parser* p) {
    return p->i >= p->n;
}

//////////////////////////////////////////////////////////////////////////////

static enum request_state
method(const uint8_t c, struct request_parser* p) {
    enum request_state next;

    // If the parser has NULL for method sub_parser, then instantiate it
    // else, use it and control return of feeding a byte.
    if(p->http_sub_parser == NULL) {
        struct parser_definition d;

        switch(c) {
            case 'G':
                d = parser_utils_strcmpi("GET");
                break;
            case 'H':
                d = parser_utils_strcmpi("HEAD");
                break;
            case 'P':
                d = parser_utils_strcmpi("POST");
                break;
            default:
            p->state = request_error_unsupported_method;
            break;
        }

        if(p->state != request_error_unsupported_method) {
            p->http_sub_parser = parser_init(parser_no_classes(), &d);
        } else {
            return p->state = next;
        }
        
    }

    switch (parser_feed(p->http_sub_parser, c)->type) {
        case STRING_CMP_MAYEQ:
            next = request_method;
            break;
        case STRING_CMP_EQ:
            next = request_SP;

            //TODO: check this returns expected (GET, HEAD or POST)
            p->request->method = p->http_sub_parser->state;

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
target(const uint8_t c, struct request_parser* p) {
    enum request_state next;

    switch (parser_feed(p->http_sub_parser, c)->type) {
        case STRING_CMP_MAYEQ:
            next = request_method;
            break;
        case STRING_CMP_EQ:
            next = request_target;

            //TODO: check this returns expected (GET, HEAD or POST)
            p->request->method = p->http_sub_parser->state;

            break;
        case STRING_CMP_NEQ:
        default:
            next = request_error_unsupported_method;
            break;
    }

    return next;
}

static enum request_state
HTTP_version(const uint8_t c, struct request_parser* p) {
    enum request_state next;

    // The parser should be NULL after method sub parser destruction.
    if(p->http_sub_parser == NULL) {
        struct parser_definition d = parser_utils_strcmpi("HTTP/1.1");
        p->http_sub_parser = parser_init(parser_no_classes(), &d);
    }

    switch (parser_feed(p->http_sub_parser, c)->type) {
        case STRING_CMP_MAYEQ:
            next = request_method;
            break;
        case STRING_CMP_EQ:
            next = request_SP;
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
single_space(const uint8_t c, struct request_parser* p) {
    enum request_state next;

    if(c != ' ') {
        next = request_error;
    } else {
        switch(p->state) {
            case request_method:
                next = request_target;
                break;
            case request_target:
                next = request_HTTP_version;
                break;
            case request_HTTP_version:
                next = request_CRLF;
                break;
            default:
                next = request_error;
                break;
        }
    }

    return next;
}

static enum request_state
dstaddr_fqdn(const uint8_t c, struct request_parser* p) {
    remaining_set(p, c);
    p->request->dest_addr.fqdn[p->n - 1] = 0;

    return request_dstaddr;
}

static enum request_state
dstaddr(const uint8_t c, struct request_parser* p) {
    enum request_state next;

    switch (p->request->dest_addr_type) {
        case socks_req_addrtype_ipv4:
            ((uint8_t *)&(p->request->dest_addr.ipv4.sin_addr))[p->i++] = c;
            break;
        case socks_req_addrtype_ipv6:
            ((uint8_t *)&(p->request->dest_addr.ipv6.sin6_addr))[p->i++] = c;
            break;
        case socks_req_addrtype_domain:
            p->request->dest_addr.fqdn[p->i++] = c;
            break;
        }
    if (remaining_is_done(p)) {
        remaining_set(p, 2);
        p->request->dest_port = 0;
        next = request_dstport;
    } else {
        next = request_dstaddr;
    }

    return next;
}

static enum request_state
dstport(const uint8_t c, struct request_parser* p) {
    enum request_state next;
    *(((uint8_t *) &(p->request->dest_port)) + p->i) = c;
    p->i++;
    next = request_dstport;
    if (p->i >= p->n) {
        next = request_done;
    }
    return next;
}

extern void
request_parser_init (struct request_parser* p) {
    p->state = request_method;
    memset(p->request, 0, sizeof(*(p->request)));
}


extern enum request_state 
request_parser_feed (struct request_parser* p, const uint8_t c) {
    enum request_state next;

    switch(p->state) {
        case request_method:
            next = method(c, p);
            break;
        case request_target:
            next = target(c, p);
            break;
        case request_HTTP_version:
            next = HTTP_version(c, p);
            break;
        case request_SP:
            next = single_space(c, p);
            break;
        case request_CRLF:
            // next = atyp(c, p);
            break;
        case request_dstaddr_fqdn:
            next = dstaddr_fqdn(c, p);
            break;
        case  request_dstaddr:
            next = dstaddr(c, p);
            break;
        case request_dstport:
            next = dstport(c, p);
            break;
        case request_done:
        case request_error:
        case request_error_unsupported_version:
        case request_error_unsupported_atyp:
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
    if(st >= request_error && errored != 0) {
        *errored = true;
    }
    return st >= request_done;
}

extern enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored) {
    enum request_state st = p->state;

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       st = request_parser_feed(p, c);
       if(request_is_done(st, errored)) {
          break;
       }
    }
    return st;
}

extern void
request_close(struct request_parser *p) {
    // nada que hacer
}

extern int
request_marshall(buffer *b,
                 const enum socks_response_status status) {
     size_t  n;
    uint8_t *buff = buffer_write_ptr(b, &n);
    if(n < 10) {
        return -1;
    }
    buff[0] = 0x05;
    buff[1] = status;
    buff[2] = 0x00;
    buff[3] = socks_req_addrtype_ipv4;
    buff[4] = 0x00;
    buff[5] = 0x00;
    buff[6] = 0x00;
    buff[7] = 0x00;
    buff[8] = 0x00;
    buff[9] = 0x00;

    buffer_write_adv(b, 10);
    return 10;
}

enum socks_response_status
cmd_resolve(struct request* request,  struct sockaddr **originaddr,
            socklen_t *originlen, int *domain) {
    enum socks_response_status ret = status_general_SOCKS_server_failure;

    *domain                  = AF_INET;
    struct sockaddr *addr    = 0x00;
    socklen_t        addrlen = 0;

    switch (request->dest_addr_type) {
        case socks_req_addrtype_domain: {
            struct hostent *hp = gethostbyname(request->dest_addr.fqdn);
            if (hp == 0) {
                memset(&request->dest_addr, 0x00,
                                       sizeof(request->dest_addr));
                break;
            } 
            request->dest_addr.ipv4.sin_family = hp->h_addrtype;
            memcpy((char *)&request->dest_addr.ipv4.sin_addr,
                   *hp->h_addr_list, hp->h_length);
            
        }
        /* no break */
        case socks_req_addrtype_ipv4:
            *domain  = AF_INET;
            addr    = (struct sockaddr *)&(request->dest_addr.ipv4);
            addrlen = sizeof(request->dest_addr.ipv4);
            request->dest_addr.ipv4.sin_port = request->dest_port;
            break;
        case socks_req_addrtype_ipv6:
            *domain  = AF_INET6;
            addr    = (struct sockaddr *)&(request->dest_addr.ipv6);
            addrlen = sizeof(request->dest_addr.ipv6);
            request->dest_addr.ipv6.sin6_port = request->dest_port;
            break;
        default:
            return status_address_type_not_supported;
    }

    *originaddr = addr;
    *originlen  = addrlen;

    return ret;
}

#include <errno.h>

enum socks_response_status
errno_to_socks(const int e) {
    enum socks_response_status ret = status_general_SOCKS_server_failure;
    switch (e) {
        case 0:
            ret = status_succeeded;
            break;
        case ECONNREFUSED:
            ret = status_connection_refused;
            break;
        case EHOSTUNREACH:
            ret = status_host_unreachable;
            break;
        case ENETUNREACH:
            ret = status_network_unreachable;
            break;
        case ETIMEDOUT:
            ret = status_ttl_expired;
            break;
    }
    return ret;
}
