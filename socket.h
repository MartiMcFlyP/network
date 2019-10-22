#ifndef SRC_SOCKET_H
#define SRC_SOCKET_H
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

const char * decode_address(const char *address, struct sockaddr_in6 *rval);
struct addrinfo hints;
struct addrinfo *res;
int s;

int create_socket(struct sockaddr_in6 *source_addr, int src_port, struct sockaddr_in6 *dest_addr,int dst_port);
int sockfd;


int wait_for_client(int sfd);
struct sockaddr_in6 caller_addr;
char buf[1024];


#endif //SRC_SOCKET_H
