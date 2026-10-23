#ifndef __TFTP__
#define __TFTP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "unp.h"

#define MAX_RQ_SIZE 109 //9 bytes for opcode, delims, and octet, 100 bytes for filename

void tftpConnection(int fd, struct sockaddr_in *cliAddr, char *packet, ssize_t bytesRcvd);

#endif