#if defined WIN32
#include <winsock.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "protocol.h"

void errorhandler(char *errorMessage) {
    printf("%s", errorMessage);
}

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void print_usage(void) {
    printf("Usage: client-project [-s server] [-p port] -r \"type city\"\n");
    printf("  -s server : Server IP (default: 127.0.0.1)\n");
    printf("  -p port   : Server port (default: 56700)\n");
    printf("  -r request: Weather request \"t|h|w|p city\" (REQUIRED)\n");
    printf("\nExample: client-project -r \"t bari\"\n");
}

void print_result(char *server_name, char *server_ip, weather_response_t response, char *city) {
    printf("Ricevuto risultato dal server %s (ip %s). ", server_name, server_ip);

    if (response.status == STATUS_SUCCESS) {
        city[0] = toupper((unsigned char)city[0]);

        switch(response.type) {
            case TYPE_TEMPERATURE:
                printf("%s: Temperatura = %.1f°C\n", city, response.value);
                break;
            case TYPE_HUMIDITY:
                printf("%s: Umidita' = %.1f%%\n", city, response.value);
                break;
            case TYPE_WIND:
                printf("%s: Vento = %.1f km/h\n", city, response.value);
                break;
            case TYPE_PRESSURE:
                printf("%s: Pressione = %.1f hPa\n", city, response.value);
                break;
        }
    }
    else if (response.status == STATUS_CITY_NOT_FOUND) {
        printf("Citta' non disponibile\n");
    }
    else if (response.status == STATUS_INVALID_REQUEST) {
        printf("Richiesta non valida\n");
    }
}

int main(int argc, char *argv[]) {

    char server_ip[16] = "127.0.0.1";
    char server_name[256] = "localhost";
    int server_port = SERVER_PORT;
    char request_string[128] = "";
    int has_request = 0;

    // PARSING ARGOMENTI
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            // Prova prima come hostname
            struct hostent *host = gethostbyname(argv[i + 1]);
            if (host != NULL) {
                // È un hostname valido
                strcpy(server_name, argv[i + 1]);
                strcpy(server_ip, inet_ntoa(*((struct in_addr*)host->h_addr)));
            } else {
                // Prova come indirizzo IP
                struct in_addr addr;
                if (inet_pton(AF_INET, argv[i + 1], &addr) == 1) {
                    strcpy(server_ip, argv[i + 1]);
                    // Reverse lookup
                    host = gethostbyaddr((char*)&addr, sizeof(addr), AF_INET);
                    if (host != NULL) {
                        strcpy(server_name, host->h_name);
                    } else {
                        // Se il reverse lookup fallisce, usa l'IP come nome
                        strcpy(server_name, argv[i + 1]);
                    }
                } else {
                    printf("Errore: host non valido '%s'\n", argv[i + 1]);
                    return -1;
                }
            }
            i++;
        }
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            server_port = atoi(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            strcpy(request_string, argv[i + 1]);
            has_request = 1;
            i++;
        }
    }

    if (!has_request) {
        printf("Errore: argomento -r obbligatorio\n");
        print_usage();
        return -1;
    }

	#if defined WIN32
		WSADATA wsa_data;
		int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
		if (result != 0) {
			printf("Error at WSAStartup()\n");
			return 0;
		}
	#endif

    // CREA SOCKET
    int my_socket;
    my_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket < 0) {
        errorhandler("Creazione del socket fallita.\n");
        clearwinsock();
        return -1;
    }

	#if defined WIN32
		DWORD timeout = 5000;
		setsockopt(my_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	#else
		struct timeval tv;
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		setsockopt(my_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	#endif

	printf("Creazione socket eseguita.\n");

    // CONFIGURA INDIRIZZO SERVER
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    // PARSE DELLA RICHIESTA
    weather_request_t request;
    memset(&request, 0, sizeof(request));

    // Validazione: verifica caratteri di tabulazione
    for (int i = 0; request_string[i]; i++) {
        if (request_string[i] == '\t') {
            printf("Errore: la richiesta non puo' contenere caratteri di tabulazione\n");
            closesocket(my_socket);
            clearwinsock();
            return -1;
        }
    }

    // Trova il primo spazio
    char *space_pos = strchr(request_string, ' ');
    if (space_pos == NULL || space_pos == request_string) {
        printf("Errore: formato richiesta non valido\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // Verifica che il primo token sia un solo carattere
    int first_token_len = space_pos - request_string;
    if (first_token_len != 1) {
        printf("Errore: il tipo deve essere un singolo carattere\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    request.type = request_string[0];

    // Estrai la città (dopo lo spazio)
    char *city_start = space_pos + 1;
    
    // Verifica lunghezza città
    if (strlen(city_start) >= 64) {
        printf("Errore: nome citta' troppo lungo (massimo 63 caratteri)\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    strcpy(request.city, city_start);

    // Converti città in lowercase
    for (int i = 0; request.city[i]; i++) {
        request.city[i] = tolower((unsigned char)request.city[i]);
    }

    // INVIA RICHIESTA con serializzazione manuale
    char send_buffer[sizeof(char) + 64];
    int offset = 0;
    
    // Serializza type (1 byte, no conversione)
    memcpy(send_buffer + offset, &request.type, sizeof(char));
    offset += sizeof(char);
    
    // Serializza city (64 bytes, no conversione)
    memcpy(send_buffer + offset, request.city, 64);
    offset += 64;
    
    if (sendto(my_socket, send_buffer, offset, 0,
       (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        errorhandler("Invio richiesta fallito.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // RICEVI RISPOSTA con deserializzazione manuale
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    char recv_buffer[sizeof(uint32_t) + sizeof(char) + sizeof(float)];
    int bytes_rcvd = recvfrom(my_socket, recv_buffer, sizeof(recv_buffer), 0,
                              (struct sockaddr*)&from_addr, &from_len);
    if (bytes_rcvd <= 0) {
        errorhandler("Ricezione risposta fallita.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // Deserializza la risposta
    weather_response_t response;
    offset = 0;
    
    // Deserializza status (uint32_t con network byte order)
    uint32_t net_status;
    memcpy(&net_status, recv_buffer + offset, sizeof(uint32_t));
    response.status = ntohl(net_status);
    offset += sizeof(uint32_t);
    
    // Deserializza type (1 byte)
    memcpy(&response.type, recv_buffer + offset, sizeof(char));
    offset += sizeof(char);
    
    // Deserializza value (float con network byte order)
    uint32_t temp;
    memcpy(&temp, recv_buffer + offset, sizeof(float));
    temp = ntohl(temp);
    memcpy(&response.value, &temp, sizeof(float));

    // STAMPA RISULTATO
    print_result(server_name, server_ip, response, request.city);

    // CHIUDI E PULISCI
    closesocket(my_socket);
    clearwinsock();

    return 0;
}
