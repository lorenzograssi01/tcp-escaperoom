#include "utility.h"

void error(const char* error_msg)
{
    perror(error_msg);
    exit(EXIT_FAILURE);
}

int8_t interpret_command(char* buffer, int8_t type, void* arg1, void* arg2) //interpreta il comando sulla stringa buffer e ritorna il codice del comando in base al caso di utilizzo (type)
{                                                                           //mette su arg1 e arg2 gli argomenti eventualmente convertiti per essere usati in base al comando
    int narg;
    char comm[16], c_arg1[16], c_arg2[16];
    narg = sscanf(buffer, "%15s %15s %15s", comm, c_arg1, c_arg2);
    narg--;
    if(type == 0)
    {
        if(strcmp(comm, "start") == 0)
        {
            if(narg == 1)
            {
                sscanf(c_arg1, "%u", (unsigned int*)arg1);
                return 0;
            }
            if(narg == 0)
            {
                printf("No port specified, using default (4242)\n");
                *(unsigned int*)arg1 = 4242;
                return 0;
            }
            printf("Invalid format\n");
            return -1;
        }
        if(strcmp(comm, "stop") == 0)
        {
            return 1;
        }
        printf("Invalid command\n");
        return -1;
    }
    if(type == 1)
    {            
        if(strcmp(comm, "start") == 0)
        {
            if(narg == 1)
            {
                sscanf(c_arg1, "%u", (unsigned int*)arg1);
                return 0;
            }
            printf("Invalid format\n");
            return -1;
        }
        printf("Invalid command\n");
        return -1;
    }
    if(type == 2)
    {        
        if(strcmp(comm, "look") == 0)
        {
            if(narg == 1)
            {
                strcpy((char*)arg1, c_arg1);
                return 11;
            }
            if(narg == 0)
            {
                return 10;
            }
            return -1;
        }
        if(strcmp(comm, "take") == 0)
        {
            if(narg == 1)
            {
                strcpy((char*)arg1, c_arg1);
                return 20;
            }
            return -1;
        }
        if(strcmp(comm, "use") == 0)
        {
            if(narg == 1)
            {
                strcpy((char*)arg1, c_arg1);
                return 21;
            }
            if(narg == 2)
            {
                strcpy((char*)arg1, c_arg1);
                strcpy((char*)arg2, c_arg2);
                return 22;
            }
            return -1;
        }
        if(strcmp(comm, "objs") == 0)
        {
            if(narg == 0)
            {
                return 40;
            }
            return -1;
        }
        if(strcmp(comm, "end") == 0)
        {
            if(narg == 0)
            {
                return 50;
            }
            return -1;
        }
        return -2;
    }
    return -1;
}

int8_t readcommand(const char* commands, int8_t type, void* arg1, void* arg2) //legge e interpreta il comando fino a che non è valido
{                                                                             //ritorna il codice del comando
    char buffer[MAXCOMMANDSTRING];
    while(1)
    {
        int8_t rv;
        printf("%s", commands);
        fgets(buffer, MAXCOMMANDSTRING, stdin);
        rv = interpret_command(buffer, type, arg1, arg2);
        if(rv >= 0)
            return rv;
    }
}

void unserialize_account(const char* buffer, struct account_data* account)  //interpreta stringa di byte in un'account
{
    strncpy(account->username, buffer, USERNAMELENGTH);
    strncpy(account->password, buffer + USERNAMELENGTH, PASSWORDLENGHT);
}

void serialize_account(char* buffer, const struct account_data* account) //converte account in stringa di byte
{
    strncpy(buffer, account->username, USERNAMELENGTH);
    strncpy(buffer + USERNAMELENGTH, account->password, PASSWORDLENGHT);
}

void send_byte(int8_t byte, int socket) //invia un byte, in caso di errore stampa un messaggio di errore e termina il processo
{
    int ret = send(socket, &byte, 1, 0);
    if(ret < 0)
        error("Error sending byte");
}

int8_t recv_byte(int8_t* byte, int socket, int8_t close_die) //riceve un byte, in caso di errore stampa un messaggio di errore e termina il processo
{                                                            //close_die serve per decidere cosa fare in caso di chiusura della connessione
    int ret = recv(socket, byte, 1, 0);
    if(ret < 0)
        error("Error receiving byte");
    if(ret == 0)
    {
        printf("Connection has been closed\n");
        if(close_die)
            exit(1);
        return -1;
    }
    return 0;
}

int8_t recv_string(char* string, int socket, int len, int8_t close_die) //riceve una stringa di byte di lunghezza len, in caso di errore stampa un messaggio di errore e termina il processo
{                                                                       //close_die serve per decidere cosa fare in caso di chiusura della connessione
    int ret, sum = 0;
    while(sum < len)
    {
        ret = recv(socket, string, len, 0);
        if(ret < 0)
            error("Error receiving string");
        if(ret == 0)
        {
            printf("Connection has been closed\n");
            if(close_die)
                exit(1);
            return -1;
        }
        sum += ret;
    }
    return 0;
}

void send_string(const char* string, int socket, int len) //invia una stringa di byte di lunghezza len, in caso di errore stampa un messaggio di errore e termina il processo
{
    int ret, sum = 0;
    while(sum < len)
    {
        ret = send(socket, string, len, 0);
        if(ret < 0)
            error("Error sending string");
        sum += ret;
    }
}

int8_t recv_short(__int16_t* val, int socket, int8_t close_die) //riceve due byte e li converte in un intero a 16 bit, in caso di errore stampa un messaggio di errore e termina il processo
{                                                               //close_die serve per decidere cosa fare in caso di chiusura della connessione
    int ret, sum = 0;
    void* p = val;
    while(sum < 2)
    {
        ret = recv(socket, p, 2, 0);
        if(ret < 0)
            error("Error receiving value");
        if(ret == 0)
        {
            printf("Connection has been closed\n");
            if(close_die)
                exit(1);
            return -1;
        }
        sum += ret;
    }
    *val = ntohs(*val);
    return 0;
}

void send_short(__int16_t val, int socket) //invia un intero a 16 bit convertendolo in due byte, in caso di errore stampa un messaggio di errore e termina il processo
{
    int ret, sum = 0;
    void* p = &val;
    val = htons(val);
    while(sum < 2)
    {
        ret = send(socket, p, 2, 0);
        if(ret < 0)
            error("Error sending value");
        sum += ret;
    }
    return;
}