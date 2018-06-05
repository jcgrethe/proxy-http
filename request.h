#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "buffer.h"
#include "parser_utils.h"

#define MAX_REQUEST_TARGET_SIZE 8000
#define MAX_HEADER_FIELD_NAME_SIZE 256
#define MAX_HEADER_FIELD_VALUE_SIZE 8000 //TODO

enum http_method
{
    http_method_GET,
    http_method_POST,
    http_method_HEAD
};

enum socks_addr_type
{
    socks_req_addrtype_ipv4 = 0x01,
    socks_req_addrtype_domain = 0x03,
    socks_req_addrtype_ipv6 = 0x04,
};

union socks_addr {
    char fqdn[0xff];
    struct sockaddr_in ipv4;
    struct sockaddr_in6 ipv6;
};

struct request
{
    enum http_method method;
    union socks_addr      dest_addr;
    char *host;
    in_port_t port;

    /** port in network byte order */
};

enum request_state
{

    request_method,
    request_target,
    request_SP_AFTER_TARGET,
    request_HTTP_version,
    request_SP_AFTER_METHOD,
    request_CRLF,

    request_header_field_name,
    request_host_field_value,
    request_OWS_after_value,
    request_waiting_for_LF,
    request_empty_line_waiting_for_LF,
    request_no_empty_host,

    request_dstaddr_fqdn,
    request_dstaddr,
    request_dstport,
    request_last_crlf,
    LF_end,
    // apartir de aca están done
    request_done,

    // y apartir de aca son considerado con error
    request_error,
    request_error_unsupported_method,
    request_error_unsupported_http_version,
    request_error_unsupported_version,
    request_error_too_long_request_target,
    request_error_CRLF_not_found,

};

struct request_parser
{
    struct request *request;
    enum request_state state;

    char request_target[MAX_REQUEST_TARGET_SIZE + 1];
    char header_field_name[MAX_HEADER_FIELD_NAME_SIZE + 1];
    char header_field_value[MAX_HEADER_FIELD_VALUE_SIZE + 1];

    /** cuantos bytes ya leimos */
    uint16_t i;

    /** http parser for method, http version **/
    struct parser *http_sub_parser;

    int host_field_value_complete;

    int value_len;
};

enum response_status
{
    status_succeeded,
    status_general_SOCKS_server_failure,
    status_connection_not_allowed_by_ruleset,
    status_network_unreachable,
    status_host_unreachable,
    status_connection_refused,
    status_ttl_expired,
    status_command_not_supported,
    status_address_type_not_supported,
};

/** inicializa el parser */
void request_parser_init(struct request_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum request_state
request_parser_feed(struct request_parser *p, const uint8_t c, buffer *accum);

/**
 * por cada elemento del buffer llama a `request_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored, buffer *pBuffer );

/**
 * Permite distinguir a quien usa socks_hello_parser_feed si debe seguir
 * enviando caracters o no. 
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool request_is_done(const enum request_state st, bool *errored);

void request_close(struct request_parser *p);

/**
 * serializa en buff la una respuesta al request.
 *
 * Retorna la cantidad de bytes ocupados del buffer o -1 si no había
 * espacio suficiente.
 */
extern int
request_marshall(buffer *b,
                 const enum response_status status);

/** convierte a errno en response_status */
enum response_status
errno_to_socks(int e);

#include <netdb.h>
#include <arpa/inet.h>

/** se encarga de la resolcuión de un request */
enum response_status
cmd_resolve(struct request *request, struct sockaddr **originaddr,
            socklen_t *originlen, int *domain);

#endif
