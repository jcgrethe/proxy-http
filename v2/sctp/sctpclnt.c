/*
 *  sctpclnt.c
 *
 *  SCTP multi-stream client.
 *
 *  M. Tim Jones <mtj@mtjones.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"
#include <netinet/sctp.h>
#include "handlers.h"

 #define PROXY_SCTP_PORT 1081

int main()
{
  int connSock, in, i, ret, flags;
  int * position;
  struct sockaddr_in servaddr;
  struct sctp_status status;
  struct sctp_sndrcvinfo sndrcvinfo;
  struct sctp_event_subscribe events;
  struct sctp_initmsg initmsg;
  uint8_t buffer[MAX_BUFFER+1];

  clean(buffer);
  position = 0;
  
  /* Create an SCTP TCP-Style Socket */
  connSock = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP );

  /* Specify that a maximum of 5 streams will be available per socket */
  memset( &initmsg, 0, sizeof(initmsg) );
  initmsg.sinit_num_ostreams = 5;
  initmsg.sinit_max_instreams = 5;
  initmsg.sinit_max_attempts = 4;
  ret = setsockopt( connSock, IPPROTO_SCTP, SCTP_INITMSG,
                     &initmsg, sizeof(initmsg) );

  /* Specify the peer endpoint to which we'll connect */
  bzero( (void *)&servaddr, sizeof(servaddr) );
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PROXY_SCTP_PORT);
  servaddr.sin_addr.s_addr = inet_addr( "127.0.0.1" );

  /* Connect to the server */
  ret = connect( connSock, (struct sockaddr *)&servaddr, sizeof(servaddr) );

  printf("Connected?: %d %d\n", ret, PROXY_SCTP_PORT);
  /* Enable receipt of SCTP Snd/Rcv Data via sctp_recvmsg */
  memset( (void *)&events, 0, sizeof(events) );
  events.sctp_data_io_event = 1;
  ret = setsockopt( connSock, SOL_SCTP, SCTP_EVENTS,
                     (const void *)&events, sizeof(events));

  /* Read and emit the status of the Socket (optional step) */
  in = sizeof(status);
  ret = getsockopt( connSock, SOL_SCTP, SCTP_STATUS,
                     (void *)&status, (socklen_t *)&in);
    
  printf("Hi! Please login to J2M2:\n");

  // Start J2M2 Logic
  if(fgets(buffer, sizeof(buffer), stdin) == NULL){              
    printf("No characters read.\n");
  }

  char first[MAX_BUFFER] = {0};
  char second[MAX_BUFFER] = {0};
  char third[MAX_BUFFER] = {0};
  sscanf(buffer, "%s %s %s", first, second, third);
    
    if(strcmp(first, "login")==0){
      if(handleLogin(second, third, connSock)){
        while(1){
            first[0] = NULL; second[0] = NULL; third[0] = NULL;
            if(fgets(buffer, sizeof(buffer), stdin) == NULL){
              printf("No characters read.\n");
              break;
            }
            sscanf(buffer, "%s %s %s", first, second, third);
            if(strcmp(first, "metric")==0){
              if(!handleMetric(second, third, connSock)){
                continue;
              }
            }else if(strcmp(first, "config")==0){
              if(!handleConfig(second, third, connSock)){
                continue;
              }
            }else if(strcmp(first, "help")==0){
              if(handleHelp(second, third, connSock)){
                continue;
              }
            }else if(strcmp(first, "exit")==0){
              if(handleExit(second, third, connSock)){
                continue;
              }
            }else{
              printf("Bad Request. %s %s %s\n", first, second, third);
              continue;
            }
            // Recieve response
            flags = 0;
            uint8_t resp[MAX_BUFFER];
            ret = sctp_recvmsg( connSock, (void *)resp, 24,
                                 (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags );
            if(resp[3] == 1){
              //Response statuts OK accepted
              printf("OK! Response: %s", &resp[4]);
              // printDatagram(resp, 20);
            }
        }
      }else{
        printf("Invalid Login.\n");
      }
    }
    close(connSock);
    return 0;
}

void clean(uint8_t * buf){
  for(int i = 0; i < MAX_BUFFER; i++){
    buf[i] = 0;
  }
  return;
}

void to_buffer(uint8_t * buf, uint8_t cpy[], int len, int * position){
  if(len > 0){  
    int i = 0;
    int j = (int) &position;
    for(i; i < len; i++){
      buf[j + i] = cpy[i];
    }
    position = j + i;
  }
  return;
}

