#include "socket.h"
#include "packet_interface.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* * sockaddr_in6 */
#include <netdb.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

char *buffer_payload[MAX_PAYLOAD_SIZE]; //Permet de stocker les payload recu
int buffer_len[MAX_WINDOW_SIZE]; //Permet de stocker la taille des payload recu

void receive_data(char* hostname, int port, int N);

void add_buffer(int index, int seq_rcv, int seq_exp, pkt_t * pkt_rcv, int window);

int send_ack(pkt_t *pkt_ack, int seqnum, int socket_fd, int ack, uint32_t time_data, int window);
