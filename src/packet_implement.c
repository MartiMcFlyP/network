#include "packet_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <zlib.h>

/* Extra #includes */
/* Your code will be inserted here */

struct __attribute__((__packed__)) pkt {
    ptypes_t type;
    uint8_t tr :1;
    uint8_t window :5;
    uint16_t length;
    uint8_t seqnum;
    uint32_t timestamp;
    char *payload;
    uint32_t crc1;
    uint32_t crc2;
};


/* Alloue et initialise une struct pkt
 * @return: NULL en cas d'erreur */

pkt_t* pkt_new()
{
    pkt_t *new_pkt;
    new_pkt = (pkt_t*) calloc(1, sizeof(pkt_t));
    if (new_pkt == NULL){
        return NULL;
    }
    return new_pkt;
}

/**************************************************/
/* Libere le pointeur vers la struct pkt, ainsi que toutes les
 * ressources associees */
void pkt_del(pkt_t *pkt)
{
    if(pkt!=NULL){
        if(pkt->payload!=NULL){
            free(pkt->payload);
        }
        free(pkt);
    }


}

/**************************************************/
/*
 * Decode des donnees recues et cree une nouvelle structure pkt.
 * Le paquet recu est en network byte-order.
 * La fonction verifie que:
 * - Le CRC32 du header recu est le mÃƒÂªme que celui decode a la fin
 *   du header (en considerant le champ TR a 0)
 * - S'il est present, le CRC32 du payload recu est le meme que celui
 *   decode a la fin du payload
 * - Le type du paquet est valide
 * - La longueur du paquet et le champ TR sont valides et coherents
 *   avec le nombre d'octets recus.
 *
 * @data: L'ensemble d'octets constituant le paquet recu
 * @len: Le nombre de bytes recus
 * @pkt: Une struct pkt valide
 * @post: pkt est la representation du paquet recu
 *
 * @return: Un code indiquant si l'operation a reussi ou representant
 *         l'erreur rencontree.
 */
pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt){
    int taille_header;

    if(len == 0){
        return E_UNCONSISTENT;
    }
    if(len < 7){
        return E_NOHEADER;
    }
    uint8_t byte;
    ptypes_t type;
    memcpy(&byte,data,1);

    if((byte & 0b11000000)==0b01000000){
        type = PTYPE_DATA;
    }
    else if((byte & 0b11000000)==0b10000000){
        type = PTYPE_ACK;
    }
    else{
        type = PTYPE_NACK;
    }

    pkt_set_type(pkt, type); //Le type est represente sur les deux premiers bits de data

    memcpy(&byte,data,1);
    byte = ((byte<<2)>>7);
    pkt_set_tr(pkt, byte); //le paquet est tronque si le troisieme bit est a 1

    memcpy(&byte,data,1);
    pkt_set_window(pkt, (byte & 00011111)); //la window se situe sur les cinq derniers bit du premier byte

    memcpy(&byte,data,1);
    int l = (byte>>7); //la longueur est definie sur sept bits ou quinze dependant de la valeur de l, premier bit du second byte

    if(l==0){ //la longueur est definie sur sept bits, le header sur sept bytes
        taille_header = 7;

        memcpy(&byte,data+1,1); //le byte entier peut etre pris car le bit de haut rang vaut 0
        pkt_set_length(pkt,((byte<<1)>>1)); //on set la longueur

        memcpy(&byte,data+2,1);
        pkt_set_seqnum(pkt,(uint8_t) byte); //on set le seqnum


        uint32_t timestp;
        memcpy(&timestp,data+3,4); //timestamp
        //timestamp = ntohl(timestamp);
        pkt_set_timestamp(pkt, (uint32_t) timestp); //on set le timestamp


        uint32_t crc1; //crc1
        memcpy(&crc1,data+taille_header,4);
        crc1 = ntohl(crc1);
        pkt_set_crc1(pkt, crc1); //on set le crc1
    }

    else{ //la longueur est definie sur quinze bits, le header sur huit bytes
        taille_header = 8;

        uint16_t leng; //on supprime bit de haut rang du second byte et on ajoute le troisieme byte
        memcpy(&leng,data+1,1);

        pkt_set_length(pkt,((len<<1)>>1)); //on set la longueur

        memcpy(&byte,data+3,1);
        pkt_set_seqnum(pkt, byte); //on set le seqnum

        uint32_t timestp;
        memcpy(&timestp,data+4,4); //timestamp
        //timestamp = ntohl(timestamp);
        pkt_set_timestamp(pkt, timestp); //on set le timestamp

        uint32_t crc1; //crc1
        memcpy(&crc1, data+(taille_header),4);
        crc1 = ntohl(crc1);
        pkt_set_crc1(pkt, crc1); //on set le crc1
    }

    if(sizeof(data) == (taille_header + 4 + pkt_get_length(pkt) + 4)){ //payload + crc2

        char * payload = (char*)malloc(pkt_get_length(pkt));
        memcpy(&payload,data+(taille_header+4),pkt_get_length(pkt)); //payload
        pkt_set_payload(pkt, payload, pkt_get_length(pkt)); //on set le payload

        uint32_t crc2;
        memcpy(&crc2, data+(taille_header+4+pkt_get_length(pkt)),4); //crc2
        pkt_set_crc2(pkt, crc2); //on set le crc1
        return PKT_OK;
    }

    else if(sizeof(data) == (taille_header + 4 + pkt_get_length(pkt))){ //payload

        char * payload = (char*)malloc(pkt_get_length(pkt));
        memcpy(&payload,data+(taille_header+4),pkt_get_length(pkt)); //payload
        pkt_set_payload(pkt, payload, pkt_get_length(pkt)); //on set le payload
        return PKT_OK;
    }

    else{ //pas de payload
        return PKT_OK;
    }
    return PKT_OK;
}


/**************************************************/

pkt_status_code pkt_encode_ack(const pkt_t* pkt, char *buf, size_t *len) //receiver donc on ne gere que l envoi de ack/nack
{
    uint8_t header[7];
    uint8_t one;

    if(pkt_get_type(pkt)==PTYPE_ACK){
        one = 10000000;
    }
    else{
        one = 11000000;
    }

    one = one | pkt_get_window(pkt);
    memcpy(buf, &one, 1);

    uint8_t two = 0;    //pas de payload
    memcpy(buf+1, &two, 1);

    uint8_t three = pkt_get_seqnum(pkt);
    memcpy(buf+2, &three, 1);

    uint32_t tmstp = pkt_get_timestamp(pkt);
    memcpy(buf+3, &tmstp, 4);

    header[0] = one;   //cree le tableau header pour la fonction crc32
    header[1] = two;
    header [3] = three;
    memcpy(header+3, &tmstp, 4);

    uint32_t crc1 = htonl(crc32(0L, header, 7));
    memcpy(buf+7, &crc1, 4);  //ajout du crc1 
    *len=7;

    return PKT_OK;
}



/**************************************************/
ptypes_t pkt_get_type  (const pkt_t* pkt)
{
    return ntohl(pkt->type);
}

/**************************************************/
uint8_t  pkt_get_tr(const pkt_t* pkt)
{
    return ntohl(pkt->tr);
}

/**************************************************/
uint8_t  pkt_get_window(const pkt_t* pkt)
{
    return ntohl(pkt->window);
}

/**************************************************/
uint8_t  pkt_get_seqnum(const pkt_t* pkt)
{
    return ntohl(pkt->seqnum);
}

/**************************************************/
uint16_t pkt_get_length(const pkt_t* pkt)
{
    return ntohl(pkt->length);
}

/**************************************************/
uint32_t pkt_get_timestamp   (const pkt_t* pkt){
    return ntohl(pkt->timestamp);
}

/**************************************************/
uint32_t pkt_get_crc1   (const pkt_t* pkt){
    return ntohl(pkt->crc1);
}

/**************************************************/
/* Renvoie le CRC2 dans l'endianness native de la machine. Si
 * ce field n'est pas present, retourne 0.*/
uint32_t pkt_get_crc2   (const pkt_t* pkt){
    if(pkt_get_tr(pkt) == 0 && pkt_get_length(pkt) != 0){
        return ntohl(pkt->crc2);
    }
    return 0;
}

/**************************************************/
/* Renvoie un pointeur vers le payload du paquet, ou NULL s'il n'y
 * en a pas.*/
const char* pkt_get_payload(const pkt_t* pkt){
    if(pkt_get_length(pkt) == 0){
        return NULL;
    }
    return pkt->payload;
}

/**************************************************/
pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type){
    if(type == PTYPE_DATA || type == PTYPE_ACK ||type == PTYPE_NACK){
        pkt->type = type;
        return PKT_OK;
    }
    return E_TYPE;
}

/**************************************************/
pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr){
    if(pkt->type != PTYPE_DATA || tr > 1){
        return E_TR;
    }

    pkt->tr = tr;
    return PKT_OK;
}

/**************************************************/
pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window){
    if(window > MAX_WINDOW_SIZE){
        return E_WINDOW;
    }
    pkt->window = window;
    return PKT_OK;
}

/**************************************************/
pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
    pkt->seqnum = seqnum;
    return PKT_OK;
}

/**************************************************/
pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
    if(length > MAX_PAYLOAD_SIZE){
        return E_LENGTH;
    }
    pkt->length = length;
    return PKT_OK;
}

/**************************************************/
pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
    pkt->timestamp = timestamp;
    return PKT_OK;
}

/**************************************************/
pkt_status_code pkt_set_crc1(pkt_t *pkt, const uint32_t crc1)
{
    pkt->crc1 = htonl(crc1);
    return PKT_OK;
}

/**************************************************/
pkt_status_code pkt_set_crc2(pkt_t *pkt, const uint32_t crc2)
{
    pkt->crc2 = htonl(crc2);
    return PKT_OK;
}

/**************************************************/
/* Defini la valeur du champs payload du paquet.
 * @data: Une succession d'octets representants le payload
 * @length: Le nombre d'octets composant le payload
 * @POST: pkt_get_length(pkt) == length */
pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length)
{
    pkt->payload = (char*) calloc(1, length);
    memcpy(pkt->payload, data, length);
    pkt->length = length;
    return PKT_OK;
}


/**************************************************/
/*
 * Decode un varuint (entier non signe de taille variable  dont le premier bit indique la longueur)
 * encode en network byte-order dans le buffer data disposant d'une taille maximale len.
 * @post: place Ã  l'adresse retval la valeur en host byte-order de l'entier de taille variable stocke
 * dans data si aucune erreur ne s'est produite
 * @return:
 *
 *          -1 si data ne contient pas un varuint valide (la taille du varint
 * est trop grande par rapport Ã  la place disponible dans data)
 *
 *          le nombre de bytes utilises si aucune erreur ne s'est produite
 */
ssize_t varuint_decode(const uint8_t *data, const size_t len, uint16_t *retval)
{
    size_t len_var = varuint_len(data);

    if(len_var > len || (len_var != 1 && len_var != 2)){
        return -1;
    }

    if(len_var == 1){
        *retval = (*data & 1111111);
        return len_var;
    }

    *retval = (ntohs(*data) & 111111111111111);
    return len_var;
}

/**************************************************/
/*
 * Encode un varuint en network byte-order dans le buffer data disposant d'une
 * taille maximale len.
 * @pre: val < 0x8000 (val peut Ãªtre encode en varuint)
 * @return: -1 si data ne contient pas une taille suffisante pour encoder le varuint
 *la taille necessaire pour encoder le varuint (1 ou 2 bytes) si aucune erreur ne s'est produite
 */
ssize_t varuint_encode(uint16_t val, uint8_t *data, const size_t len)
{
    if(val > 127 && val < 0x8000 && len >= 2){
        val = htons((val | 0x8000));
        memcpy(data, &val, 2);
        return 2;
    }

    if(val <= 127 && len >=1){
        memcpy(data, &val, 1);
        return 1;
    }
    return -1;
}

/**************************************************/
/*
 * @pre: data pointe vers un buffer d'au moins 1 byte
 * @return: la taille en bytes du varuint stocke dans data, soit 1 ou 2 bytes.
 */
size_t varuint_len(const uint8_t *data)
{
    if((data[0] & 10000000) == 10000000){
        return 2;
    }
    return 1;
}

/**************************************************/
/*
 * @return: la taille en bytes que prendra la valeur val
 * une fois encodee en varuint si val contient une valeur varuint valide (val < 0x8000). 
 -1 si val ne contient pas une valeur varuint valide
 */
ssize_t varuint_predict_len(uint16_t val)
{
    if(val >= 0x8000){
        return -1;
    }
    if(val <= 127){
        return 1;
    }
    return 2;
}

/**************************************************/
/*
 * Retourne la longueur du header en bytes si le champs pkt->length
 * a une valeur valide pour un champs de type varuint (i.e. pkt->length < 0x8000).
 * Retourne -1 sinon
 * @pre: pkt contient une adresse valide (!= NULL)
 */
ssize_t predict_header_length(const pkt_t *pkt)
{
    if(pkt_get_length(pkt) >= 0x8000){
        return -1;
    }
    if(varuint_predict_len(pkt_get_length(pkt)) == 1){
        return 7;
    }
    return 8;
}