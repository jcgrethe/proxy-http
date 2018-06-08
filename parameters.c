#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "./parameters.h"
#include "styles.h"
#include <string.h>
void print_help();
void print_version();
uint16_t validate_port(char * port);

// Global variable with the parameters
options parameters;

void parse_options(int argc,const char **argv) {
    uint16_t port;
    /* Initialize default values */
    parameters = malloc(sizeof(*parameters));

    /* Default Values */
    parameters->http_port = HTTP_DEFAULT_PORT;
    parameters->sctp_port = SCTP_DEFAULT_PORT;
    parameters->error_file = ERROR_FILE_DEFAULT;
    parameters->management_address = LOCALHOST;
    parameters->listen_address = LISTENING_ADDRESS;
    parameters->transformations = 0;    //Desactivated by default
    parameters->filter_command = NULL;

    int c;
    /* e: option e requires argument e:: optional argument */
    while ((c = getopt (argc, argv, "e:hl:L:m:M:o:p:P:t:v")) != -1){
        switch (c) {
            case 'e':
                /* Error file */
                parameters->error_file = optarg;
                break;
            case 'h':
                /* Help */
                print_help();
                exit(0);
            case 'l':
                /* Listen address */
                parameters->listen_address = optarg;
                break;
            case 'L':
                /* Management listen address */
                parameters->management_address = optarg;
                break;
            case 'M':
                /* Media types transformables */
                parameters->media_types_input = optarg;
                parameters->mts = parse_media_types(parameters->media_types_input);
                break;
            case 'o':
                /* Management SCTP port */
                port = validate_port(optarg);
                parameters->sctp_port = (port > 0)?port:SCTP_DEFAULT_PORT;
                break;
            case 'p':
                /* proxy port */
                port = validate_port(optarg);
                parameters->http_port = (port > 0)?port:HTTP_DEFAULT_PORT;
                break;
            case 't': {
                /* Response filters */
                int size = sizeof(char) * strlen(optarg) + 1;
                char * cmd = malloc(size);
                if (cmd == NULL)
                    exit(0);
                strcpy(cmd, optarg);
                parameters->filter_command = cmd;
                parameters->transformations   = 1;
            }
                break;
            case 'v':
                /* Version */
                print_version();
                exit(0);
            default:
                printf(ECOLOR EPREFIX"Parametros invalidos.\n Puede consultar en "STCOLOR"./httpd -h "ECOLOR" las opciones disponibles\n"ESUFIX RESETCOLOR);
                exit(0);
        }
    }
}

void print_help(){
    printf(TCOLOR TPREFIX " Help " TSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t Parametros disponibles " SSUFIX RESETCOLOR"\n\n");
    printf(SCOLOR SPREFIX"\t-e "STCOLOR"archivo-de-error" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\tespecifica el archivo de error donde se redirecciona stderr de las "
           "ejecuciones de los filtros" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-h" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\timprime la ayuda y termina" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-l "STCOLOR"direccion_pop3" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\testablece la dirección donde servirá el proxy" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-L "STCOLOR"direccion_management" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\testablece la dirección donde servirá el servicio de management" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-m "STCOLOR"mensaje_de_reemplazo" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\tmensaje testigo dejado cuando una parte es reemplazada por el "
           "filtro" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-M "STCOLOR" media_types_censurables" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\tlista de media types censurados" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-o "STCOLOR" puerto_de_management" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\tpuerto SCTP donde servirá el servicio de management" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-p "STCOLOR" puerto_local" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\tpuerto TCP donde escuchará conexiones entrantes" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-t "STCOLOR" cmd" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\tcomando utilizado para las transofmraciones externas" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX "\t-v" SSUFIX RESETCOLOR"\n");
    printf(SCOLOR SPREFIX"\timprime la versión y termina" SSUFIX RESETCOLOR"\n");

}
void print_version(){
    printf("Version: %s\n", VERSION);
}
uint16_t validate_port(char * port){
    char ** rubbish;
    return (strtol(port, rubbish, 10) > MIN_PORT && strtol(port, rubbish, 10) < MAX_PORT)?strtol(port, rubbish, 10):0;
}