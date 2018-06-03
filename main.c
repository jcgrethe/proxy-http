/**
 * main.c - servidor proxy http y sctp concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "http.h"
#include "selector.h"
#include "httpnio.h"

//#include "sctp/sctp_integration.h"

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n", signal);
    done = true;
}

int
main(const int argc, const char **argv) {
    unsigned port = 1080;
    unsigned sctp_port = 1081;

    if (argc == 1) {
        // utilizamos el default
    } else if (argc == 2) {
        char *end = 0;
        const long sl = strtol(argv[1], &end, 10);

        if (end == argv[1] || '\0' != *end
            || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
            || sl < 0 || sl > USHRT_MAX) {
            fprintf(stderr, "port should be an integer: %s\n", argv[1]);
            return 1;
        }
        port = sl;
    } else {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // no tenemos nada que leer de stdin
    close(0);

    const char *err_msg = NULL;
    selector_status ss = SELECTOR_SUCCESS;
    fd_selector selector = NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    struct sockaddr_in addr_sctp;
    memset(&addr_sctp, 0, sizeof(addr_sctp));
    addr_sctp.sin_family = AF_INET;
    addr_sctp.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_sctp.sin_port = htons(sctp_port);


    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    const int sctp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);

    if (server < 0) {
        err_msg = "unable to create server socket";
        goto finally;
    }
//    if (sctp_socket < 0) {
//        err_msg = "unable to create sctp socket";
//        goto finally;
//    }
//    fprintf(stdout, "Listening on TCP port %d for http and %d for sctp.\n", port, sctp_port);
    fprintf(stdout, "Listening on TCP port %d for http.\n", port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int));

    // Main proxy socket listening
    if (bind(server, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }


    //SCTP Socket listening
//    if (bind(sctp_socket, (struct sockaddr *) &addr_sctp, sizeof(addr_sctp)) < 0) {
//        err_msg = "unable to bind socket for sctp";
//        goto finally;
//    }
//
//    if (listen(sctp_socket, 20) < 0) {
//        err_msg = "unable to listen sctp socket";
//        goto finally;
//    }
    // check with: sudo nmap -sY localhost -PY -p 1081

    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    if (selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }
    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec  = 10,
                    .tv_nsec = 0,
            },
    };
    if (0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    selector = selector_new(1024);
    if (selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }
    const struct fd_handler http = {
            .handle_read       = http_passive_accept,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };
//    const struct fd_handler sctp_handler = {
//            .handle_read       = sctp_passive_accept,  //TODO: Put sctp function
//            .handle_write      = NULL,
//            .handle_close      = NULL, // nada que liberar
//    };
    ss = selector_register(selector, server, &http,
                           OP_READ, NULL);
//    selector_status ss_sctp = selector_register(selector, sctp_socket, &sctp_handler,
//                                                OP_READ, NULL);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "registering server fd";
        goto finally;
    }
//    if (ss_sctp != SELECTOR_SUCCESS) {
//        err_msg = "registering sctp fd";
//        goto finally;
//    }


    for (; !done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if (ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if (err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
    finally:
    if (ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "" : err_msg,
                ss == SELECTOR_IO
                ? strerror(errno)
                : selector_error(ss));
        ret = 2;
    } else if (err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if (selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();

    http_pool_destroy();

    if (server >= 0) {
        close(server);
    }
    return ret;
}
