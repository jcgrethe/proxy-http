#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define SERVER_PORT  12345
#define TRUE             1
#define FALSE            0


int main(int args, char* argv[]){
   int    i, len, rc, on = 1;
   int    active_socket, max_sd, new_sd, desc_ready;
   char   buffer[1025];
   struct sockaddr_in   addr;
   struct fd_set        master_set, working_set;

   /* Creating an Active Socket to listen requests.*/
   active_socket = socket(AF_INET, SOCK_STREAM, 0);
   if (active_socket < 0){
      perror("Error opening active socket.\n");
      exit(EXIT_FAILURE);
   }

   /* Active Socket must be reusabe. */
   if (setsockopt(active_socket, SOL_SOCKET,  SO_REUSEADDR, (char *)&on, sizeof(on)) < 0){
      perror("setsockopt()\n");
      close(active_socket);
      exit(EXIT_FAILURE);
   }

   /* Setting Active socket as non-blocking. */
   rc = fcntl(active_socket, F_SETFL, O_NONBLOCK);
   if (rc < 0){
      perror("Error setting Active Socket as non-blocking.\n");
      close(active_socket);
      exit(EXIT_FAILURE);
   }

   // /* Setting Active socket as not astyncrhonus I/O . */
   // rc = fcntl(active_socket, F_SETFL, O_ASYNC);
   // if (rc < 0){
   //    perror("Error setting Active Socket as non-blocking.\n");
   //    close(active_socket);
   //    exit(EXIT_FAILURE);
   // }

   /* Let`s bind our Active Socket to a port. */
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port        = htons(SERVER_PORT);

   if (bind(active_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0){
      perror("Error binding Active Socket.\n");
      close(active_socket);
      exit(EXIT_FAILURE);
   }

   /* Let`s set our Active Socket to listen */
   rc = listen(active_socket, 32);
   if (rc < 0){
      perror("Error setting Active Socket to listen.\n");
      close(active_socket);
      exit(EXIT_FAILURE);
   }

   /* Adding active socket to fd_master so select can recognize it. **/
   FD_ZERO(&master_set);
   max_sd = active_socket;
   FD_SET(active_socket, &master_set);

   while(TRUE){
      memcpy(&working_set, &master_set, sizeof(master_set));
      printf("Waiting for connection...\n");

      rc = select(max_sd + 1, &working_set, NULL, NULL, NULL);

      if (rc <= 0){ //rc = 0, only if timeout was setted. In this case it will never be 0.
         perror("Select error.\n");
         break;
      }
      /* If sc > 0, there are one or more file descriptors to work with. */
      desc_ready = rc;
      for (i=0; i <= max_sd  &&  desc_ready > 0; ++i){
         /* Checking if this is available to work. */
         if (FD_ISSET(i, &working_set)){
            desc_ready -= 1;
            if (i == active_socket){
               /* This is Active Socket. So wait for connections. */
               do{
                  new_sd = accept(active_socket, NULL, NULL);
                  if (new_sd < 0){
                     if (errno != EWOULDBLOCK){
                        perror("Not accepting connection.\n");
                     }
                     break;
                  }
                  printf("New connection - %d\n", new_sd);
                  /* The connection is added to master_set*/
                  FD_SET(new_sd, &master_set);
                  /* Select needs the max fd + 1. */
                  if (new_sd > max_sd)
                     max_sd = new_sd;
               } while (new_sd != -1);
            }
            else{
                  rc = recv(i, buffer, sizeof(buffer), 0);
                  if (rc < 0){
                     if (errno != EWOULDBLOCK){
                        perror("recv() error.\n");
                     }
                     break;
                  }
                  if (rc == 0){
                     printf("Bye bye connection!\n");
                     break;
                  }
                  int pid = fork();
                  if(pid == 0){
                     handleConnection(buffer);
                  }
            } 
         } 
      }
   }
}

int handleConnection(char* buffer){
   printf("HOLAA FORKKK FOKKRR\n");
}