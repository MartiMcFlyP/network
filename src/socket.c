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

/* Resolve the resource name to an usable IPv6 address
 * @address: The name to resolve
 * @rval: Where the resulting IPv6 address descriptor should be stored
 * @return: NULL if it succeeded, or a pointer towards
 *          a string describing the error if any.
 *          (const char* means the caller cannot modify or free the return value,
 *           so do not use malloc!)
 */
const char * real_address(const char *address, struct sockaddr_in6 *rval){
  struct addrinfo hints;

  memset(&hints, 0, sizeof hints);
 
  hints.ai_family = AF_INET6;  //Type d'adresse : IPV6
  hints.ai_socktype = SOCK_DGRAM; 
  hints.ai_protocol = IPPROTO_UDP;  //Type de protocole : UDP
  hints.ai_flags = 0; //Pas besoin de flags spécifiques

  struct addrinfo *res;
  int err;
  //Transformation de l'adresse reçu en adresse IPV6 utilisable	
  err= getaddrinfo(address, NULL, &hints, &res);
  if(err != 0)
    return gai_strerror(err);
  //Cast en sockaddr IPV6
  struct sockaddr_in6 * resu = (struct sockaddr_in6 *)(res->ai_addr);
  *rval = *resu;
  //On a plus besoin de res
  freeaddrinfo(res);
  return NULL;
}

/* Creates a socket and initialize it
 * @source_addr: if !NULL, the source address that should be bound to this socket
 * @src_port: if >0, the port on which the socket is listening
 * @dest_addr: if !NULL, the destination address to which the socket should send data
 * @dst_port: if >0, the destination port to which the socket should be connected
 * @return: a file descriptor number representing the socket,
 *         or -1 in case of error (explanation will be printed on stderr)
 */
int create_socket(struct sockaddr_in6 *source_addr,
                 int src_port,
                 struct sockaddr_in6 *dest_addr,
                 int dst_port){
    int sockfd;
    int err;
	//Création du socket en UPV6, UDP, SOCK_DGRAM
      sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
      if(sockfd == -1){
        perror("erreur lors la création du socket : ");
        return -1;
      }
	//On bind l'adresse de source si il y en a une
    if(source_addr != NULL && src_port >0){ 
      source_addr->sin6_port = htons(src_port);
      err = bind(sockfd,(struct sockaddr *)source_addr, sizeof(struct sockaddr_in6));
      if(err != 0){
        perror("erreur de liaison du socket1 : ");
        return -1;
      }
    }
	//On se connecte à l'adresse de destination si il y en a une
    if(dest_addr != NULL && dst_port >0){
      dest_addr->sin6_port = htons(dst_port);
      err = connect(sockfd,(struct sockaddr *)dest_addr, sizeof(struct sockaddr_in6));
      if(err != 0){
        perror("erreur de connection du socket2 : ");
        return -1;
      }
    }
    return sockfd;
}

/* Block the caller until a message is received on sfd,
 * and connect the socket to the source addresse of the received message.
 * @sfd: a file descriptor to a bound socket but not yet connected
 * @return: 0 in case of success, -1 otherwise
 * @POST: This call is idempotent, it does not 'consume' the data of the message,
 * and could be repeated several times blocking only at the first call.
 */
int wait_for_client(int sfd){
  struct sockaddr_in6 their_addr;
  socklen_t sin_size = sizeof(struct sockaddr_in6);
  memset(&their_addr, 0, sin_size);
  char buf[1024];
  int err;
  //On attend la réception de message pour connaitre l'auteur du message	
  err = recvfrom(sfd, buf, sizeof(buf), MSG_PEEK, (struct sockaddr *)&their_addr, &sin_size);
  if(err == -1){
    perror("erreur lors de la reception du message : ");
    return -1;
  }
  //Connection du socket avec l'adresse source du message recue
  err = connect(sfd, (struct sockaddr *)&their_addr, sin_size);
  if(err == -1){
    perror("erreur lors de la connextion : ");
  }
  return err;
}