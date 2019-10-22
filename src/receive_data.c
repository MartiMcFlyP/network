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


/* !!! Attention, ce code est grandement inspiré d'un code trouvé sur internet !!! */

char *buffer_payload[MAX_PAYLOAD_SIZE]; //Permet de stocker les payload recu
int buffer_len[MAX_WINDOW_SIZE]; //Permet de stocker la taille des payload recu

void receive_data(char* hostname, int port, int N){

    struct sockaddr_in6 addr;
	memset(&addr, 0, sizeof(addr));
	//Récupération  la real address en IPV6
	const char* ret_real = real_address(hostname, &addr);

	if (ret_real != NULL){
		fprintf(stderr, "Address '%s' is not recognized.", hostname);
		return;
	}

	//Création du socket en fournissant l'adresse et le port du receiver fournit par l'utilisateur
	int socket_fd = create_socket(&addr, port, NULL, 0);

	if(socket_fd == -1){
		return;
    }

	//On attend une connection
	int wait = wait_for_client(socket_fd);

	if(wait == -1){
		return;
    }

    //Permet de lire les packet recu
	pkt_t* pkt_rcv;
	//Permet d'envoyer des ACK/NACK
	pkt_t* pkt_ack;


	struct timeval tv;	
	tv.tv_sec = 5;
    tv.tv_usec = 0;

	int window = 3;
	int index = 0; //Utilisé pour gerer les indice du buffer
	int seq_exp = 0; //Numéro de segment attendu

	memset(buffer_len,-1,MAX_WINDOW_SIZE);
	char packet_encoded[528];


    pkt_rcv = pkt_new();
	if(pkt_rcv == NULL){
		fprintf(stderr, "An occur failed while creating a data packet.");
		pkt_del(pkt_rcv);
		return;
	}

	pkt_ack = pkt_new();
	if(pkt_ack == NULL){
		fprintf(stderr, "An occur failed while creating a data packet.");
		pkt_del(pkt_ack);
		pkt_del(pkt_rcv);
		return;
	}
 	 

 	fd_set readset;

	FD_ZERO(&readset);
   	FD_SET(socket_fd, &readset);
   	int result = select(socket_fd + N-1, &readset, NULL, NULL, &tv);

	while (result == -1 && errno == EINTR){

		if (result > 0) {
			if (FD_ISSET(socket_fd, &readset)) {
				/* The socket_fd has data available to be read */
				result = read(socket_fd, packet_encoded, 528);

				if (result == 0) {
					/* This means the other side closed the socket */
					close(socket_fd);
				}

				else if(result < 0){
					pkt_del(pkt_rcv);
					pkt_del(pkt_ack);
				}

				else if(result >0);
				//Décodage du packet(on a besoin que de packet contenant de la data)
				err = 
            	if(pkt_decode((const char*)packet_encoded,(int)result,pkt_rcv) == PKT_OK && pkt_get_type(pkt_rcv) == PTYPE_DATA){
                	int seq_rcv = pkt_get_seqnum(pkt_rcv);

                	//Si tr == 1 => on envoie un NACK
                	if(pkt_get_tr(pkt_rcv) == 1){
                   	 	if(send_ack(pkt_ack,seq_rcv,socket_fd, PTYPE_NACK, pkt_get_timestamp(pkt_rcv),window) < 0){
                  	    	fprintf(stderr,"Error sending nack");
						}

              		}

             	   else{
                   	 //Si on recooit un pack de taille 0 -> Indique la fin du transfert
                  	  int leng = pkt_get_length(pkt_rcv);

                 	   if(leng == 0)
                 	   {
                	        printf("[[[ EOF RECEIVED ]]]\n");
                	    }

                 	   else
                 	   {
                        
						//Ajout du packet recu dans un buffer (pour gerer les cas ou on a recu
						//un packet avec un numéro de segment supérieur au numéro de segment attendu
						add_buffer(index, seq_rcv, seq_exp, pkt_rcv, window);
						//Ecriture de buffer et on le vide si le packet avec le bon numéro de segment attendu
						//a bien été reçu
						while (buffer_len[index] != -1){
							//Ecriture le pyaload des packets dans le fichier
							if(write(readset,buffer_payload[index],buffer_len[index]) < 0)
							{
								fprintf(stderr,"ERROR WRITING PACKET");
							}
							//Réinitialisation du payload écrit
							buffer_payload[index] = (char *)NULL;
							buffer_len[index] = -1;
							//On passe à l'index suivant (l'index ne peut jamais dépasser la window size).
							index = (index+1)%window;
							//Le numéro de séquence attendu est incrémenté
							seq_exp = (seq_exp+1)%256;
						}
						if(send_ack(pkt_ack,seq_exp-1,sfd, PTYPE_ACK, pkt_get_timestamp(pkt_rcv), window) < 0)
						{
							fprintf(stderr,"Error sending ack");
						}

					}
						
					}
				}
				else
				{
					printf("NACKNACK\n");
				}
				}
		}
		else if (result < 0) {
		/* An error ocurred, just print it to stdout */
		printf("Error on select(): %s\"", strerror(errno));
		}
		
	}

	
	close(socket_fd);
	close(readset);
	pkt_del(pkt_ack);
	pkt_del(pkt_rcv);
}



void add_buffer(int index, int seq_rcv, int seq_exp, pkt_t * pkt_rcv, int window){
	//On regarde si le num de segement recu est compris dans la window   && l  e numéro de segment recu est = ou > que le segment attendu (les numéros de segments inférieurs sont inutiles)
	if ((seq_rcv <= seq_exp + window-1 && seq_rcv >= seq_exp) || (seq_exp + window -1 > 256 && seq_rcv <= (seq_exp+window-1)%256)){
	//Exemple de cas à gérer :
	//Le numéro segment attendu est 255 et on recoit 1, ne passe pas dans notre première condition mais le packet recu est pourtant correct.
	//On regarde alors si le numéro de segment attendu + la window-1 dépasse 256   &&   si le numéro de segment recu est compris dans la window
	 // le segment peut etre contenu dans le buffer

		//On ajoute 256 pour ne pas fausser le calcul d'index du tableau
		if(seq_rcv < seq_exp) {
			seq_rcv +=  256;
		}
		//Calcul de l'indice
		//l'ajout de (seq_rcv - seq_exp) permet de placer le payload au bon indice du payload (cas ou le numérod segment recu > le numéro de segment attendu)
		int real_ind =(index + (seq_rcv - seq_exp))%window;
		//remplissage du buffer
		buffer_len[real_ind] = pkt_get_length(pkt_rcv);
		buffer_payload[real_ind] = (char *)pkt_get_payload(pkt_rcv);
	}
}


int send_ack(pkt_t *pkt_ack, int seqnum, int socket_fd, int ack, uint32_t time_data, int window){

	pkt_status_code return_status;
	//Etablissement des valeurs du ack
	return_status = pkt_set_seqnum(pkt_ack, seqnum+1);

	if(return_status != PKT_OK){    //problème au set
		perror("Creation de l'acknowledge : ");
		return -1;
	}

	// type à PTYPE_ACK ou PTYPE_NACK 
	if(ack == PTYPE_ACK){
		return_status = pkt_set_type(pkt_ack, PTYPE_ACK);
	}

	else if(ack == PTYPE_NACK)
		return_status = pkt_set_type(pkt_ack, PTYPE_NACK);

	if(return_status != PKT_OK){        //problème au set
		perror("Creation de l'acknowledge : ");
		return -1;
	}
	return_status = pkt_set_timestamp(pkt_ack, time_data);


	return_status = pkt_set_window(pkt_ack, window);
	if(return_status != PKT_OK){
		perror("Creation de l'acknowledge : ");
		return -1;
	}

	//Les ack/nack n'ont pas de payload
	return_status = pkt_set_payload(pkt_ack, NULL, 0);
	if(return_status != PKT_OK){
		perror("Creation de l'acknowledge : ");
		return -1;
	}

	char buf[20];
	size_t buf_len = 20;
	
	//Encodage du ACK/NACK et remplissage de la variable buf
	return_status = pkt_encode_ack(pkt_ack, buf, &buf_len);
	if(return_status != PKT_OK){
		perror("Encodage de l'acknowledge");
		return -1;
	}

	//On envoie le ack/nack encodé
	if(write(socket_fd, buf, buf_len) < 0)
	{
		perror("Encodage write ack");
		return -1;
	}

	return 0;
}
