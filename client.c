#include "utility.h"

int sd; //socket descriptor per il socket di connessione al server
struct account_data user; //Questa variabile memorizza l'account dell'utente che fa il login
pthread_t r_thread; //id del thread che si occupa di ricevere e decodificare i messaggi del server
pthread_mutex_t synch; //semaforo di sincronizzazione tra i due thread (aspetto la risposta a un comando prima di poterne inviare un altro)
time_t start_time; //tempo di inizio della partita (serve per il cronometro)
int16_t tot_time; //durata massima della partita (valore ricevuto da server)

int8_t login()
{
    printf("WELCOME!\nPlease login (usernames and passwords can only contain alphanumeric characters, no spaces allowed)\n");
    while(1)
    {
        char buffer[PLAYERDATASIZE];
        int8_t response = -1;
        printf("Username: ");
        scanf("%15s", user.username);
        getchar(); //uso le getch per evitare che mi rimanga un \n nel buffer
        printf("Password: ");
        scanf("%15s", user.password);
        getchar();
        serialize_account(buffer, &user);
        send_string(buffer, sd, PLAYERDATASIZE);
        recv_byte(&response, sd, 1);
        if(response == 0)
        {
            printf("Successfully logged in!\n");
            return 1;
        }
        if(response == 1)
        {
            printf("Your username didn't exist, the account was created\n");
            return 1;
        }
        if(response == 2)
            printf("Wrong password, try again\n");
        else
            return 0;
    }
}

void asynch_bytes(int8_t byte) //quesa funzione serve per decodificare i byte che il server manda in modo asincrono (non come risposta a un comando)
{
    switch (byte)
    {
    case (int8_t)0xFF:
        printf("\nYOU WON!\nCongratulations!\n");
        exit(0);
        break;
    case (int8_t)0xFE:
        printf("\nTime's up!\nThanks for playing, good luck next time!\n");
        exit(0);
        break;
    default:
        printf("Code unrecognized, an error has occurred or this client is not up to date\n");
        exit(1);
        break;
    }
}

void* receive_thread(void* attr) //funzione eseguita dal thread che si occupa di ricevere e decodificare i messaggi del server
{
    int sd = *(int*)attr; //il socket viene passato come argomento
    char buffer[MAXRESPONSELENGHT];
    while(1)
    {
        int8_t msg_type;
        recv_byte(&msg_type, sd, 1); //ricevo byte dal server, a seconda del codice lo interpreto in modo diverso

        switch (msg_type)
        {
            fd_set read_fs;
            int fd_max;
            int i;
            int8_t token_byte, tokens, remaining_tokens; //token byte memorizza insieme i token raccolti e quelli rimanenti in modo da mandare un byte solo
        case 0x02: //codice 0x02 indica che devo risolvere un enigma

            recv_string(buffer, sd, MAXRESPONSELENGHT, 1); //ricevo la domanda dell'enigma, qua non rischio di ricevere un byte asincrono perché il server è in zona protetta da mutex
            printf("%s\n", buffer);
            
            FD_ZERO(&read_fs);

            FD_SET(STDIN_FILENO, &read_fs);
            FD_SET(sd, &read_fs);

            fd_max = STDIN_FILENO > sd ? STDIN_FILENO : sd;
            select(fd_max + 1, &read_fs, NULL, NULL, NULL); //uso la select perché mentre aspetto che l'utente digiti la risposta all'indovinello, il server potrebbe mandare byte in modo asincrono
            for(i = 0; i <= fd_max; i++)
            {
                if(!(FD_ISSET(i, &read_fs)))
                    continue;
                if(i == STDIN_FILENO) //in questo caso l'utente ha digitato la risposta, quindi la invio al server
                {            
                    int8_t is_ans_corr;
                    scanf("%15s", buffer);
                    getchar();
                    send_string(buffer, sd, 16);
                    recv_byte(&is_ans_corr, sd, 1);
                    if(is_ans_corr != 0x00 && is_ans_corr != 0x01) //controllo comunque che il server non abbia mandato byte asincroni nel mentre che io inviavo la risposta
                        asynch_bytes(is_ans_corr);
                }
                else //in questo caso ho ricevuto un byte asincrono quindi lo interpreto
                {   
                    int8_t msg_type;
                    recv_byte(&msg_type, sd, 1);
                    asynch_bytes(msg_type);
                }
            }
            break;
        case 0x01: //codice 0x01 indica un messaggio di risposta normale
            recv_byte(&token_byte, sd, 1);
            tokens = (token_byte >> 4);
            remaining_tokens = (token_byte & 0x0F);

            recv_string(buffer, sd, MAXRESPONSELENGHT, 1);
            printf("%s\nTokens: %d Remaining: %d\nRemaining time: %ds\n", buffer, tokens, remaining_tokens, tot_time - (int)(time(NULL) - start_time));
            pthread_mutex_unlock(&synch); //sblocco il thread che legge i comandi perché ho finito di interpretare la risposta
            break;

        default:
            asynch_bytes(msg_type); //se il codice è diverso vuol dire che è un byte asincrono e lo interpreto a parte
        }
    }
}

int main(int argc, char** argv)
{
    int port = 4242;
    struct sockaddr_in s_addr;
    int8_t ok;
    int8_t player_id; //memorizza l'id del giocatore nella parita, il giocatore 1 deve selezionare la stanza in cui giocare
    if(argc >= 2)
        sscanf(argv[1], "%d", &port);
    if(port > 65535 || port < 1024)
        port = 4242;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd < 0)
        error("Error creating socket");
    
    inet_pton(AF_INET, "127.0.0.1", &s_addr.sin_addr);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    
    while(connect(sd, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0) //provo a connettermi al server
    {
        printf("Error connecting to server. Trying again in 4 seconds...\n");
        sleep(4);
    }
    
    if(!login()) //faccio il login
        error("Error logging in, the game might have ended"); //login potrebbe fallire se il server chiude la connessione o se altri giocatori hanno già iniziato la partita
  
    recv_byte(&player_id, sd, 1); //ricevo un byte di id del giocatore
    printf("You're player %d\n", player_id);

    if(player_id == 1) //se sono giocatore 1 devo selezionare la mappa
    {
        int8_t n_mappe;
        char buffer[MAXMAPNAME];
        int i;
        recv_byte(&n_mappe, sd, 1); //il server invia il numero di mappe su un byte e poi una stringa di 32 caratteri per ogni mappa che rappresenta il nome
        printf("Which room do you wanna play in?\n");
        for(i = 0; i < n_mappe; i++)
        {
            recv_string(buffer, sd, MAXMAPNAME, 1);
            printf(" %d) %s\n", i + 1, buffer);
        }
        int selected_room = -1;
        while(selected_room < 0 || selected_room >= n_mappe)
        {            
            readcommand("Use: start <room_id>\n", 1, &selected_room, NULL);
            selected_room--;
            if(selected_room < 0 || selected_room >= n_mappe)
                printf("Invalid room number\n");
        }
        send_byte((int8_t)selected_room, sd); //invio un byte che rappresenta l'id della mappa scelta
    }
    else //altrimenti aspetto
    {
        printf("Please wait while player 1 selects the room\n");
    }
    recv_byte(&ok, sd, 1); //quando il server ha ricevuto l'id della mappa ed è pronto invia a tutti i client un byte 0x00 e il gioco inizia
    if(ok != 0)
        error("Can't start game, an error has occurred");
    else
    {
        printf("GAME STARTED!\n\n");
        recv_short(&tot_time, sd, 1); //il server manda un intero a 16 bit per indicare il tempo a disposizione
    }

    char buffer[MAXCOMMANDSTRING];
    pthread_mutex_init(&synch, NULL);
    pthread_mutex_lock(&synch); //in questo modo la prossima lock blocca il processo
    start_time = time(NULL);
    pthread_create(&r_thread, NULL, receive_thread, &sd); //creo il processo che riceve dati dal server
    while(1)
    {
        fgets(buffer, MAXCOMMANDSTRING, stdin); //aspetto per avere un comando da inviare al server
        send_string(buffer, sd, MAXCOMMANDSTRING); //lo mando al server come stringa
        pthread_mutex_lock(&synch); //blocco questo thread che verrà sbloccato quando ricevo una risposta
    }
}