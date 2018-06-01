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



int handleLogin(uint8_t * second, uint8_t * third, int connSock){
  uint8_t type = 0, command = 0, argsq = 2, code = 0;
  uint8_t datagram[MAX_DATAGRAM];
  struct sctp_sndrcvinfo sndrcvinfo = {0};
  int secondLenght = strlen(second), thirdLenght = strlen(third), ret, flags, in;
  int datagramSize = 4 + secondLenght + 1 + thirdLenght;
  struct sctp_status status;
  
  if(second == NULL || third == NULL){
    printf("Invalid parameters.\n");
    return -1;
  } 

  datagram[0] = type;
  datagram[1] = command;
  datagram[2] = argsq;
  datagram[3] = code;
  datagram[4] = second;
  memcpy(&datagram[4], second, secondLenght);
  datagram[4 + secondLenght] = ' ';
  memcpy(&datagram[4 + secondLenght + 1], third, thirdLenght);
  datagram[4 + secondLenght + thirdLenght + 1] = "\0"; 
  // Send login request

  printf("...Sending Login...");
  ret = sctp_sendmsg( connSock, (const void *)datagram, datagramSize,
                         NULL, 0, 0, 0, STREAM, 0, 0 ); 

  printf("OK!\n");                        

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
    printf("Welcome!\n");
    return 1;
  }
  return 0;
}

int handleMetric(char * second, char * third, int connSock){
  char type = 1, command, argsq, code = 0, ret;
  uint8_t datagram[MAX_DATAGRAM];
  
  if (*third != NULL){
    printf("Invalid params. %s\n", third);
    return 0;
  }
  if (*second == NULL){
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
    }else if(strcmp(second, "timealive") == 0){
      command = 4;
    }else{
      printf("Invalid metric.\n");
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
  char type = 2, command = 0, argsq = 0, code = 0, ret, lenght = 4;
  uint8_t datagram[MAX_DATAGRAM];
  
  if(*second == NULL && *third == NULL){
    argsq = 0;
  }else{
    argsq = 1;
    if(strcmp(second, "currcon") == 0){
      command = 0;
    }else if(strcmp(second, "buffsize") == 0){
      command = 1;
    }else if(strcmp(second, "timeout") == 0){
      command = 2;
    }else{
      printf("Invalid config.\n");
      return 0;
    }    
    if (*third != NULL){
      argsq = 2;
      memcpy(&datagram[4], third, strlen(third));
      lenght = lenght + strlen(third);
    }
  }
  datagram[0] = type;
  datagram[1] = command;
  datagram[2] = argsq;
  datagram[3] = code;

  ret = sctp_sendmsg( connSock, (const void *)datagram, lenght,
                         NULL, 0, 0, 0, STREAM, 0, 0 );
  if(ret > 1){
    return 1;
  }
  return 0;  
}

int handleHelp(char * second, char * third, int connSock){
  char type = 3, command = 0, argsq = 0, code = 0;
  if(second != NULL){
    printf("Help function does not allow params!\n");
    return 0;
  }
  return 1;
  //Send
}

// void handleBlock(char * second, char * third, int connSock){
//   char type = 0b0003, command = 1, argsq = 0, code = 0;
//   if(second != NULL || third != NULL){
//     printf("Block function does not allow params!\n");
//     return;
//   }  

//   //Send
// }

int handleExit(char * second, char * third, int connSock){
  char type = 3, command = 2, argsq = 0, code = 0, ret;
  uint8_t datagram[MAX_DATAGRAM];
 
  if(second != EOF){
    printf("Exit function does not allow params!\n");
    return 0;
  }  

  datagram[0] = type;
  datagram[1] = command;
  datagram[2] = argsq;
  datagram[3] = code;
  ret = sctp_sendmsg( connSock, (const void *)datagram, 4,
                         NULL, 0, 0, 0, STREAM, 0, 0 );    
  //Close connection
  close(connSock);
  exit(0);
}

void printDatagram(uint8_t * datagram, int size){
  //Print Datagram for debbuging
  printf("Datagram: ");
  for (int i = 0; i < size; ++i)
    printf("[%d]%02x[%c]-",i, datagram[i], datagram[i]);
  printf("\n");
}