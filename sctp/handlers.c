#ifndef MAX_DATAGRAM
  #define MAX_DATAGRAM 132
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common.h"

//TODO: Chekear si se pueden evitar algunos includes, ojo con funciones sctp.
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../styles.h"


int handleLogin(uint8_t * second, uint8_t * third, int connSock){
  uint8_t type = 0, command = 0, argsq = 2, code = 0;
  uint8_t datagram[MAX_DATAGRAM];
  struct sctp_sndrcvinfo sndrcvinfo = {0};
  int secondLenght = strlen(second), thirdLenght = strlen(third), ret, flags, in;
  int datagramSize = 4 + secondLenght + 1 + thirdLenght;
  struct sctp_status status;
  
  if(!second[0] || !third[0]){
    printf(ECOLOR EPREFIX " Invalid parameters. " ESUFIX RESETCOLOR"\n");
    return 0;
  } 

  datagram[0] = type;
  datagram[1] = command;
  datagram[2] = argsq;
  datagram[3] = code;
  memcpy(&datagram[4], second, secondLenght);
  datagram[4 + secondLenght] = ' ';
  memcpy(&datagram[4 + secondLenght + 1], third, thirdLenght);
  memcpy(&datagram[4 + secondLenght + thirdLenght + 1], "\0", 1);
  //  datagram[4 + secondLenght + thirdLenght + 1] = "\0";
  // Send login request

  printf(ICOLOR IPREFIX " ...Sending login... ");
  ret = sctp_sendmsg( connSock, (const void *)datagram, datagramSize,
                         NULL, 0, 0, 0, STREAM, 0, 0 ); 

  printf("OK!" ISUFIX RESETCOLOR"\n");

  /* Read and emit the status of the Socket (optional step) */
  in = sizeof(status);
  ret = getsockopt( connSock, SOL_SCTP, SCTP_STATUS,
                  (void *)&status, (socklen_t *)&in );

  // Recieve login response
  flags = 0;
  uint8_t resp[MAX_DATAGRAM];
  ret = sctp_recvmsg( connSock, (void *)resp, sizeof(resp),
                       (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags );

  if(resp[3] == 1){
    //Login accepted
    printf(SCOLOR SPREFIX " Welcome! " SSUFIX RESETCOLOR"\n");
    return 1;
  }
  return 0;
}

int handleMetric(char * second, char * third, int connSock){
  char type = 1, command, argsq, code = 0, ret;
  uint8_t datagram[MAX_DATAGRAM];
  
  if (third[0]){
    printf(ECOLOR EPREFIX " Invalid parameters. " ESUFIX RESETCOLOR"\n");
    return 0;
  }
  if (!second[0]){
    argsq = 0;
  }else{
    argsq = 1;
    if(strcmp(second, "currcon") == 0){
      command = 0;
    }else if(strcmp(second, "histacc") == 0){
      command = 1;
    }else if(strcmp(second, "trabytes") == 0){
      command = 2;
    }else if(strcmp(second, "connsucc") == 0){
      command = 3;
    }else{
      printf(ECOLOR EPREFIX " Invalid metric. " ESUFIX RESETCOLOR"\n");
      return 0;
    }
  }

  datagram[0] = type;
  datagram[1] = command;
  datagram[2] = argsq;
  datagram[3] = code;

  ret = sctp_sendmsg( connSock, (const void *)datagram, 4,
                         NULL, 0, 0, 0, STREAM, 0, 0 );
  if (ret > 1){
    return 1;
  }
  return 0;     
}

int handleConfig(char * second, char * third, int connSock){
  char type = 2, command = 0, argsq = 0, code = 0, ret, lenght = 4, thirdLenght = strlen(third);
  uint8_t datagram[lenght + thirdLenght + 1];
  
  if(strcmp(second, "transform") == 0){
    command = TRANSFORM;
  } else if(strcmp(second, "mediatypes") == 0){
    command = MEDIATYPES;
    argsq = 1;
  } else if(strcmp(second, "command") == 0){
    command = COMM;
    argsq = 1;
  } else{
    printf(ECOLOR EPREFIX " Invalid Config. " ESUFIX RESETCOLOR"\n");
    return 0;
  }    
  
  datagram[0] = type;
  datagram[1] = command;
  datagram[2] = argsq;
  datagram[3] = code;
  if (argsq == 1){
    lenght += thirdLenght;
    third[thirdLenght-1] = '\0';
    memcpy(&datagram[4], third, thirdLenght+1);
  }

  ret = sctp_sendmsg( connSock, (const void *)datagram, lenght + 1,
                         NULL, 0, 0, 0, STREAM, 0, 0 );
  if(ret > 1){
    return 1;
  }
  return 0;  
}

int handleHelp(){
  printf("\nWe are here to help you!\n");
  
  printf("        [Access]        \n\n");
  printf("login  [username][password].\n\n");

  printf("        [Configurations]        \n\n");
  printf("config  [transform | mediatypes [ARGS] | command [ARG]].\n\n");
  printf("   - config transform                   Activate/Desactivated Transformations.\n");
  printf("   - config mediatypes [ARGS]           Set a list of media-types.\n");  
  printf("   - config command [ARG]               Set a command for transformations.\n");

  printf("        [Metrics]        \n\n");
  printf("metric [NONE] | [currcon | histacc | trabytes | connsucc].\n");
  printf("   - metric            Shows all metrics.\n");
  printf("   - metric currcon    Shows current http connections.\n");
  printf("   - metric histacc    Shows historical access.\n");
  printf("   - metric trabytes   Shows transfered bytes.\n");
  printf("   - metric connsucc   Shows connections success.\n\n");

  printf("        [System]        \n\n");
  printf("help [NONE].\n");
  printf("exit [NONE].\n\n\n");
  return 0;
}

int handleExit(char * second, char * third, int connSock){
  char type = 3, command = 2, argsq = 0, code = 0, ret;
  uint8_t datagram[MAX_DATAGRAM];
 
  if(second[0]){
    printf(ECOLOR EPREFIX " Parameters not allowed. " ESUFIX RESETCOLOR"\n");
    return 0;
  }  

  datagram[0] = type;
  datagram[1] = command;
  datagram[2] = argsq;
  datagram[3] = code;
  ret = sctp_sendmsg( connSock, (const void *)datagram, 4,
                         NULL, 0, 0, 0, STREAM, 0, 0 );    
  //Close connection after recieving response
  return 1;
}

void printDatagram(uint8_t * datagram, int size){
  //Print Datagram for debbuging
  printf("Datagram: ");
  for (int i = 0; i < size; ++i)
    printf("[%d]%02x[%c]-",i, datagram[i], datagram[i]);
  printf("\n");
}