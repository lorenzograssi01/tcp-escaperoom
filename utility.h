#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#define MAXPLAYERS 5 //numero massimo di giocatori connessi a una partita
#define PASSWORDLENGHT 16 //lunghezza massima password
#define USERNAMELENGTH 16 // lunghezza massima username
#define MAXMAPNAME 32 //lunghezza massima nomi delle mappe
#define MAXCOMMANDSTRING 64 //lunghezza massima della stringa di comandi
#define MAXRESPONSELENGHT 256 //lunghezza massima della stringa di risposta del server ai comandi
#define PLAYERDATASIZE (PASSWORDLENGHT + USERNAMELENGTH) //lunghezza dati di uno user (in rete)

struct account_data
{
    char username[USERNAMELENGTH];
    char password[PASSWORDLENGHT];
};

void error(const char* error_msg);
int8_t interpret_command(char* buffer, int8_t is_client, void* arg1, void* arg2);
int8_t readcommand(const char* commands, int8_t is_client, void* arg1, void* arg2);
void serialize_account(char* buffer, const struct account_data* account);
void unserialize_account(const char* buffer, struct account_data* account);
void send_byte(int8_t byte, int socket);
int8_t recv_byte(int8_t* byte, int socket, int8_t close_die);
int8_t recv_string(char* string, int socket, int len, int8_t close_die);
void send_string(const char* string, int socket, int len);
int8_t recv_short(__int16_t* val, int socket, int8_t close_die);
void send_short(__int16_t val, int socket);