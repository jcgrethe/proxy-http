#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>

#include "../selector.h"
#include "sctp_integration.h"

// #include "../httpnio.c"
#define N(x) (sizeof(x)/sizeof((x)[0]))
#define ATTACHMENT(key) ( (struct sctp_data *)(key)->data)

void sctp_read(struct selector_key *key);

void sctp_write(struct selector_key *key);

void sctp_close(struct selector_key *key);

static const struct fd_handler sctp_handler = {
        .handle_read   = sctp_read,
        .handle_write  = sctp_write,
        .handle_close  = sctp_close,
        .handle_block  = NULL,
};

// management struct functions.
struct sctp_data *
new_sctp_data(const int client_fd){
    struct sctp_data *ret;
    ret = malloc(sizeof(struct sctp_data));
    if (ret == NULL){
        return ret;
    }

    ret->client_fd     = client_fd;
    buffer_init(&ret->buffer_write, N(ret->raw_buffer_write),ret->raw_buffer_write);
    buffer_init(&ret->buffer_read , N(ret->raw_buffer_read) ,ret->raw_buffer_read);
    return ret;
}

void sctp_passive_accept(struct selector_key *key){
 	printf("Arrived to sctp_passive_accept\n");
	
	struct sockaddr_storage       client_addr;
	socklen_t                     client_addr_len = sizeof(client_addr);
	struct sctp_data                  * data_struct           = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr,
                              &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    // print_connection_status("Connection established", client_addr);
    printf("File descriptor: %d\n", client);
    data_struct = new_sctp_data(client);
    if(data_struct == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }

    memcpy(&data_struct->client_addr, &client_addr, client_addr_len);
    data_struct->client_addr_len = client_addr_len;

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &sctp_handler,
                                             OP_READ, data_struct)) {
        goto fail;
    }

    return ;

    fail:
    if(client != -1) {
        close(client);
    }
    // data_struct_destroy(data_struct);
}

// When server is reading - key structure obtained from selector with data and current active clien
// For more info about key see selector.h file
void sctp_read(struct selector_key *key){
	printf("reading...\n");

	//Getting data
	struct sctp_data *data = ATTACHMENT(key);	//Obtiene la data dentro de la key
	//set interest OP WRITE tells to the selector that you are going to write and after this it calls sctp_write
	//Thats why you have to save things on buffers in data structure    
    if (handle_request(data) < 0 || selector_set_interest(key->s, key->fd, OP_WRITE) != SELECTOR_SUCCESS){
        selector_unregister_fd(key->s, data->client_fd);
    };


	exit(0); 	//REMOVE!
}

// When server is writting
void sctp_write(struct selector_key *key){
	printf("writting!\n");
}

// When server is closing
void sctp_close(struct selector_key *key){
	printf("closing!\n");
}

int handle_request(struct sctp_data *data){
    buffer * b = &data->buffer_read;
}