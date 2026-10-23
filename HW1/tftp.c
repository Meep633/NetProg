#include "tftp.h"

// globals to use in alarmHandler()
/*volatile*/ int gFd;
/*volatile*/ FILE *gFile;
/*volatile*/ struct sockaddr_in *gCliAddr;
/*volatile*/ char gPacket[516];
/*volatile*/ size_t gBytesRead;
/*volatile*/ sig_atomic_t gTransmitCount = 0;

unsigned short getOpcode(char *packet) {
  return ntohs(*((unsigned short *)packet));
}
unsigned short getBlock(char *packet) {
  return ntohs(*((unsigned short *)(packet+2)));
}

void alarmHandler(int signum) {
  // abort if retransmitted 10 times (or more somehow)
  if (gTransmitCount >= 10) {
    fprintf(stderr, "Connection timed out. Aborting...\n");
    fclose(gFile);
    close(gFd);
    exit(EXIT_FAILURE);
  }
  gTransmitCount++;

  // retransmit packet
  if (sendto(gFd, gPacket, 4+gBytesRead, 0, (struct sockaddr *)gCliAddr, (socklen_t)sizeof(*gCliAddr)) == -1) {
    perror("sendto() failed");
    fclose(gFile);
    close(gFd);
    exit(EXIT_FAILURE);
  }
  alarm(1);
}

void sendError(int fd, struct sockaddr_in *cliAddr, char *packet, unsigned short err, char *msg) {
  int ind = 0;
  unsigned short opcode = htons(5);
  err = htons(err);

  memcpy(packet, &opcode, sizeof(unsigned short));
  ind += 2;
  memcpy(packet+ind, &err, sizeof(unsigned short));
  ind += 2;

  if (msg != NULL) {
    memcpy(packet+ind, msg, strlen(msg));
    ind += strlen(msg);
  }
  packet[ind] = 0;
  ind++;

  // check if you should wait or not to see if client received error msg
  if (sendto(fd, packet, ind, 0, (struct sockaddr *)cliAddr, (socklen_t)sizeof(*cliAddr)) == -1) {
    perror("sendto() failed");
  }
  close(fd);
  exit(EXIT_FAILURE);
}

void handleRRQ(int fd, struct sockaddr_in *cliAddr, in_port_t expectedPort, char *packet, char *filename) {
  // check if file exists and can be read
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    sendError(fd, cliAddr, packet, 2, "File cannot be read");
  }
  gFile = file;

  unsigned short opcode = htons(3);
  unsigned short block = 1;
  socklen_t cliAddrLen = sizeof(*cliAddr);
  while (1) {
    //send data
    unsigned short nBlock = htons(block);
    memcpy(packet, &opcode, sizeof(unsigned short));
    memcpy(packet+2, &nBlock, sizeof(unsigned short));
    size_t bytesRead = fread(packet+4, sizeof(char), 512, file);
    if (bytesRead == 0 && ferror(file)) {
      fclose(file);
      sendError(fd, cliAddr, packet, 0, "fread() failed");
    }

    if (sendto(fd, packet, 4+bytesRead, 0, (struct sockaddr *)cliAddr, (socklen_t)sizeof(*cliAddr)) == -1) {
      perror("sendto() failed");
      fclose(file);
      close(fd);
      exit(EXIT_FAILURE);
    }
    memcpy(gPacket, packet, 4+bytesRead); //copy to global for any retransmits
    gBytesRead = bytesRead;
    alarm(1);

    //receive ack
    while (1) {
      cliAddr->sin_port = expectedPort;
      size_t bytesRcvd = recvfrom(fd, packet, 4, 0, (struct sockaddr *)cliAddr, &cliAddrLen);
      if (bytesRcvd == -1 && errno != EINTR) {
        perror("recvfrom() failed");
        fclose(file);
        close(fd);
        exit(EXIT_FAILURE);
      } else if (bytesRcvd == -1) { //recvfrom was interrupted by alarm, try again
        continue;
      }
    
      //check if packet is ACK with correct block and client port
      if (cliAddr->sin_port != expectedPort || bytesRcvd != 4)
        continue;
      unsigned short rcvdOpcode = getOpcode(packet);
      unsigned short rcvdBlock = getBlock(packet);
      if (rcvdOpcode == 4 && rcvdBlock == block)
        break;
    }
    
    alarm(0); //remove alarm
    gTransmitCount = 0; //reset retransmit count for next block
    block++;
    if (bytesRead < 512) break; //final block
  }

  fclose(file);
}

void handleWRQ(int fd, struct sockaddr_in *cliAddr, in_port_t expectedPort, char *packet, char *filename) {
  // check if file can be overwritten / created
  FILE *file = fopen(filename, "wb");
  if (file == NULL) {
    sendError(fd, cliAddr, packet, 2, "File cannot be written to");
  }
  gFile = file;

  unsigned short opcode = htons(4);
  unsigned short block = 0;
  socklen_t cliAddrLen = sizeof(*cliAddr);
  size_t bytesRcvd = 516;
  gBytesRead = 0;
  while (1) {
    // send ack
    unsigned short nBlock = htons(block);
    memcpy(packet, &opcode, sizeof(unsigned short));
    memcpy(packet+2, &nBlock, sizeof(unsigned short));

    if (sendto(fd, packet, 4, 0, (struct sockaddr *)cliAddr, (socklen_t)sizeof(*cliAddr)) == -1) {
      perror("sendto() failed");
      fclose(file);
      close(fd);
      exit(EXIT_FAILURE);
    }
    if (bytesRcvd < 516) break;
    memcpy(gPacket, packet, 4); //copy to global for any retransmits
    block++;
    alarm(1);

    // receive data
    while (1) {
      cliAddr->sin_port = expectedPort;
      bytesRcvd = recvfrom(fd, packet, 516, 0, (struct sockaddr *)cliAddr, &cliAddrLen);
      if (bytesRcvd == -1 && errno != EINTR) {
        perror("recvfrom() failed");
        fclose(file);
        close(fd);
        exit(EXIT_FAILURE);
      } else if (bytesRcvd == -1) { //recvfrom was interrupted by alarm, try again
        continue;
      }

      if (cliAddr->sin_port != expectedPort || bytesRcvd < 4 || bytesRcvd > 516)
        continue;
      unsigned short rcvdOpcode = getOpcode(packet);
      unsigned short rcvdBlock = getBlock(packet);
      if (rcvdOpcode == 3 && rcvdBlock == block)
        break;
    }

    alarm(0); //remove alarm
    gTransmitCount = 0; //reset retransmit count for next block
    if (fwrite(packet+4, 1, bytesRcvd-4, file) != bytesRcvd-4) {
      fclose(file);
      sendError(fd, cliAddr, packet, 0, "fwrite() failed");
    }
  }

  fclose(file);
}

void tftpConnection(int fd, struct sockaddr_in *cliAddr, char *packet, ssize_t bytesRcvd) {
  // check if packet is a valid start to the connection (is RRQ/WRQ)
  if (bytesRcvd <= 9) {
    sendError(fd, cliAddr, packet, 4, "Connection must begin connection with RRQ/WRQ");
  }
  unsigned short opcode = getOpcode(packet);
  if (opcode != 1 && opcode != 2) {
    sendError(fd, cliAddr, packet, 4, "Connection must begin connection with RRQ/WRQ");
  }

  // read filename and mode and check if mode == octet
  char filename[101];
  strcpy(filename, packet + sizeof(unsigned short));
  char mode[6];
  strcpy(mode, packet + sizeof(unsigned short) + strlen(filename) + 1);
  if (strcmp(mode, "octet") != 0) {
    sendError(fd, cliAddr, packet, 0, "Mode must be \"octet\"");
  }

  // set alarm handler
  Signal(SIGALRM, alarmHandler);
  gFd = fd;
  gCliAddr = cliAddr;

  // rrq/wrq loop
  if (opcode == 1)
    handleRRQ(fd, cliAddr, cliAddr->sin_port, packet, filename);
  else
    handleWRQ(fd, cliAddr, cliAddr->sin_port, packet, filename);
  
  close(fd);
  exit(EXIT_SUCCESS);
}