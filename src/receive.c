#include "socket.h"
#include "packet_interface.h"
#include "server.h"
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

char *buffer_payload[MAX_PAYLOAD_SIZE]; //Permet de stocker les payload recu
int buffer_len[MAX_WINDOW_SIZE]; //Permet de stocker la taille des payload recu

void receive(char* hostname, int port, char* file) {


	struct sockaddr_in6 real_addr;
	memset(&real_addr, 0, sizeof(real_addr));
	//Récupération  la real address en IPV6
	const char* ret_real = real_address(hostname, &real_addr);
	if (ret_real != NULL) {
		fprintf(stderr, "Address '%s' is not recognized.", hostname);
		return;
	}
	//Création du socket en fournissant l'adresse et le port du receiver fournit par l'utilisateur
	int sfd = create_socket(&real_addr, port, NULL, 0);

	if (sfd < 0)
		return;
	//On attend une connection
	int wait = wait_for_client(sfd);
	if (wait < 0)
		return;


	int fd;
	//Ouvrage du fichier si l'utilisateur en a fournit hein
	if (file != NULL)
		fd = open((const char *)file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
	else
		fd = STDOUT_FILENO;

	//Valeur du retransmission timer utilisé par défault
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	int endFile = 0; //Permet de savoir si on est à la fin du fichier
	int max_length = 0;
	//Permet de lire les packet recu
	pkt_t* pkt_rcv;
	//Permet d'envoyer des ACK/NACK
	pkt_t* pkt_ack;

	int window = 3;

	int index = 0; //Utilisé pour gerer les indice du buffer

	int seq_exp = 0; //Numéro de segment attendu
	memset(buffer_len, -1, MAX_WINDOW_SIZE);
	char packet_encoded[528];
	fd_set read_set;

	pkt_rcv = pkt_new();
	if (pkt_rcv == NULL) {
		fprintf(stderr, "An occur failed while creating a data packet.");
		pkt_del(pkt_rcv);
		return;
	}
	pkt_ack = pkt_new();
	if (pkt_ack == NULL) {
		fprintf(stderr, "An occur failed while creating a data packet.");
		pkt_del(pkt_ack);
		pkt_del(pkt_rcv);
		return;
	}
	//On continue à recevoir des messages dans que le sender n'a pas envoyé son packet indiquant la fin du transfert
	while (endFile == 0) {


		FD_ZERO(&read_set);
		FD_SET(sfd, &read_set);
		//calcul de la taille max entre les deux file directory
		max_length = (fd > sfd) ? fd + 1 : sfd + 1;
		//On considère que la variable peut être modifiée après l'appel de la fonction,
		//on utilise donc une autre structure.
		struct timeval newtv = tv;

		//Permet de gerer plusieurs entrées et sorties à la fois
		select(max_length, &read_set, NULL, NULL, &newtv);

		//Cas ou on a reçu un packet
		if (FD_ISSET(sfd, &read_set)) {
			//on lit le packet encodée recu
			int length = read(sfd, (void *)packet_encoded, 528);
			//Si taille == 0 , réception du packet qui confirme la fin de transmission
			if (length == 0) {
			}
			//Si taille < 0 => problème
			else if (length < 0) {
				printf("LENGTH NEG\n");
				pkt_del(pkt_rcv);
				pkt_del(pkt_ack);
				return;
			}
			//Si taille > 0 , packet recu valide
			else if (length > 0) {
				//Décodage du packet(on a besoin que de packet contenant de la data)
				if (pkt_decode((const char*)packet_encoded, (int)length, pkt_rcv) == PKT_OK && pkt_get_type(pkt_rcv) == PTYPE_DATA)
				{
					int seq_rcv = pkt_get_seqnum(pkt_rcv);

					//Si tr == 1 => on envoie un NACK
					if (pkt_get_tr(pkt_rcv) == 1) {
						if (send_ack(pkt_ack, seq_rcv, sfd, PTYPE_NACK, pkt_get_timestamp(pkt_rcv), window) < 0)
						{
							fprintf(stderr, "Error sending nack");
						}

					}
					else {
						//Si on recooit un pack de taille 0 -> Indique la fin du transfert
						int lg = pkt_get_length(pkt_rcv);
						if (lg == 0)
						{
							printf("[[[ EOF RECEIVED ]]]\n");
							endFile = 1;
						}
						else
						{

							//Ajout du packet recu dans un buffer (pour gerer les cas ou on a recu
							//un packet avec un numéro de segment supérieur au numéro de segment attendu
							add_buffer(index, seq_rcv, seq_exp, pkt_rcv, window);
							//Ecriture de buffer et on le vide si le packet avec le bon numéro de segment attendu
							//a bien été reçu
							while (buffer_len[index] != -1) {
								//Ecriture le pyaload des packets dans le fichier
								if (write(fd, buffer_payload[index], buffer_len[index]) < 0)
								{
									fprintf(stderr, "ERROR WRITING PACKET");
								}
								//Réinitialisation du payload écrit
								buffer_payload[index] = (char *)NULL;
								buffer_len[index] = -1;
								//On passe à l'index suivant (l'index ne peut jamais dépasser la window size).
								index = (index + 1) % window;
								//Le numéro de séquence attendu est incrémenté
								seq_exp = (seq_exp + 1) % 256;
							}
							if (send_ack(pkt_ack, seq_exp - 1, sfd, PTYPE_ACK, pkt_get_timestamp(pkt_rcv), window) < 0)
							{
								fprintf(stderr, "Error sending ack");
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
	}

	//On ne sait pas si le sender va recevoir le ACK du packet de fin de transfert, donc on l'envoie 10 fois
	int count = 0;
	while (count < 11)
	{
		if (send_ack(pkt_ack, seq_exp - 1, sfd, PTYPE_ACK, pkt_get_timestamp(pkt_rcv), window) < 0)
		{
			fprintf(stderr, "Error sending ack");
		}
		count++;
		printf("[[[ACK END SENT]]]\n");
	}

	close(sfd);
	close(fd);
	pkt_del(pkt_ack);
	pkt_del(pkt_rcv);
}


void add_buffer(int index, int seq_rcv, int seq_exp, pkt_t * pkt_rcv, int window) {
	//On regarde si le num de segement recu est compris dans la window   && le numéro de segment recu est = ou > que le segment attendu (les numéros de segments inférieurs sont inutiles)
	if ((seq_rcv <= seq_exp + window - 1 && seq_rcv >= seq_exp)
		//Exemple de cas à gérer :
		//Le numéro segment attendu est 255 et on recoit 1, ne passe pas dans notre première condition mais le packet recu est pourtant correct.
		//On regarde alors si le numéro de segment attendu + la window-1 dépasse 256   &&   si le numéro de segment recu est compris dans la window
		|| (seq_exp + window - 1 > 256 && seq_rcv <= (seq_exp + window - 1) % 256)) { // le segment peut etre contenu dans le buffer

			//On ajoute 256 pour ne pas fausser le calcul d'index du tableau
		if (seq_rcv < seq_exp) {
			seq_rcv += 256;
		}
		//Calcul de l'indice
		//l'ajout de (seq_rcv - seq_exp) permet de placer le payload au bon indice du payload (cas ou le numérod segment recu > le numéro de segment attendu)
		int real_ind = (index + (seq_rcv - seq_exp)) % window;
		//remplissage du buffer
		buffer_len[real_ind] = pkt_get_length(pkt_rcv);
		buffer_payload[real_ind] = (char *)pkt_get_payload(pkt_rcv);
	}
}



int send_ack(pkt_t *pkt_ack, int seqnum, int sfd, int ack, uint32_t time_data, int window) {

	pkt_status_code return_status;
	//Etablissement des valeurs du ack
	return_status = pkt_set_seqnum(pkt_ack, seqnum + 1);
	if (return_status != PKT_OK) {
		perror("Creation de l'acknowledge : ");
		return -1;
	}

	//On met le type à PTYPE_ACK ou PTYPE_NACK en fonction du paramètre passé en argument
	if (ack == PTYPE_ACK) {
		return_status = pkt_set_type(pkt_ack, PTYPE_ACK);
	}
	else if (ack == PTYPE_NACK)
		return_status = pkt_set_type(pkt_ack, PTYPE_NACK);
	if (return_status != PKT_OK) {
		perror("Creation de l'acknowledge : ");
		return -1;
	}
	return_status = pkt_set_timestamp(pkt_ack, time_data);


	return_status = pkt_set_window(pkt_ack, window);
	if (return_status != PKT_OK) {
		perror("Creation de l'acknowledge : ");
		return -1;
	}

	//Les ack/nack n'ont pas de payload
	return_status = pkt_set_payload(pkt_ack, NULL, 0);
	if (return_status != PKT_OK) {
		perror("Creation de l'acknowledge : ");
		return -1;
	}

	char buf[20];
	size_t buf_len = 20;
	//Encodage du ACK/NACK et remplissage de la variable buf
	return_status = pkt_encode(pkt_ack, buf, &buf_len);
	if (return_status != PKT_OK) {
		perror("Encodage de l'acknowledge");
		return -1;
	}
	//On envoie le ack/nack encodé
	if (write(sfd, buf, buf_len) < 0)
	{
		perror("Encodage write ack");
		return -1;
	}

	return 0;
}