/*
 *  sctpclnt.c
 *
 *  SCTP multi-stream client.
 *
 *  M. Tim Jones <mtj@mtjones.com>
 *
 */
//    Styles examples
//    printf(SCOLOR SPREFIX " Succes using styles! " SSUFIX RESETCOLOR"\n");
//    printf(ECOLOR EPREFIX " Error using styles! " ESUFIX RESETCOLOR"\n");
//    printf(ICOLOR IPREFIX " Info using styles! " ISUFIX RESETCOLOR"\n");
//    printf(WCOLOR WPREFIX " Warning using styles! " WSUFIX RESETCOLOR"\n");
//    printf(TCOLOR TPREFIX " Title using styles! " TSUFIX RESETCOLOR"\n");
//    printf(STCOLOR STPREFIX " Subtitle using styles! " STSUFIX RESETCOLOR"\n");

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
#include "../styles.h"

 #define DEFAULT_PORT 1081
 #define DEFAULT_IP "127.0.0.1"

int main(int argc, char * argv[]) {
  int connSock, in, i, ret, flags;
  int *position;
  struct sockaddr_in servaddr;
  struct sctp_status status;
  struct sctp_sndrcvinfo sndrcvinfo;
  struct sctp_event_subscribe events;
  struct sctp_initmsg initmsg;
  uint8_t buffer[MAX_BUFFER + 1];

  clean(buffer);
  position = 0;

  int port;
  char address[16];
  if(argc == 3){    //Custom port and ip
    int port_in = atoi( argv[2] );
    if(validIpAddress(argv[1])){
      strcpy(address, argv[1]);
    }else{
      printf(ICOLOR IPREFIX " Bad IP argument, using default value. " ISUFIX RESETCOLOR"\n");
      strcpy(address, DEFAULT_IP);  //Localhost
    }
    if(port_in > 1024 && port_in < 60000){  //Valid ports
      port = port_in;
    }else{
      printf(ICOLOR IPREFIX " Bad PORT argument, using default value. " ISUFIX RESETCOLOR"\n");
      port = DEFAULT_PORT;
    }
  }else if(argc == 2){  //Custom port
    int port_in = atoi( argv[1] );
    strcpy(address, DEFAULT_IP);
    if(port_in > 1024 && port_in < 60000){  //Valid ports
      port = port_in;
    }else{
      printf(ICOLOR IPREFIX " Bad PORT argument, using default value. " ISUFIX RESETCOLOR"\n");
      port = DEFAULT_PORT;
    }
  }else{  //Default values
    if(argc == 1)
      printf(ICOLOR IPREFIX " No arguments, using default values. " ISUFIX RESETCOLOR"\n");
    else
      printf(ICOLOR IPREFIX " Bad arguments, using default values. " ISUFIX RESETCOLOR"\n");
    strcpy(address, DEFAULT_IP);
    port = DEFAULT_PORT;
  }
  printf(ICOLOR IPREFIX " Trying to connect to %s:%d " ISUFIX RESETCOLOR"\n", address, port);


  /* Create an SCTP TCP-Style Socket */
  connSock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);

  /* Specify that a maximum of 5 streams will be available per socket */
  memset(&initmsg, 0, sizeof(initmsg));
  initmsg.sinit_num_ostreams = 5;
  initmsg.sinit_max_instreams = 5;
  initmsg.sinit_max_attempts = 4;
  ret = setsockopt(connSock, IPPROTO_SCTP, SCTP_INITMSG,
                   &initmsg, sizeof(initmsg));

  /* Specify the peer endpoint to which we'll connect */
  bzero((void *) &servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = inet_addr(address);

  /* Connect to the server */
  ret = connect(connSock, (struct sockaddr *) &servaddr, sizeof(servaddr));

  if (ret < 0) {
    printf(ECOLOR EPREFIX " Can't connect. Check if the server is working property. " ESUFIX RESETCOLOR"\n");
    exit(0);
  }
  /* Enable receipt of SCTP Snd/Rcv Data via sctp_recvmsg */
  memset((void *) &events, 0, sizeof(events));
  events.sctp_data_io_event = 1;
  ret = setsockopt(connSock, SOL_SCTP, SCTP_EVENTS,
                   (const void *) &events, sizeof(events));

  /* Read and emit the status of the Socket (optional step) */
  in = sizeof(status);
  ret = getsockopt(connSock, SOL_SCTP, SCTP_STATUS,
                   (void *) &status, (socklen_t * ) & in);

  printf(TCOLOR TPREFIX " WELCOME TO J2M2 PROTOCOL CLIENT " TSUFIX RESETCOLOR"\n");
  printf(STCOLOR STPREFIX "        Please login        " STSUFIX RESETCOLOR"\n");
  // Start J2M2 Logic

 while(1){
  if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
    printf(ECOLOR EPREFIX " No characters read " ESUFIX RESETCOLOR"\n");
  }

  char first[MAX_BUFFER] = {0};
  char second[MAX_BUFFER] = {0};
  char third[MAX_BUFFER] = {0};
  sscanf(buffer, "%s %s %s", first, second, third);

  if (strcmp(first, "login") == 0) {
    if (handleLogin(second, third, connSock)) {
      while (1) {
        first[0] = NULL;
        second[0] = NULL;
        third[0] = NULL;
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
          printf(ECOLOR EPREFIX " No characters read " ESUFIX RESETCOLOR"\n");
          break;
        }
        sscanf(buffer, "%s %s %s", first, second, third);
        if (strcmp(first, "metric") == 0) {
          if (!handleMetric(second, third, connSock)) {
            continue;
          }
        } else if (strcmp(first, "config") == 0) {
          if (!handleConfig(second, third, connSock)) {
            continue;
          }
        } else if (strcmp(first, "help") == 0) {
          if (!handleHelp(second, third, connSock)) {
            continue;
          }
        } else if (strcmp(first, "exit") == 0) {
          if (!handleExit(second, third, connSock)) {
            continue;
          }
        } else {
          printf(ECOLOR EPREFIX " Bad Request " ESUFIX RESETCOLOR"\n");
          continue;
        }
        // Recieve response
        flags = 0;
        uint8_t resp[MAX_BUFFER];
        ret = sctp_recvmsg(connSock, (void *) resp, MAX_DATAGRAM_SIZE,
                           (struct sockaddr *) NULL, 0, &sndrcvinfo, &flags);
        if (resp[3] == 1) {
          //Response statuts OK accepted
          printf(ICOLOR IPREFIX ICOLOR"---Start Message---" ISUFIX "\n" SCOLOR "%s" ICOLOR IPREFIX "---End Message---"ISUFIX RESETCOLOR"\n",
                 &resp[4]);
          if (resp[0] == 3 && resp[1] == 2) {
            close(connSock);
            exit(0);
          }
        } else {
          printf(ECOLOR EPREFIX " Error code %i\n " ESUFIX RESETCOLOR"\n", resp[3]);
        }
        clean(resp);
      }
    } else {
      printf(ECOLOR EPREFIX " Invalid login. " ESUFIX RESETCOLOR"\n");
    }
  } else {
    printf(ECOLOR EPREFIX " Please login property. " ESUFIX RESETCOLOR"\n");
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

int validIpAddress(char * ipAddress)
{
  struct sockaddr_in sa;
  int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
  return result != 0;
}