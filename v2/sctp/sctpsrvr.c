  /*
   *  sctpsrvr.c
   *
   *  SCTP multi-stream server.
   *
   *  M. Tim Jones <mtj@mtjones.com>
   *  Martin Capparelli
   *  
   */

  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <time.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <netinet/in.h>
  #include <netinet/sctp.h>
  #include <sys/select.h>
  #include <errno.h>
  #include <stdbool.h>

  #include "map.h"
  #include "common.h"
  #include "handlers.h"

  #define MAX_CLIENTS 30


  void clean(int * flag, uint8_t * buffer);

int main() {
  int listenSock, connSock, ret, flags, in, act, max_sd, clients[MAX_CLIENTS], 
      logged[MAX_CLIENTS], i, j, sd, sd2, speaking[MAX_CLIENTS];
  struct sockaddr_in servaddr;
  struct sctp_initmsg initmsg;
  struct sctp_status status;
  uint8_t buffer[MAX_BUFFER+1];
  struct sctp_sndrcvinfo sndrcvinfo = {0};
  struct sctp_event_subscribe events;

  fd_set readfds, writefds;
  map_void_t m;

  char * usr = "admin";
  char * pss = "admin";

  /* Create SCTP TCP-Style Socket */
  listenSock = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP );

  if(listenSock < 0){
    perror("Can not create socket.\n");
    return -1;
  }

  /* Accept connections from any interface */
  bzero( (void *)&servaddr, sizeof(servaddr) );
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
  servaddr.sin_port = htons(MY_PORT_NUM);

  ret = bind( listenSock, (struct sockaddr *)&servaddr, sizeof(servaddr) );
  if(ret < 0){
    perror("Can not bind socket.\n");
    return -1;
  }

  /* Specify that a maximum of 5 streams will be available per socket */
  memset( &initmsg, 0, sizeof(initmsg) );
  initmsg.sinit_num_ostreams = 5;
  initmsg.sinit_max_instreams = 5;
  initmsg.sinit_max_attempts = 4;
  ret = setsockopt( listenSock, IPPROTO_SCTP, SCTP_INITMSG, 
   &initmsg, sizeof(initmsg) );

  /* Place the server socket into the listening state */
  ret = listen( listenSock, 5 );
  if(ret < 0){
    perror("Can not listen on socket.\n");
    return -1;
  }

  /* Enable receipt of SCTP Snd/Rcv Data via sctp_recvmsg */
  memset( (void *)&events, 0, sizeof(events) );
  events.sctp_data_io_event = 1;
  ret = setsockopt( listenSock, SOL_SCTP, SCTP_EVENTS,
                    (const void *)&events, sizeof(events) );
  if(ret < 0){
    perror("Can not send and receive data on socket.\n");
    return -1;
  }

  /* Initializing clients, logged and listen to 0. */
  for(i = 0; i < MAX_CLIENTS; i++){
    clients[i] = 0;
    logged[i] = 0;
    speaking[i] = 0;
  }

  /* Initializing map. */
  map_init(&m);

  /* Server loop... */
  while( 1 ) {
    printf("Awaiting a new connection...\n");

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(listenSock, &readfds);

    max_sd = listenSock;

    /* Adding clients to readfds & writefds. */
    for(i = 0; i < MAX_CLIENTS; i++){
      sd = clients[i];
      sd2 = speaking[i];

      if(sd > 0){
        FD_SET(sd, &readfds);
        if(sd > max_sd)
          max_sd = sd;
      }

      if(sd2 > 0){
        FD_SET(sd2, &writefds);
        FD_CLR(sd2, &readfds);
        if(sd2 > max_sd)
          max_sd = sd2;
      }
    }

    act = select(max_sd + 1, &readfds, &writefds, NULL, NULL);

    if(act < 0 && errno != EINTR)
      printf("Select error. \n");

    if(FD_ISSET(listenSock, &readfds)){
      /* Await a new client connection */
      connSock = accept(listenSock, (struct sockaddr *)NULL, (int *)NULL);

      if(connSock < 0){
        printf("Error at connection.\n");
        break;
      } else {
        printf("Connected... to #%i\n", connSock);
        for(i = 0; i < MAX_CLIENTS; i++){
          if(clients[i] == 0){
            clients[i] = connSock;
            if(max_sd < connSock){
              max_sd = connSock;
            }
            break;
          }
        }
      }
    } else { 
      for(i = 0; i < MAX_CLIENTS; i++){
        connSock = clients[i];
        if(FD_ISSET(connSock, &readfds)){

          printf("Attending client... #%i\n", connSock);

          /* Cleaning buffer and flags before receiving info. */
          clean(&flags, &buffer);

          // Start J2M2 Logic
          in = sctp_recvmsg( connSock, (void *) buffer, sizeof(buffer),
                  (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags);
          if(in < 0){
            perror("Error at receiving msg.\n");
            break;
          }

          for(j = 0; j < MAX_CLIENTS; j++){
              if(speaking[j] == 0){
                speaking[j] = connSock;
                break;
              }
          }

          if(!is_logged(connSock, logged)){
            uint8_t * bufs = (uint8_t *) calloc(LOGIN_RESPONSE_LEN, sizeof(uint8_t));
            bufs[TYPE] = 0;
            bufs[COMMAND] = 0;
            bufs[ARGSQ] = 0;
            bufs[CODE] = 5; //unautorized default.

            if (buffer[TYPE] == 0 && buffer[COMMAND] == 0 
                    && buffer[ARGSQ] == 2 && buffer[CODE] == 0){
              if(log(buffer, usr, pss)){
                for(j = 0; j < MAX_CLIENTS; j++){
                  if(logged[j] == 0){
                    logged[j] = connSock;
                    bufs[CODE] = 1; //Change default to ok.
                    break;
                  }
                } 
              }
            } else {
              bufs[CODE] = 3; //Bad Request.
            }

            /* Message will be stored until O.S. is available to send . */
            map_set(&m, &connSock, bufs);

          } else { 

            uint8_t * bufs = (uint8_t *) calloc(COM_RESPONSE_LEN, sizeof(uint8_t));
            bufs[TYPE] = 0;
            bufs[COMMAND] = 0;
            bufs[ARGSQ] = 0;
            bufs[CODE] = 3; //bad request default.

            switch(buffer[0]){
              case 1: //metric 
                bufs[3] = 1;
                break;

              case 2: //config
                bufs[3] = 2;
                break;

              case 3: //sys
                bufs[3] = 3;
                if (bufs[1] == 2){  //Exit
                  printf("Closing connection...\n");
                  close(connSock);
                  continue;
                }
                break;

              default: // bad request
                break;
            }

            map_set(&m, &connSock, bufs);
          }
        } else if(FD_ISSET(connSock, &writefds)){
          printf("Sending msg to #%i...", connSock);
          uint8_t * b = (uint8_t *) map_get(&m, &connSock);
          if(b != NULL){
            ret = sctp_sendmsg( connSock, (void *) b, sizeof(b) ,
                          NULL, 0, 0, 0, STREAM, 0, 0 );
            printf("OK.\n");
          } else {
            printf("Not OK.\n");
          }
          if(ret > 0){
            for(j = 0; j < MAX_CLIENTS; j++){
              if(speaking[j] == connSock){
                speaking[j] = 0;
                break;
              }
            }
            map_remove(&m, &connSock);
          } else {
            printf("Error at sending msg from: %i.\n", connSock);
          }
        } else {
          if(connSock < 0){
            printf("Closing connection...\n");
            close( connSock );
          }
        }
      }
    }
  }
  return 0;
}

void clean(int * flag, uint8_t * buffer){
  flag = 0;  
  for(int i = 0; i < MAX_BUFFER; i++){
    buffer[i] = 0;
  }
}

/* Log returns 1 in case user logged in succesfuly. 0 otherwise.*/
int log(uint8_t * buf, char * us, char * ps){
  char usr[MAX_USER + 1] = {0}, pss[MAX_PASS + 1] = {0}, c;
  int i = START_USER;
  int j = 0;

  /* Getting username. */
  for(i, j; j < MAX_USER; i++, j++){
    c = (char) buf[i];
    if(c != ' '){
      usr[j] = c;
    } else {
      break;
    }
  }
  usr[j+1] = "\0";

  /* Incrementing i to set it at first password character*/
  i++;

  /* Getting password. */
  for(i, j = 0; j < MAX_PASS ; i++, j++){
    c = (char) buf[i];
    if(c != '\0'){
      pss[j] = c;
    } else {
      break;
    }
  }

  return !strcmp(usr,us) && !strcmp(pss,ps);
}

int is_logged(int connSock, int * con){
  for(int i = 0; i < MAX_CLIENTS; i++){
    if(connSock == con[i])
      return 1;
  }
  return 0;
}


