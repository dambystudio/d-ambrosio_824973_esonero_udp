/*
 * protocol.h
 *
 * Client header file
 * Definitions, constants and function prototypes for the client
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

// === PARAMETRI NETWORK ===
#define SERVER_PORT 56700  // Server port (change if needed)
#define BUFFER_SIZE 512    // Buffer size for messages

// === CODICI DI STATO ===
#define STATUS_SUCCESS 0        // Operation successful
#define STATUS_CITY_NOT_FOUND 1  // Unknown city
#define STATUS_INVALID_REQUEST 2    // Invalid request

// === TIPI DI DATI METEOROLOGICI ===
#define TYPE_TEMPERATURE 't'  // Temperature
#define TYPE_HUMIDITY 'h'     // Humidity
#define TYPE_WIND 'w'         // Wind speed
#define TYPE_PRESSURE 'p'     // Atmospheric pressure

// === STRUTTURE DATI ===
typedef struct {
    char type;        // Weather data type: 't', 'h', 'w', 'p'
    char city[64];    // City name (null-terminated string)
} weather_request_t;

typedef struct {
    unsigned int status;  // Response status code
    char type;            // Echo of request type
    float value;          // Weather data value
} weather_response_t;

// === PROTOTIPI DELLE FUNZIONI ===
void errorhandler(char *errorMessage);
void clearwinsock(void);
void print_usage(void);
void print_result(char *server_name, char *server_ip, weather_response_t response, char *city);

#endif /* PROTOCOL_H_ */
