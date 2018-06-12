#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_RESPONSE_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_RESPONSE_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "buffer.h"
#include "parser_utils.h"
#include "request.h"

#define MAX_HEADER_FIELD_NAME_SIZE 256
#define MAX_HEADER_FIELD_VALUE_SIZE 8000 //TODO

struct response
{
    http_method method;

    int status_code;

    bool transfer_enconding_chunked;

    bool content_length_present;

    bool content_enconding_gzip;

    ssize_t content_length;
};

enum response_state
{

    response_HTTP_version,
    response_SP_after_HTTP_version,

    response_status_code,
    response_SP_after_status_code,

    response_reason_phrase,
    response_CR_after_reason_phrase,

    response_header_field_name,
    response_header_value,
    response_CR_after_header_value,
    response_LF_after_header_value,
    response_LF_after_header_value_content_length,

    //11
    response_empty_line_waiting_for_LF,

    response_parse_transfer_encoding,
    response_parse_content_length,
    response_parse_content_type,
    response_parse_content_encoding,

//    response_body,

//    // chunked
//    response_chunk,
//    response_last_chunk,
//    response_trailer_part,
//    response_chunk_CRLF,


    // apartir de aca están done
    response_done,

    // y apartir de aca son considerado con error
    response_error,
    response_error_unsupported_http_version,

    response_error_transfer_encoding_not_supported,
    response_error_chunk_decode_failed

};

struct response_parser
{
    struct response *response;
    enum response_state state;

    char header_field_name[MAX_HEADER_FIELD_NAME_SIZE + 1];
    char header_field_value[MAX_HEADER_FIELD_VALUE_SIZE + 1];

    /** media types del Content-type header **/
    char content_type_medias[MAX_HEADER_FIELD_VALUE_SIZE + 1];

    /** cuantos digitos del status code ya leimos */
    int digits;

    /** cuantos bytes ya leimos */
    uint16_t i;

    /** http parser for http version **/
    struct parser *http_sub_parser;

};

enum response_status
{
    status_succeeded,
    status_general_HTTP_server_failure,
    status_command_not_supported,
    status_address_type_not_supported,
};

/** inicializa el parser */
void response_parser_init(struct response_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum response_state
response_parser_feed(struct response_parser *p, const uint8_t c, buffer *wb);

/**
 * por cada elemento del buffer llama a `response_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum response_state
response_consume(buffer *b, struct response_parser *p, bool *errored, buffer *wb, ssize_t * bytesRead);

/**
 * Permite distinguir a quien usa response_parser_feed si debe seguir
 * enviando caracters o no. 
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool response_is_done(const enum response_state st, bool *errored);

//static void response_close(struct response_parser *p, struct selector_key *key);
//void response_close(struct response_parser *p);

///**
// * serializa en buff la una respuesta al response.
// *
// * Retorna la cantidad de bytes ocupados del buffer o -1 si no había
// * espacio suficiente.
// */
//extern int
//response_marshall(buffer *b,
//                 const enum response_status status);

/** convierte a errno en response_status */
enum response_status
errno_to_socks(int e);

#include <netdb.h>
#include <arpa/inet.h>

/** se encarga de la resolcuión de un response */
enum response_status
cmd_resolve(struct response *response, struct sockaddr **originaddr,
            socklen_t *originlen, int *domain);

#endif
