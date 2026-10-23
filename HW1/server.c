#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "tftp.h"

int validInt(char *port) { //not sure what ports will be used on submitty so won't check for that
  for (int i = 0; i < strlen(port); i++) {
    if (!isdigit(port[i])) 
      return 0;
  }
  return 1;
}

int main(int argc, char **argv) {
  // check arguments
  if (argc != 3) {
    fprintf(stderr, "Invalid number of arguments\n");
    return EXIT_FAILURE;
  }
  if (!validInt(argv[1]) || !validInt(argv[2])) {
    fprintf(stderr, "%s and/or %s are invalid ports\n", argv[1], argv[2]);
    return EXIT_FAILURE;
  }
  int startPort = atoi(argv[1]);
  int endPort = atoi(argv[2]);

  // create socket + bind
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {
    perror("socket() failed");
    return EXIT_FAILURE;
  }

  struct sockaddr_in servAddr, cliAddr;
  socklen_t cliSize = sizeof(cliAddr);
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(startPort);
  startPort++;

  if (bind(fd, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1) {
    perror("bind() failed");
    return EXIT_FAILURE;
  }

  // accept clients until no more ports remain
  char packet[516]; //max packet size = DATA message
  while (startPort <= endPort) {
    ssize_t bytesRcvd = recvfrom(fd, packet, MAX_RQ_SIZE, 0, (struct sockaddr *)&cliAddr, &cliSize);
    pid_t pid = fork();
    if (pid == 0) {
      //open new socket for child process
      close(fd);
      fd = socket(AF_INET, SOCK_DGRAM, 0);
      if (fd == -1) {
        perror("socket() failed");
        return EXIT_FAILURE;
      }
      servAddr.sin_port = htons(startPort);
      if (bind(fd, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1) {
        perror("bind() failed");
        return EXIT_FAILURE;
      }
      
      tftpConnection(fd, &cliAddr, packet, bytesRcvd); //try to find/write a tftp client to see if serv port is correct
    } else if (pid == -1) {
      perror("fork() failed");
      return EXIT_FAILURE;
    }
    startPort++;
  }

  // end child process (parent should reclaim it i think - sigchld handler?)
  
  close(fd);
  return EXIT_SUCCESS;
}
