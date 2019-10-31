
  
  #include "socket.h"
#include "packet_implement.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>








/*A FAIRE ! 
selective repeat
acq seq
calcul CRC
commentaires
*/




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

    char buff[20];
    size_t buf_len = 20;

    //Encodage du ACK/NACK et remplissage de la variable buf
    return_status = pkt_encode_ack(pkt_ack, buff, &buf_len);
    if(return_status != PKT_OK){
        perror("Encodage de l'acknowledge");
        return -1;
    }

    //On envoie le ack/nack encodé
    if(write(socket_fd, buff, buf_len) < 0)
    {
        perror("Encodage write ack");
        return -1;
    }

    return 0;
}


void receiver(char* hostname, int port, int N, char* format){

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    //Récupération  l'address en IPV6
    const char* ret_real = decode_address(hostname, &addr);

    if (ret_real != NULL){
        fprintf(stderr, "Address '%s' is not recognized.", hostname);
        return;
    }

    //Création du socket serveur
    int socket_fd = create_socket(&addr, port, NULL, 0);
    if(socket_fd == -1){
        return;
    }


    //Permet de lire les packet recu
    pkt_t* pkt_rcv;
    //Permet d'envoyer des ACK/NACK
    pkt_t* pkt_ack;

    memset(buffer_len,-1,MAX_WINDOW_SIZE);
    char packet_encoded[528];


    pkt_rcv = pkt_new();
    if(pkt_rcv == NULL){
        fprintf(stderr, "Erreur creation packet");
        pkt_del(pkt_rcv);
        return;
    }

    pkt_ack = pkt_new();
    if(pkt_ack == NULL){
        fprintf(stderr, "Erreur creation packet");
        pkt_del(pkt_ack);
        pkt_del(pkt_rcv);
        return;
    }

    //ouverture du fichier de sortie     
    char file[sizeof(format)];
    sprintf(file,format,0); //sends formatted output to a string pointed to, by str
    int fd = open((const char *)file, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);
    if(fd == -1){
        fprintf(stderr, "An occur failed while opening fd.");
        return;
    }

    fd_set readset;     //liste de fd
    

    int window = 10;
    int index = 0; //Utilisé pour gerer les indice du buffer
    int listen_socket = listen(socket_fd,N);
    int activity;
    int sender_socket [N];
    struct sockaddr_in client_address;
    int seq_attendue[N]; //seq attendue

    char **buffer_payload[N][MAX_PAYLOAD_SIZE]; //Permet de stocker les payload recu
    int *buffer_len[N][MAX_WINDOW_SIZE]; //Permet de stocker la taille des payload recu
    int *buffer_seq[N][MAX_WINDOW_SIZE]; // Permet de stocker les num de seq attendues

    while (1) {


        FD_ZERO(&readset);  //on le met à 0 
        FD_SET(socket_fd, &readset);    //on ajoute le socket serveur

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select(socket_fd + N-1 , &readset , NULL , NULL , NULL);
   
        if ((activity == -1) && (errno == EINTR)) 
        {
            continue;
        }
        
        if (activity == -1) {
            perror("probleme select()");
            exit(EXIT_FAILURE);
        }

        //If something happened on the  socket , then its an incoming connection
        if (FD_ISSET(socket_fd, &readset)) {
            int new_socket = accept(socket_fd, (struct sockaddr *) &client_address, sizeof(client_address));
            sender_socket[i] == new_socket;
            FD_SET(new_socket, &readset);
        }

        for( int i = 0; i < N; i++){
            if(FD_ISSET(sender_socket[i]), &readset){
            
                int reader = recv(sender_socket[i], packet_encoded, 528, 0);
                if (reader == -1) {
                    close(sender_socket[i]);
                }


                else if(reader >0){
                    //Décodage du packet(on a besoin que de packet contenant de la data)
                    if(pkt_decode((const char*)packet_encoded,(int)reader,pkt_rcv) == PKT_OK && pkt_get_type(pkt_rcv) == PTYPE_DATA){
                        int seq_rcv = pkt_get_seqnum(pkt_rcv);

                        if(pkt_get_tr(pkt_rcv) == 1){ //paquet tronqué donc envoi de NACK
                            if(send_ack(pkt_ack,seq_rcv,socket_fd, PTYPE_NACK, pkt_get_timestamp(pkt_rcv),window) < 0){
                                fprintf(stderr,"Error sending nack");
                            }
                        }

                        else{
                            
                            int leng = pkt_get_length(pkt_rcv);

                            if(leng == 0)
                            {
                                printf("Fermeture du socket [%d]\n", i);
                                if(close(sender_socket[i]) == -1){
                                    fprintf(stderr,"Erreur fermeture socket");
                                }
                                //tt remettre à 0
                            }

                            else
                            {
                                if(seq_rcv < (seq_attendue[i] + window)){
                                    if(seq_attendue[i] == seq_rcv){
                                        write(fd,pkt_get_payload(pkt_rcv), pkt_get_length(pkt_rcv));
                                        if(send_ack(pkt_ack,seq_rcv,socket_fd, PTYPE_NACK, pkt_get_timestamp(pkt_rcv),window) < 0){
                                            fprintf(stderr,"Error sending nack");
                                        }
                                        seq_attendue[i] ++;
                                        int j;
                                        for(j = 0; j < 2*window; j ++){
                                            if(buffer_seq[i][j%window] == seq_attendue[i]){
                                                write(fd,buffer_payload[i][j%window],buffer_len[i][j%window]);
                                                if(send_ack(pkt_ack,seq_rcv,socket_fd, PTYPE_NACK, pkt_get_timestamp(pkt_rcv),window) < 0){
                                                    fprintf(stderr,"Error sending nack");
                                                }
                                                buffer_payload[i][j%window] = NULL;
                                                buffer_len[i][j%window] = -1;
                                                buffer_seq[i][j%window] = -1;
                                                seq_attendue[i] ++;
                                            }
                                        }
                                    }

                                    else{
                                        buffer_len[i][(seq_rcv%window)] = pkt_get_length(pkt_rcv);
                                        buffer_payload[i][(seq_rcv%window)] = pkt_get_payload(pkt_rcv);
                                        buffer_seq[i][seq_rcv%window] = pkt_get_seqnum(pkt_rcv);
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    if(close(sender_socket[i]) == -1){
                        fprintf(stderr,"Erreur fermeture socket");
                    }
                }
            }
        }
    }  
}


int main(int argc, char **argv) {
    int opt;
    int port;
    int N = 1;
    char *format = NULL;
    char *hostname;
    while ((opt = getopt(argc, argv, "o:m:")) != -1) {
        switch (opt) {
            case 'o':
                format = optarg;
                break;
            case 'm':
                N = atoi(optarg);
                break;
            case '?':
                if(optopt == 'c') {
                    fprintf(stderr, "Option -%c doit prendre un argument.\n", optopt);
                }
                else{
                    fprintf(stderr, "caractère d'option inconnue '\\x%x'.\n",optopt);
                }
            default: /* */
                abort();
        }
    }
    if (optind+2 != argc) {
        fprintf(stderr, "Nombre d'arguments incorrect\n");
        exit(EXIT_FAILURE);
    }
    hostname = argv[optind];
    port = atoi(argv[optind+1]);
    receiver(hostname, port, N, format);
    exit(EXIT_SUCCESS);
}




