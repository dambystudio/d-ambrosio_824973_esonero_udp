/*
 * main.c
 *
 * TCP Server - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a TCP server
 * portable across Windows, Linux and macOS.
 */

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
#include <time.h>
#include "protocol.h"

// === FUNZIONI ===

float get_temperature(void) {
    return ((rand() % 501) / 10.0) - 10.0;
}

float get_humidity(void) {
    return ((rand() % 801) / 10.0) + 20.0;
}

float get_wind(void) {
    return (rand() % 1001) / 10.0;
}

float get_pressure(void) {
    return ((rand() % 1001) / 10.0) + 950.0;
}

void errorhandler(char *errorMessage) {
	printf("%s", errorMessage);
}

void clearwinsock() {
	#if defined WIN32
		WSACleanup();
	#endif
}

const char *supported_cities[] = {
    "bari", "roma", "milano", "napoli", "torino",
    "palermo", "genova", "bologna", "firenze", "venezia"
};

const int num_cities = 10;

int is_valid_type(char type) {
    return (type == TYPE_TEMPERATURE ||
            type == TYPE_HUMIDITY ||
            type == TYPE_WIND ||
            type == TYPE_PRESSURE);
}

int is_valid_city(const char *city) {
    for (int i = 0; i < num_cities; i++) {
        #if defined WIN32
            if (_stricmp(city, supported_cities[i]) == 0) {
                return 1;
            }
        #else
            if (strcasecmp(city, supported_cities[i]) == 0) {
                return 1;
            }
        #endif
    }
    return 0;
}

float get_weather_value(char type) {
    switch(type) {
        case TYPE_TEMPERATURE:
			return get_temperature();
        case TYPE_HUMIDITY:
			return get_humidity();
        case TYPE_WIND:
			return get_wind();
        case TYPE_PRESSURE:
			return get_pressure();
        default:
			return 0.0;
    }
}

int comunicazione(int serverSocket, struct sockaddr_in *clientAddr, socklen_t clientLen) {
    weather_request_t request;
    weather_response_t response;
    int bytes_rcvd;

    // Ricevi richiesta con deserializzazione manuale
    char recv_buffer[sizeof(char) + 64];
    bytes_rcvd = recvfrom(serverSocket, recv_buffer, sizeof(recv_buffer), 0,
                          (struct sockaddr*)clientAddr, &clientLen);

    if (bytes_rcvd <= 0) {
        errorhandler("Ricezione richiesta fallita.\n");
        return -1;
    }

    // Deserializza la richiesta
    int offset = 0;
    
    // Deserializza type (1 byte)
    memcpy(&request.type, recv_buffer + offset, sizeof(char));
    offset += sizeof(char);
    
    // Deserializza city (64 bytes)
    memcpy(request.city, recv_buffer + offset, 64);

    // Risoluzione DNS del client
    char client_name[256];
    struct hostent *host = gethostbyaddr((char*)&clientAddr->sin_addr, 
                                          sizeof(clientAddr->sin_addr), AF_INET);
    if (host != NULL) {
        strcpy(client_name, host->h_name);
    } else {
        strcpy(client_name, inet_ntoa(clientAddr->sin_addr));
    }

    printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
           client_name,
           inet_ntoa(clientAddr->sin_addr),
           request.type, 
           request.city);

    // Elabora richiesta
    if (!is_valid_type(request.type)) {
        response.status = STATUS_INVALID_REQUEST;
        response.type = '\0';
        response.value = 0.0;
    }
    else if (!is_valid_city(request.city)) {
        response.status = STATUS_CITY_NOT_FOUND;
        response.type = request.type;
        response.value = 0.0;
    }
    else {
        response.status = STATUS_SUCCESS;
        response.type = request.type;
        response.value = get_weather_value(request.type);
    }

    // Invia risposta con serializzazione manuale
    char send_buffer[sizeof(uint32_t) + sizeof(char) + sizeof(float)];
    offset = 0;
    
    // Serializza status (uint32_t con network byte order)
    uint32_t net_status = htonl(response.status);
    memcpy(send_buffer + offset, &net_status, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    // Serializza type (1 byte)
    memcpy(send_buffer + offset, &response.type, sizeof(char));
    offset += sizeof(char);
    
    // Serializza value (float con network byte order)
    uint32_t temp;
    memcpy(&temp, &response.value, sizeof(float));
    temp = htonl(temp);
    memcpy(send_buffer + offset, &temp, sizeof(float));
    offset += sizeof(float);
    
    if (sendto(serverSocket, send_buffer, offset, 0,
               (struct sockaddr*)clientAddr, clientLen) < 0) {
        errorhandler("Invio risposta fallito.\n");
        return -1;
    }

    printf("Risposta inviata.\n");
    return 0;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int server_port = SERVER_PORT;  // Porta di default

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            server_port = atoi(argv[i + 1]);
            if (server_port < 1024 || server_port > 65535) {
                printf("Errore: porta non valida (usa 1024-65535)\n");
                return -1;
            }
            printf("Utilizzo porta: %d\n", server_port);
            i++;
        }
    }

	#if defined WIN32
		WSADATA wsa_data;
		int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
		if (result != 0) {
			printf("Errore nella funziona WSAStartup()\n");
			return 0;
		}
	#endif

    int socketServer;
    socketServer = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketServer < 0) {
        errorhandler("Creazione del socket fallita.\n");
        clearwinsock();
        return -1;
    }
    printf("Creazione socket eseguita.\n");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketServer, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        errorhandler("Binding fallito.\n");
        closesocket(socketServer);
        clearwinsock();
        return -1;
    }
    printf("Binding completato sulla porta %d.\n", server_port);

    struct sockaddr_in clientAddr;
    socklen_t clientLen;

    while(1) {
        printf("\nIn attesa di richieste UDP...\n");
        clientLen = sizeof(clientAddr);

        comunicazione(socketServer, &clientAddr, clientLen);
    }

    closesocket(socketServer);
    clearwinsock();
    return 0;
}
