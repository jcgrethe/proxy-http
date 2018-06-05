/*
 * ~J2M2 Protocol~
 * SCTP Server for managing proxy metrics and configurations
 * More detailed info at J2M2 Documentation
 *
 * Client   Server
 * <-Connection->
 *
 * >v|Request---->
 * ^<|<---Response
 *
 * <----End------>
 */

#include <unistd.h>
#include <memory.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>

#include "metrics_struct.h"
#include "../selector.h"
#include "sctp_integration.h"
#include "common.h"

// #include "../httpnio.c"
#define N(x) (sizeof(x)/sizeof((x)[0]))
#define ATTACHMENT(key) ( (struct sctp_data *)(key)->data)

#define USER    "admin"
#define PASS    "admin"
#define UNIT    1

void sctp_read(struct selector_key *key);
void sctp_write(struct selector_key *key);
void sctp_close(struct selector_key *key);
int handle_request(struct sctp_data *data);
int check_login(char message[MAX_DATAGRAM_MESSAGE]);
void parse_datagram(struct sctp_data *data, buffer * b);
void handle_metric(struct sctp_data * data);
void handle_config(struct sctp_data * data);
void handle_sys(struct sctp_data * data);
void handle_badrequest(struct sctp_data * data);
void clean(uint8_t * buf);

static void sigpipe_handler();

// Global variables for handling SIPIPE property.
struct selector_key * current_key;
int current_client;
//metrics metrstr;
//tfbyte tf;

static const struct fd_handler sctp_handler = {
        .handle_read   = sctp_read,
        .handle_write  = sctp_write,
        .handle_close  = sctp_close,
        .handle_block  = NULL,
};

static void sigpipe_handler() {
    printf("Handling sigpipe of %d", current_client);
//    selector_set_interest(current_key->s, current_key->fd, OP_NOOP);
    if (selector_set_interest(current_key->s, current_key->fd, OP_NOOP) != SELECTOR_SUCCESS){
//        selector_unregister_fd(current_key->s, current_client);
        printf("Cant change operation");
        exit(0);
    }else{
//        selector_unregister_fd(current_key->s, current_client);
//        close(&current_client);
        exit(0);
    }
//    selector_unregister_fd(current_key->s, current_client);
//    close(current_client);
}

// management struct functions.
struct sctp_data * new_sctp_data(const int client_fd){
    struct sctp_data * ret;
    ret = malloc(sizeof(struct sctp_data));
    if (ret == NULL){
        return ret;
    }
    ret->is_logged = 0;
    ret->client_fd     = client_fd;
    buffer_init(&ret->buffer_write, N(ret->raw_buffer_write),ret->raw_buffer_write);
    buffer_init(&ret->buffer_read , N(ret->raw_buffer_read) ,ret->raw_buffer_read);
    return ret;
}

void sctp_passive_accept(struct selector_key *key){
 	printf("Welcome to SCTP Server\n");
	
	struct sockaddr_storage       client_addr;
	socklen_t                     client_addr_len = sizeof(client_addr);
	struct sctp_data              * data_struct           = NULL;

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

//    signal(SIGPIPE, SIG_IGN);
    //Handling SIGPIPE signals for avoiding killing server
    struct sigaction sa;
    sa.sa_handler = sigpipe_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa, 0) == -1) {
        perror("sigaction");
    }

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

	current_key = key;
	current_client = key->fd;

	//Getting data
	struct sctp_data *data = ATTACHMENT(key);	//Obtiene la data dentro de la key
	//set interest OP WRITE tells to the selector that you are going to write and after this it calls sctp_write
	//Thats why you have to save things on buffers in data structure    
    if (handle_request(data) <= 0 || selector_set_interest(key->s, key->fd, OP_WRITE) != SELECTOR_SUCCESS){
        selector_unregister_fd(key->s, data->client_fd);
    };
}

// When server is writting
void sctp_write(struct selector_key *key){
	struct sctp_data *data = ATTACHMENT(key);
	int length = HEADERS + strlen(data->datagram.message), ret;

	printf("Datagram: [%u %u %u %u] [%llu] [%u %u %u]\n", data->datagram.type, 
        data->datagram.command, data->datagram.argsq, data->datagram.code, data->datagram.message,
        data->datagram.message[START_CURR], data->datagram.message[START_HIS], data->datagram.message[START_SUC]);

	uint8_t * buffer = calloc(MAX_MSG, sizeof(uint8_t));
	buffer[TYPE] = data->datagram.type;
	buffer[COMMAND] = data->datagram.command;
	buffer[ARGSQ] = data->datagram.argsq;
	buffer[CODE] = data->datagram.code;
	memcpy(&buffer[START_BYTES], data->datagram.message, MAX_MSG);

	ret = sctp_sendmsg( data->client_fd, (void *) buffer, length ,
                          NULL, 0, 0, 0, STREAM, 0, 0 );

	if (ret>0){
		printf("Sent.\n");
	}
	if(data->datagram.type != 3 || data->datagram.command != 2) {
        if (selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
            selector_unregister_fd(key->s, data->client_fd);
        }
//	    selector_set_interest(key->s, key->fd, OP_READ);    //Get ready for listen the next request
    }else{
//        close(data->client_fd);
//        selector_set_interest(key->s, key->fd, OP_NOOP);    //Get ready for listen the next request
        selector_unregister_fd(key->s, data->client_fd);
    }
}

// When server is closing
void sctp_close(struct selector_key *key){
    struct sctp_data *data = ATTACHMENT(key);

    printf("closing %d!\n", data->client_fd);
	free(data);
}

int handle_request(struct sctp_data *data){
    buffer * b = &data->buffer_read;
    struct sctp_sndrcvinfo sndrcvinfo;
    int flags = 0;
	size_t count;
    uint8_t * ptr = buffer_write_ptr(&data->buffer_read, &count);
    ssize_t length;
    char c;
    int i = 0;
        length = sctp_recvmsg(data->client_fd, ptr, count, NULL, 0, &sndrcvinfo, &flags);
        if (length <= 0){
            return 0;
        }
        buffer_write_adv(&data->buffer_read, length);

        parse_datagram(data, b);
		
        if(!data->is_logged){
        	if (data->datagram.type == 0){
        		// Wants to log in
        		if(check_login(data->datagram.message)){	
        			data->is_logged = 1;
        			printf("Logged.\n");
        			data->datagram.argsq = 0; 
        			data->datagram.code = 1; //Ok 
        		}
        	}
        }else{
        	//Already logged & Analize request
        	switch(data->datagram.type){
              case 1: //metric 
                    handle_metric(data);
                    break;

              case 2: //config
                    handle_config(data);
                    break;

              case 3: //sys
                    handle_sys(data);
                    break;

              default: // bad request
                    handle_badrequest(data);
                    break;
            }
        }
  		printf("Datagram Header: %u %u %u %u\n", data->datagram.type, data->datagram.command, data->datagram.argsq, data->datagram.code);
    return 1; 
}

void parse_datagram(struct sctp_data *data, buffer * b){
	int i = 0;
	char c;
	if (buffer_can_read(b)){
			data->datagram.type = (uint8_t)buffer_read(b);
		}else{
			// Type not found
		}
		if (buffer_can_read(b)){
			data->datagram.command = (uint8_t)buffer_read(b);
		}else{
			// Command not found
		}
		if (buffer_can_read(b)){
			data->datagram.argsq = (uint8_t)buffer_read(b);
		}else{
			// Argsq not found
		}
		if (buffer_can_read(b)){
			data->datagram.code = (uint8_t)buffer_read(b);
		}else{
			// Code not found
		}
        while(buffer_can_read(b) && i < MAX_DATAGRAM_MESSAGE){
            c = buffer_read(b);
			data->datagram.message[i++] = c;
        }
}

int check_login(char message[MAX_DATAGRAM_MESSAGE]){
	char user[MAX_DATAGRAM_MESSAGE/2], pass[MAX_DATAGRAM_MESSAGE/2];
	sscanf(message, "%s %s", user, pass);
	if (strcmp(user, USER)!=0 || strcmp(pass, PASS)!=0){
		return 0;
	}
	return 1;
}

void handle_metric(struct sctp_data * data){
    clean(data->datagram.message);
    data->datagram.code = 1;
    if(data->datagram.argsq == 0){
        data->datagram.message[ONE_BYTE] = metrstr->currcon;    
        data->datagram.message[ONE_BYTE + 1] = metrstr->histacc;  
        data->datagram.message[ONE_BYTE + 2] = metrstr->connsucc;  
        memcpy(data->datagram.message, metrstr->transfby->tfbyt_8, NUMBYTES);   //8bytes transferbytes       
        data->datagram.command = 0; 
        data->datagram.argsq = 4;                                        
    }else if(data->datagram.argsq == 1){  
        switch (data->datagram.command){
            case CURRCON:
                data->datagram.message[0] = metrstr->currcon;
                data->datagram.code = 1;
                data->datagram.argsq = 1; 
                break;
            case HISTACC:
                data->datagram.message[0] = metrstr->histacc;
                data->datagram.code = 1;
                data->datagram.argsq = 1; 
                break;
            case TRABYTES:
                memcpy(data->datagram.message, metrstr->transfby->tfbyt_8, NUMBYTES);
                data->datagram.code = 1;
                data->datagram.argsq = 1;
                break;
            case CONNSUCC:
                data->datagram.message[0] = metrstr->connsucc;
                data->datagram.code = 1;
                data->datagram.argsq = 1;
                break;
            default:
                data->datagram.code = 4;
                data->datagram.argsq = 0;
        }
    }else{
        //Bad request. Only error code.
        data->datagram.code = 4;
    }
}

void handle_config(struct sctp_data * data){
    if(data->datagram.argsq == 0){
        // Return all current configurations
        strcpy(data->datagram.message, "currcon = 5\nbuffsize = 223\ntimeout = 3114\nconnsucc = 54\n");
        data->datagram.code = 1;
    }else if(data->datagram.argsq == 1){
        // Return
        switch (data->datagram.command){
            case 0:
                //strcpy(data->datagram.message, "currcon = 5\n");
                data->datagram.code = 1;
                break;
            case 1:
                strcpy(data->datagram.message, "buffsize = 5\n");
                data->datagram.code = 1;
                break;
            case 2:
                strcpy(data->datagram.message, "timeout = 5\n");
                data->datagram.code = 1;
                break;
            default:
                strcpy(data->datagram.message, "Invalid configuration.\n");
                data->datagram.code = 4;
        }
    }else if(data->datagram.argsq == 2){
        switch (data->datagram.command){
            case 0: //Set currconn value (data->datagram.message)
                if(1){  //Setted right
                    strcpy(data->datagram.message, "New currconn config setted.\n");
                    data->datagram.code = 1;
                }
                break;
            case 1: //Set buffsize value
                if(1){  //Setted right
                    strcpy(data->datagram.message, "New buffsize config setted.\n");
                    data->datagram.code = 1;
                }
                break;
            case 2: //Set timeout value
                if(1){  //Setted right
                    strcpy(data->datagram.message, "New timeout config setted.\n");
                    data->datagram.code = 1;
                }
                break;
            default:
                strcpy(data->datagram.message, "Invalid configuration.\n");
                data->datagram.code = 4;
        }
    }else{
        strcpy(data->datagram.message, "Invalid config arguments.\n");
        data->datagram.code = 4;
    }
}

void handle_sys(struct sctp_data * data){
    if(data->datagram.command == 0){        //Help
        strcpy(data->datagram.message, "Commands available:\n-help\n-metric [metric name]\n-config [config name] [config value]\n-exit\n");
        data->datagram.code = 1;
    } else if(data->datagram.command == 2){ //Exit
        strcpy(data->datagram.message, "Bye!\n");
        data->datagram.code = 1;
        //Closing socket managed after sending ok
    }
}

void handle_badrequest(struct sctp_data * data){
    strcpy(data->datagram.message, "Bad Request.\n");
    data->datagram.code = 3;
}

void clean(uint8_t * buf){
    for(int i = 0; i < MAX_DATAGRAM_MESSAGE; i++){
        buf[i] = 0;
    }
}

