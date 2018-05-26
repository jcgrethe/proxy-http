/**
 * http.c -- Implementa de forma bloqueante un proxy http
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <pthread.h>

#include "http.h"
// #include "hello.h"
#include "request.h"
#include "netutils.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

// /**
//  * Punto de entrada de hilo que copia el contenido de un fd a otro.
//  */
// static void *
// copy_pthread(void *d) {
//   const int *fds = d;
// //  pthread_detach(pthread_self());

//   sock_blocking_copy(fds[0], fds[1]);
//   shutdown(fds[0], SHUT_RD);
//   shutdown(fds[1], SHUT_WR);

//   return 0;
// }

// /** callback del parser utilizado en `read_hello' */
// static void
// on_hello_method(struct hello_parser *p, const uint8_t method) {
//     uint8_t *selected  = p->data;

//     if(SOCKS_HELLO_NOAUTHENTICATION_REQUIRED == method) {
//        *selected = method;
//     }
// }

// /**
//  * lee e interpreta la trama `hello' que arriba por fd
//  *
//  * @return true ante un error.
//  */
// static bool
// read_hello(const int fd, const uint8_t *method) {
//     // 0. lectura del primer hello
//     uint8_t buff[256 + 1 + 1];
//     buffer buffer; buffer_init(&buffer, N(buff), buff);
//     struct hello_parser hello_parser = {
//         .data                     = (void *)method,
//         .on_authentication_method = on_hello_method,
//     };
//     hello_parser_init(&hello_parser);
//     bool error = false;
//     size_t buffsize;
//     ssize_t n;
//     do {
//         uint8_t *ptr = buffer_write_ptr(&buffer, &buffsize);
//         n = recv(fd, ptr, buffsize, 0);
//         if(n > 0) {
//             buffer_write_adv(&buffer, n);
//             const enum hello_state st = hello_consume(&buffer, &hello_parser, &error);
//             if(hello_is_done(st, &error)) {
//                 break;
//             }
//         } else {
//             break;
//         }
//     } while(true);

//     if(!hello_is_done(hello_parser.state, &error)) {
//         error = true;
//     }
//     hello_parser_close(&hello_parser);
//     return error;
// }

// /**
//  * lee e interpreta la trama `request' que arriba por fd
//  *
//  * @return true ante un error.
//  */
// static bool
// read_request(const int fd, struct request *request) {
//     uint8_t buff[22];
//     buffer  buffer; buffer_init(&buffer, N(buff), buff);
//     size_t  buffsize;

//     struct request_parser request_parser = {
//         .request = request,
//     };
//     request_parser_init(&request_parser);
//     unsigned    n = 0;
//        bool error = false;

//     do {
//         uint8_t *ptr = buffer_write_ptr(&buffer, &buffsize);
//         n = recv(fd, ptr, buffsize, 0);
//         if(n > 0) {
//             buffer_write_adv(&buffer, n);
//             const enum request_state st = request_consume(&buffer,
//                         &request_parser, &error);
//             if(request_is_done(st, &error)) {
//                 break;
//             }

//         } else {
//             break;
//         }
//     }while(true);
//     if(!request_is_done(request_parser.state, &error)) {
//         error = true;
//     }
//     request_close(&request_parser);
//     return error;
// }

/** loguea el request a stdout */
void
log_request(const enum socks_response_status status,
            const struct sockaddr* clientaddr,
            const struct sockaddr* originaddr) {
    char cbuff[SOCKADDR_TO_HUMAN_MIN * 2 + 2 + 32] = { 0 };
    unsigned n = N(cbuff);
    time_t now = 0;
    time(&now);

    // tendriamos que usar gmtime_r pero no está disponible en C99
    strftime(cbuff, n, "%FT%TZ\t", gmtime(&now));
    size_t len = strlen(cbuff);
    sockaddr_to_human(cbuff + len, N(cbuff) - len, clientaddr);
    strncat(cbuff, "\t", n-1);
    cbuff[n-1] = 0;
    len = strlen(cbuff);
    sockaddr_to_human(cbuff + len, N(cbuff) - len, originaddr);

    fprintf(stdout, "%s\tstatus=%d\n", cbuff, status);
}

// /**
//  * maneja cada conexión entrante
//  *
//  * @param fd     descriptor de la conexión entrante.
//  * @param caddr  información de la conexiónentrante.
//  */
// static void
// http_handle_connection(const int fd, const struct sockaddr *caddr) {
//     uint8_t method = SOCKS_HELLO_NO_ACCEPTABLE_METHODS;
//     struct request request;
//     struct sockaddr *originaddr = 0x00;
//     socklen_t        origin_addr_len = 0;
//     int              origin_domain;
//     int originfd = -1;
//     uint8_t buff[10];
//     buffer b; buffer_init(&(b), N(buff), (buff));

//     // 0. lectura del hello enviado por el cliente
//     if(read_hello(fd, &method)) {
//         goto finally;
//     }

//     // 1. envio de la respuesta
//     const uint8_t r = (method == SOCKS_HELLO_NO_ACCEPTABLE_METHODS)
//                     ? 0xFF : 0x00;
//     hello_marshall(&b, r);
//     if(sock_blocking_write(fd, &b)) {
//         goto finally;
//     }
//     if(SOCKS_HELLO_NO_ACCEPTABLE_METHODS == method) {
//         goto finally;
//     }

//     // 2. lectura del request
//     enum socks_response_status status = status_general_SOCKS_server_failure;
//     if(read_request(fd, &request)) {
//         status = status_general_SOCKS_server_failure;
//     } else {
//         // 3. procesamiento
//         switch (request.cmd) {
//             case socks_req_cmd_connect: {
//                 bool error = false;
//                 status = cmd_resolve(&request, &originaddr, &origin_addr_len,
//                                      &origin_domain);
//                 if(originaddr == NULL) {
//                     error = true;
//                 } else {
//                     originfd = socket(origin_domain, SOCK_STREAM, 0);
//                     if(originfd == -1) {
//                         error = true;
//                     } else {
//                         if(-1 == connect(originfd, originaddr, origin_addr_len)) {
//                             status = errno_to_socks(errno);
//                             error  = true;
//                         } else {
//                             status = status_succeeded;
//                         }
//                     }
//                 }
//                 if(error) {
//                     if(originfd != -1) {
//                         close(originfd);
//                         originfd = -1;
//                     }
//                     close(fd);
//                 }
//                 break;
//             } case socks_req_cmd_bind:
//             case socks_req_cmd_associate:
//             default:
//                 status = status_command_not_supported;
//                 break;
//         }
//     }
//     log_request(status, caddr, originaddr);

//     // 4. envio de respuesta al request.
//     request_marshall(&b, status);
//     if(sock_blocking_write(fd, &b)) {
//         goto finally;
//     }

//     if(originfd == -1) {
//         goto finally;
//     }

//     // 5. copia dos vías
//     pthread_t tid;
//     int fds[2][2]= {
//         {originfd, fd      },
//         {fd,       originfd},
//     };

//     if(pthread_create(&tid, NULL, copy_pthread, fds[0])) {
//         goto finally;
//     }
//     sock_blocking_copy(fds[1][0], fds[1][1]);
//     pthread_join (tid, 0);

//     finally:
//         if(originfd != -1) {
//             close(originfd);
//         }
//         close(fd);
// }

// /**
//  * estructura utilizada para transportar datos entre el hilo
//  * que acepta sockets y los hilos que procesa cada conexión
//  */
// struct connection {
//     int fd;
//     socklen_t addrlen;
//     struct sockaddr_in6 addr;
// };

// /** rutina de cada hilo worker */
// static void *
// handle_connection_pthread(void *args) {
//     const struct connection *c = args;
//     pthread_detach(pthread_self());

//     http_handle_connection(c->fd, (struct sockaddr *)&c->addr);
//     free(args);

//     return 0;
// }

// /**
//  * atiende a los clientes de forma concurrente con I/O bloqueante.
//  */
// extern int
// serve_http_concurrent_blocking(const int server) {
//     for (;;) {
//         struct sockaddr_in6 caddr;
//         socklen_t caddrlen = sizeof (caddr);
//         // Wait for a client to connect
//         const int client = accept(server, (struct sockaddr*)&caddr, &caddrlen);
//         if (client < 0) {
//             perror("unable to accept incoming socket");
//         } else {
//             // TODO(juan): limitar la cantidad de hilos concurrentes
//             struct connection* c = malloc(sizeof (struct connection));
//             if (c == NULL) {
//                 // lo trabajamos iterativamente
//                 http_handle_connection(client, (struct sockaddr*)&caddr);
//             } else {
//                 pthread_t tid;
//                 c->fd = client;
//                 c->addrlen = caddrlen;
//                 memcpy(&(c->addr), &caddr, caddrlen);
//                 if (pthread_create(&tid, 0, handle_connection_pthread, c)) {
//                     free(c);
//                     // lo trabajamos iterativamente
//                     http_handle_connection(client, (struct sockaddr*)&caddr);
//                 }
//             }
//         }
//     }
//     return 0;
// }

