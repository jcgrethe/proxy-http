/**
 * httpnio.c  - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes)
 */
#include <stdio.h>
#include <stdlib.h> // malloc
#include <string.h> // memset
#include <assert.h> // assert
#include <errno.h>
#include <time.h>
#include <unistd.h> // close
#include <pthread.h>

#include <arpa/inet.h>

#include "request.h"
#include "buffer.h"

#include "stm.h"
#include "httpnio.h"
#include "netutils.h"
#include "sctp/metrics_struct.h"
#include "http_codes.h"

#define N(x) (sizeof(x) / sizeof((x)[0]))

#define BUFFER_SIZE 1024 * 1024
static int n=0;
/** maquina de estados general */
enum http_state {

    /**
     * recibe el mensaje `request` del cliente, y lo inicia su proceso
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - REQUEST_READ        mientras el mensaje no esté completo
     *   - REQUEST_RESOLV      si requiere resolver un nombre DNS
     *   - REQUEST_CONNECTING  si no require resolver DNS, y podemos iniciar
     *                         la conexión al origin server.
     *   - REQUEST_WRITE       si determinamos que el mensaje no lo podemos
     *                         procesar (ej: no soportamos un comando)
     *   - ERROR               ante cualquier error (IO/parseo)
     */
            REQUEST_READ,

    /**
     * Espera la resolución DNS
     *
     * Intereses:
     *     - OP_NOOP sobre client_fd. Espera un evento de que la tarea bloqueante
     *               terminó.
     * Transiciones:
     *     - REQUEST_CONNECTING si se logra resolución al nombre y se puede
     *                          iniciar la conexión al origin server.
     *     - REQUEST_WRITE      en otro caso
     */
            REQUEST_RESOLV,

    /**
     * Espera que se establezca la conexión al origin server
     *
     * Intereses:
     *    - OP_WRITE sobre client_fd
     *
     * Transiciones:
     *    - REQUEST_WRITE    se haya logrado o no establecer la conexión.
     *
     */
            REQUEST_CONNECTING,

    /**
     * envía la respuesta del `request' al cliente.
     *
     * Intereses:
     *   - OP_WRITE sobre client_fd
     *   - OP_NOOP  sobre origin_fd
     *
     * Transiciones:
     *   - COPY         si el request fue exitoso y tenemos que copiar el
     *                  contenido de los descriptores
     *   - ERROR        ante I/O error
     */
            REQUEST_WRITE,
    /**
     * Copia bytes entre client_fd y origin_fd.
     *
     * Intereses: (tanto para client_fd y origin_fd)
     *   - OP_READ  si hay espacio para escribir en el buffer de lectura
     *   - OP_WRITE si hay bytes para leer en el buffer de escritura
     *
     * Transicion:
     *   - DONE     cuando no queda nada mas por copiar.
     */
            COPY,

    // estados terminales
            DONE,
    ERROR,
};

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado

/** usado por REQUEST_READ, REQUEST_WRITE, REQUEST_RESOLV */
struct request_st {
    /** buffer utilizado para I/O */
    buffer *rb, *wb;

    /** parser */
    struct request request;
    struct request_parser parser;

    /** el resumen del respuesta a enviar*/
    enum response_status status;

    // ¿a donde nos tenemos que conectar?
    struct sockaddr_storage *origin_addr;
    socklen_t *origin_addr_len;
    int *origin_domain;

    const int *client_fd;
    int *origin_fd;

    /* buffer que acumula bytes del request */
    uint8_t *raw_buff_accum;
//    uint8_t raw_buff_accum[1024*1024];
    buffer accum;
};

/** usado por REQUEST_CONNECTING */
struct connecting {
    buffer *wb;
    const int *client_fd;
    int *origin_fd;
    enum response_status *status;
};

/** usado por COPY */
struct copy {
    /** el otro file descriptor */
    int *fd;
    /** el buffer que se utiliza para hacer la copia */
    buffer *rb, *wb;
    /** ¿cerramos ya la escritura o la lectura? */
    fd_interest duplex;

    struct copy *other;
};

/*
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una única
 * alocación cuando recibimos la conexión.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 */
struct http {
    /** información del cliente */
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    int client_fd;

    /** resolución de la dirección del origin server */
    struct addrinfo *origin_resolution;
    /** intento actual de la dirección del origin server */
    struct addrinfo *origin_resolution_current;

    /** información del origin server */
    struct sockaddr_storage origin_addr;
    socklen_t origin_addr_len;
    int origin_domain;
    int origin_fd;

    /** maquinas de estados */
    struct state_machine stm;

    /** estados para el client_fd */
    union {
        struct request_st request;
        struct copy copy;
    } client;
    /** estados para el origin_fd */
    union {
        struct connecting conn;
        struct copy copy;
    } orig;

    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[BUFFER_SIZE], raw_buff_b[BUFFER_SIZE];
    buffer read_buffer, write_buffer;

    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;

    /** siguiente en el pool */
    struct http *next;
};

/**
 * Pool de `struct http', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */

static const unsigned max_pool = 50; // tamaño máximo
static unsigned pool_size = 0;       // tamaño actual
static struct http *pool = 0;        // pool propiamente dicho

static const struct state_definition *
http_describe_states(void);

/** crea un nuevo `struct http' */
static struct http *
http_new(int client_fd) {
    struct http *ret;

    if (pool == NULL) {
        ret = malloc(sizeof(*ret));
    } else {
        ret = pool;
        pool = pool->next;
        ret->next = 0;
    }
    if (ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->origin_fd = -1;
    ret->client_fd = client_fd;
    ret->client_addr_len = sizeof(ret->client_addr);

    ret->stm.initial = REQUEST_READ;
    ret->stm.max_state = ERROR;
    ret->stm.states = http_describe_states();
    stm_init(&ret->stm);

    buffer_init(&ret->read_buffer, N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

    ret->references = 1;
    finally:
    return ret;
}

/** realmente destruye */
static void
http_destroy_(struct http *s) {
    if (s->origin_resolution != NULL) {
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }
    free(s);
}

/**
 * destruye un  `struct http', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
http_destroy(struct http *s) {
    if (s == NULL) {
        // nada para hacer
    } else if (s->references == 1) {
        if (s != NULL) {
            if (pool_size < max_pool) {
                s->next = pool;
                pool = s;
                pool_size++;
            } else {
                http_destroy_(s);
            }
        }
    } else {
        s->references -= 1;
    }
}

void http_pool_destroy(void) {
    struct http *next, *s;
    for (s = pool; s != NULL; s = next) {
        next = s->next;
        free(s);
    }
}

/** obtiene el struct (http *) desde la llave de selección  */
#define ATTACHMENT(key) ((struct http *)(key)->data)

/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
static void http_read(struct selector_key *key);

static void http_write(struct selector_key *key);

static void http_block(struct selector_key *key);

static void http_close(struct selector_key *key);

static const struct fd_handler http_handler = {
        .handle_read = http_read,
        .handle_write = http_write,
        .handle_close = http_close,
        .handle_block = http_block,
};

/** Intenta aceptar la nueva conexión entrante*/
void http_passive_accept(struct selector_key *key) {
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct http *state = NULL;

    const int client = accept(key->fd, (struct sockaddr *) &client_addr,
                              &client_addr_len);
    if (client == -1) {
        goto fail;
    }
    if (selector_fd_set_nio(client) == -1) {
        goto fail;
    }

    /* Metrics Start. */
    metrstr->currcon += 1;
    metrstr->histacc += 1;
    /* Metrics End  . */

    state = http_new(client);
    if (state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

    if (SELECTOR_SUCCESS != selector_register(key->s, client, &http_handler,
                                              OP_READ, state)) {
        goto fail;
    }
    return;
    fail:
    if (client != -1) {
        close(client);
    }
    http_destroy(state);
}

////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables de los estados REQUEST_… */
static void
request_init(const unsigned state, struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;

    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);
    d->parser.request = &d->request;
    d->status = status_general_SOCKS_server_failure;
    request_parser_init(&d->parser);
    d->client_fd = &ATTACHMENT(key)->client_fd;
    d->origin_fd = &ATTACHMENT(key)->origin_fd;

    d->origin_addr = &ATTACHMENT(key)->origin_addr;
    d->origin_addr_len = &ATTACHMENT(key)->origin_addr_len;
    d->origin_domain = &ATTACHMENT(key)->origin_domain;

    d->request.port = (uint16_t) 80;

    d->raw_buff_accum = calloc(1, 1024 * 1024);
    buffer_init(&d->accum, 1024 * 1024, d->raw_buff_accum);
    d->request.host = calloc(1, MAX_HEADER_FIELD_VALUE_SIZE); //todo: FREE
}

static unsigned
request_process(struct selector_key *key, struct request_st *d);

/** lee todos los bytes del mensaje de tipo `request' y inicia su proceso */
static unsigned
request_read(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;

    buffer *b = d->rb;
    unsigned ret = REQUEST_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);
    if (n > 0) {
        buffer_write_adv(b, n);
        int st = request_consume(b, &d->parser, &error, &d->accum);
        if (request_is_done(st, 0)) {
            d->parser.state=request_header_field_name;
            request_consume(b, &d->parser, &error, &d->accum);
            ret = request_process(key, d);
        }
    } else {
        ret = ERROR;
    }


    // TODO: vuela error en rquest_consume?
    //return error ? ERROR : ret;
    return ret;
}

static unsigned
request_connect(struct selector_key *key, struct request_st *d);

static void *
request_resolv_blocking(void *data);

/**
 * Procesa el mensaje de tipo `request'.
 * Únicamente soportamos el comando cmd_connect.
 *
 * Si tenemos la dirección IP intentamos establecer la conexión.
 *
 * Si tenemos que resolver el nombre (operación bloqueante) disparamos
 * la resolución en un thread que luego notificará al selector que ha terminado.
 *
 */
static unsigned
request_process(struct selector_key *key, struct request_st *d) {
    unsigned ret;
    pthread_t tid;
    struct selector_key* k = malloc(sizeof(*key));
    struct http *s = ATTACHMENT(key);
    switch (d->request.method) {

        case http_method_POST:
        case http_method_HEAD:
        case http_method_GET:
                if(k == NULL) {
                    ret       = REQUEST_WRITE;
                    d->status = status_general_SOCKS_server_failure;
                    selector_set_interest_key(key, OP_WRITE);
                } else {
                    memcpy(k, key, sizeof(*k));
                    if(-1 == pthread_create(&tid, 0,
                                            request_resolv_blocking, k)) {
                        ret       = REQUEST_WRITE;
                        d->status = status_general_SOCKS_server_failure;
                        selector_set_interest_key(key, OP_WRITE);
                    } else{
                        ret = REQUEST_RESOLV;
                        selector_set_interest_key(key, OP_NOOP);
                    }
                }
            break;


        default:
            d->status = status_command_not_supported;
            write_buffer_string(d->wb,HTTP_CODE_501);
            selector_status s = 0;
            s |= selector_set_interest(key->s, *d->client_fd, OP_WRITE);
            ret = SELECTOR_SUCCESS == s ? REQUEST_WRITE : ERROR;

            break;
    }

    return ret;
}

/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
static void *
request_resolv_blocking(void *data) {
    struct selector_key *key = (struct selector_key *) data;
    struct http       *s   = ATTACHMENT(key);
    pthread_detach(pthread_self());
    s->origin_resolution = 0;
    s->origin_resolution = 0;
    struct addrinfo hints = {
            .ai_family    = AF_INET,    /* Allow IPv4*/
            .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
            .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
            .ai_protocol  = 0,            /* Any protocol */
            .ai_canonname = NULL,
            .ai_addr      = NULL,
            .ai_next      = NULL,
    };
    char buff[7];
    snprintf(buff, sizeof(buff), "%hu", s->client.request.request.port);
    if(  0!=  getaddrinfo(s->client.request.request.host, buff, &hints, &s->origin_resolution)){
        perror("Domain Error");
    }
    selector_notify_block(key->s, key->fd);
    free(data);

    return 0;
}

/** procesa el resultado de la resolución de nombres */
static unsigned
request_resolv_done(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;
    struct http *s = ATTACHMENT(key);
    int ret;
    if (s->origin_resolution == 0){
        d->status = status_general_SOCKS_server_failure;
        write_buffer_string(d->wb,HTTP_CODE_404);
        selector_status s = 0;
        s |= selector_set_interest(key->s, *d->client_fd, OP_WRITE);
        ret = SELECTOR_SUCCESS == s ? REQUEST_WRITE : ERROR;
        return ret;
    } else {
        s->origin_domain = s->origin_resolution->ai_family;
        s->origin_addr_len = s->origin_resolution->ai_addrlen;
        memcpy(&s->origin_addr,
               s->origin_resolution->ai_addr,
               s->origin_resolution->ai_addrlen);
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }
    if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE)) {
        return ERROR;
    }


    return request_connect(key, d);
}

/** intenta establecer una conexión con el origin server */
static unsigned
request_connect(struct selector_key *key, struct request_st *d) {
    bool error = false;
    // da legibilidad
    enum response_status status = d->status;
    int *fd = d->origin_fd;
    *fd = socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, 0);
    if (*fd == -1) {
        error = true;
        goto finally;
    }
    if (selector_fd_set_nio(*fd) == -1) {
        goto finally;
    }
    if (-1 == (connect(*fd, (const struct sockaddr *) &ATTACHMENT(key)->origin_addr,
                      ATTACHMENT(key)->origin_addr_len))) {
        if (errno == EINPROGRESS) {
            // es esperable,  tenemos que esperar a la conexión

            // dejamos de de pollear el socket del cliente
            selector_status st = selector_set_interest_key(key, OP_NOOP);
            if (SELECTOR_SUCCESS != st) {
                error = true;
                goto finally;
            }

            // esperamos la conexion en el nuevo socket
            st = selector_register(key->s, *fd, &http_handler,
                                   OP_WRITE, key->data);
            if (SELECTOR_SUCCESS != st) {
                error = true;
                goto finally;
            }
            ATTACHMENT(key)->references += 1;
        } else {
            status = errno_to_socks(errno);
            error = true;
            goto finally;
        }
    } else {
        // estamos conectados sin esperar... no parece posible
        // saltaríamos directamente a COPY
        abort();
    }

    finally:
    if (error) {
        if (*fd != -1) {
            close(*fd);
            *fd = -1;
        }
        return ERROR;
    }
    d->status = status;

    return REQUEST_CONNECTING;
}

static void
request_read_close(const unsigned state, struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;

    request_close(&d->parser);
}

// REQUEST CONNECT
////////////////////////////////////////////////////////////////////////////////
static void
request_connecting_init(const unsigned state, struct selector_key *key) {
    struct connecting *d = &ATTACHMENT(key)->orig.conn;

    d->client_fd = &ATTACHMENT(key)->client_fd;
    d->origin_fd = &ATTACHMENT(key)->origin_fd;
    d->status = &ATTACHMENT(key)->client.request.status;
    d->wb = &ATTACHMENT(key)->write_buffer;

}

/** la conexión ha sido establecida (o falló)  */
static unsigned
request_connecting(struct selector_key *key) {
    struct request_st *buf = &ATTACHMENT(key)->client.request;

    int error;
    socklen_t len = sizeof(error);
    struct connecting *d = &ATTACHMENT(key)->orig.conn;

    if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        *d->status = status_general_SOCKS_server_failure;
    } else {
        if (error == 0) {
            *d->status = status_succeeded;
            *d->origin_fd = key->fd;
        } else {
            *d->status = errno_to_socks(error);
        }
    }

    while (buffer_can_read(&buf->accum)){
        if(buffer_can_write(d->wb))
            buffer_write(d->wb,buffer_read(&buf->accum));
        else
            continue;
    }



    selector_status s = 0;
    s |= selector_set_interest(key->s, *d->origin_fd, OP_WRITE);
    return SELECTOR_SUCCESS == s ? REQUEST_WRITE : ERROR;
}

void log_request(enum response_status status,
                 const struct sockaddr *clientaddr,
                 const struct sockaddr *originaddr);

/** escribe todos los bytes de la respuesta al mensaje `request' */
static unsigned
request_write(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;
    unsigned ret = REQUEST_WRITE;
    buffer *b = d->wb;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);
    if (n == -1) {
        ret = ERROR;
    } else {
        /* Metrics Start. */
        metrstr->transfby->tfbyt_ll += n;
        /* Metrics end.   */
        buffer_read_adv(b, n);

        if (!buffer_can_read(b)) {
            if (d->status == status_succeeded) {
                ret = COPY;
                selector_set_interest(key->s, *d->client_fd, OP_READ);
                selector_set_interest(key->s, *d->origin_fd, OP_READ);
            } else {
                ret = DONE;
                selector_set_interest(key->s, *d->client_fd, OP_NOOP);
                if (-1 != *d->origin_fd) {
                    selector_set_interest(key->s, *d->origin_fd, OP_NOOP);
                }
            }
        }
    }

    log_request(d->status, (const struct sockaddr *) &ATTACHMENT(key)->client_addr,
                (const struct sockaddr *) &ATTACHMENT(key)->origin_addr);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// COPY
////////////////////////////////////////////////////////////////////////////////

static void
copy_init(const unsigned state, struct selector_key *key) {
    struct copy *d = &ATTACHMENT(key)->client.copy;

    d->fd = &ATTACHMENT(key)->client_fd;
    d->rb = &ATTACHMENT(key)->read_buffer;
    d->wb = &ATTACHMENT(key)->write_buffer;
    d->duplex = OP_READ | OP_WRITE;
    d->other = &ATTACHMENT(key)->orig.copy;

    d = &ATTACHMENT(key)->orig.copy;
    d->fd = &ATTACHMENT(key)->origin_fd;
    d->rb = &ATTACHMENT(key)->write_buffer;
    d->wb = &ATTACHMENT(key)->read_buffer;
    d->duplex = OP_READ | OP_WRITE;
    d->other = &ATTACHMENT(key)->client.copy;
}

/**
 * Computa los intereses en base a la disponiblidad de los buffer.
 * La variable duplex nos permite saber si alguna vía ya fue cerrada.
 * Arrancá OP_READ | OP_WRITE.
 */
static fd_interest
copy_compute_interests(fd_selector s, struct copy *d) {
    fd_interest ret = OP_NOOP;
    if ((d->duplex & OP_READ) && buffer_can_write(d->rb)) {
        ret |= OP_READ;
    }
    if ((d->duplex & OP_WRITE) && buffer_can_read(d->wb)) {
        ret |= OP_WRITE;
    }
    if (SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret)) {
        abort();
    }
    return ret;
}

/** elige la estructura de copia correcta de cada fd (origin o client) */
static struct copy *
copy_ptr(struct selector_key *key) {
    struct copy *d = &ATTACHMENT(key)->client.copy;

    if (*d->fd == key->fd) {
        // ok
    } else {
        d = d->other;
    }
    return d;
}

/** lee bytes de un socket y los encola para ser escritos en otro socket */
static unsigned
copy_r(struct selector_key *key) {
    struct copy *d = copy_ptr(key);

    assert(*d->fd == key->fd);

    size_t size;
    ssize_t n;
    buffer *b = d->rb;
    unsigned ret = COPY;

    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);
    if (n <= 0) {
        shutdown(*d->fd, SHUT_RD);
        d->duplex &= ~OP_READ;
        if (*d->other->fd != -1) {
            shutdown(*d->other->fd, SHUT_WR);
            d->other->duplex &= ~OP_WRITE;
        }
    } else {
        buffer_write_adv(b, n);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other);
    if (d->duplex == OP_NOOP) {
        ret = DONE;
    }
    return ret;
}

/** escribe bytes encolados */
static unsigned
copy_w(struct selector_key *key) {
    struct copy *d = copy_ptr(key);

    assert(*d->fd == key->fd);
    size_t size;
    ssize_t n;
    buffer *b = d->wb;
    unsigned ret = COPY;

    uint8_t *ptr = buffer_read_ptr(b, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if (n == -1) {
        shutdown(*d->fd, SHUT_WR);
        d->duplex &= ~OP_WRITE;
        if (*d->other->fd != -1) {
            shutdown(*d->other->fd, SHUT_RD);
            d->other->duplex &= ~OP_READ;
        }
    } else {
        /* Metrics Start. */
        metrstr->transfby->tfbyt_ll += n;
        /* Metrics end.   */

        buffer_read_adv(b, n);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other);
    if (d->duplex == OP_NOOP) {
        ret = DONE;
    }
    return ret;
}

/** definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
        {
                .state = REQUEST_READ,
                .on_arrival = request_init,
                .on_departure = request_read_close,
                .on_read_ready = request_read,
        },
        {
                .state = REQUEST_RESOLV,
                .on_block_ready = request_resolv_done,
        },
        {
                .state = REQUEST_CONNECTING,
                .on_arrival = request_connecting_init,
                .on_write_ready = request_connecting,
        },
        {
                .state = REQUEST_WRITE,
                .on_write_ready = request_write,
        },
        {
                .state = COPY,
                .on_arrival = copy_init,
                .on_read_ready = copy_r,
                .on_write_ready = copy_w,
        },
        {
                .state = DONE,

        },
        {
                .state = ERROR,
        }};

static const struct state_definition *
http_describe_states(void) {
    return client_statbl;
}

///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.
static void
http_done(struct selector_key *key);

static void
http_read(struct selector_key *key) {
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum http_state st = stm_handler_read(stm, key);

    if (ERROR == st || DONE == st) {
        http_done(key);
    }
}

static void
http_write(struct selector_key *key) {
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum http_state st = stm_handler_write(stm, key);

    if (ERROR == st || DONE == st) {
        http_done(key);
    }
}

static void
http_block(struct selector_key *key) {
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum http_state st = stm_handler_block(stm, key);

    if (ERROR == st || DONE == st) {
        http_done(key);
    }
}

static void
http_close(struct selector_key *key) {
    http_destroy(ATTACHMENT(key));
}

static void
http_done(struct selector_key *key) {
    const int fds[] = {
            ATTACHMENT(key)->client_fd,
            ATTACHMENT(key)->origin_fd,
    };
    for (unsigned i = 0; i < N(fds); i++) {
        if (fds[i] != -1) {
            if (SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
            close(fds[i]);
        }
    }

    /* Metrics Start. */
    metrstr->currcon -= 1;
    metrstr->connsucc += 1;
    /* Metrics End  . */
}
