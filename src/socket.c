//
// Created by Admin on 21-10-19.
//
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

const char * decode_address(const char *address, struct sockaddr_in6 *rval) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;                                                                                 //adresse IPV6
    hints.ai_flags = 0;                                                                                         //pas de flags
hints.ai_protocol = IPPROTO_UDP;                                                                                //proctocole UDP
    hints.ai_socktype = SOCK_DGRAM;                                                                             //UDP utilise DGRAM
    struct addrinfo *res;
    int s = getaddrinfo(address, NULL, &hints, & res);                                                          //transforme l'adresse reçue en adresse IPV6/DATAGRAM
    if(s != 0){
        return gai_strerror(s);                                                                                 //renvoie l'erreur sous forme compréhensible
    }
    rval = (struct sockaddr_in6*)(res->ai_addr);
    freeaddrinfo(res);                                                                                          //on libère la mémoire
    return 0;
}

int create_socket(struct sockaddr_in6 *source_addr, int src_port, struct sockaddr_in6 *dest_addr,int dst_port){

    int sockfd;
    sockfd = socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);                                                   //crée un point de communication
    if (sockfd < 0){
        perror("socket creation failed");                                                               //renvoit une erreur si socket n'a pas fonctionne
        return -1;
    }

    if(source_addr != NULL && src_port > 0){
        source_addr->sin6_port = htons(src_port);                                                       //association du port
        if (bind(sockfd,(struct sockaddr *)source_addr, sizeof(struct sockaddr_in6)) == -1){            //affectation d'un nom a la socket
            perror("bind failed");
            return -1;
        }
    }

    if (dest_addr != NULL && dst_port >0){
        dest_addr->sin6_port = htons(dst_port);
        if (connect(sockfd,(struct sockaddr *)dest_addr, sizeof(struct sockaddr_in6)) == -1){           //connection a la socket
            perror("connection failed");
            return -1;
        }
    }

    return sockfd;
}
