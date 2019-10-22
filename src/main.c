#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "packet_implement.h"
#include "receive.h"

int main(int argc, char **argv) {
    int opt;
    int port;
    int N;
    char *format;
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
                else if (isprint (optopt)) {
                    fprintf(stderr, "option inconnue '%c'.\n", optopt);
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
    receive(hostname,port,N);
    exit(EXIT_SUCCESS);
}
