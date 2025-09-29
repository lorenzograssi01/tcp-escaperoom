#include "utility.h"
#define MAXLOCATIONS 16 //massimo numero di location per stanza
#define MAXOBJS 32 //massimo numero di oggetti per stanza
#define MAXEFFECTS 32 //massimo numero di effetti per stanza
#define MAXOBJSPERPLAYER 8 //numero massimo di oggetti che può tenere un giocatore (possibile limitarlo ulteriormente nei dati della mappa)

struct location
{
    char name[16];
    char look_answ[MAXRESPONSELENGHT]; //look_answ indica cosa il server risponde quando viene eseguito il comando look su questa location/oggetto
};

struct object
{
    char name[16];
    int8_t can_pickup; //alcuni oggetti non li posso mai prendere
    int8_t locked;
    int8_t can_look;
    int8_t still_there;
    int8_t give_token;
    char look_answ[MAXRESPONSELENGHT];
    int8_t is_new_answ_location; //indica se prendo questo oggetto cambia la descrizione di una location o di un altro oggetto
    char new_look_answ[MAXRESPONSELENGHT]; //indica la nuova descrizione dell'oggetto/location dopo che questo oggetto viene preso
    int8_t look_change;
    char take_answ[MAXRESPONSELENGHT]; //frase che invio al client quando prendo questo oggetto
    int8_t interacting; //serve per evitare conflitti tra giocatori diversi
};

struct riddle
{
    int8_t type; //0 per non avere indovinello, 1 per avere indovinello, eventualmente espandibile per indovinelli di tipo diverso
    char question[MAXRESPONSELENGHT];
    char answer[16];
};

struct effect //sono le cose che succedono quando fai 'use'
{
    int8_t obj1;
    int8_t obj2;
    int8_t unlocks1;
    int8_t unlocks2;
    int8_t give_token;
    int8_t used;
    struct riddle r;
    char new_look_answ1[MAXRESPONSELENGHT];
    char new_look_answ2[MAXRESPONSELENGHT];
    char use_answ[MAXRESPONSELENGHT];
    char use_answ_wrong[MAXRESPONSELENGHT]; //cosa stampo se sbaglio l'indovinello
    char new_look_ans_location[MAXRESPONSELENGHT]; //cosa stampo se l'indovinello è giusto (o non c'è)
    int8_t location_change; //location che viene modificata dopo l'esecuzione di questo effetto
};

struct room
{
    struct location locations[MAXLOCATIONS];
    int8_t n_locations;
    struct object objects[MAXOBJS];
    int8_t n_objects;
    struct effect effects[MAXEFFECTS];
    int8_t n_effects;
    int8_t obj_per_player;
    int8_t n_max_tokens;
    int8_t n_tokens;
    int16_t time;
    char look_answ[MAXRESPONSELENGHT];
};

struct player
{
    int8_t objects[MAXOBJSPERPLAYER]; //array contenente gli oggetti che un giocatore ha per ogni momento
};

struct server_data
{
    pthread_mutex_t mutex; //mutua esclusione per le variabili condivise all'interno di una sessione
    pthread_mutex_t timer_mutex; //blocca il timer_thread finché non viene iniziata una partita
    pthread_mutex_t win; //blocca lo win_thread finché qualcuno non vince
    pthread_t threads[MAXPLAYERS]; //id dei thread dei giocatori
    pthread_t parent;
    pthread_t timer_tread;
    pthread_t win_thread;
    pthread_cond_t game_start; //serve per bloccare i thread dei vari giocatori aspettando che il giocatore 1 selezioni la stanza
    volatile int8_t account_id[MAXPLAYERS]; //id degli account dei giocatori collegati
    volatile int sd; //socket descriptor del socket di ascolto
    volatile int cd[MAXPLAYERS]; //socket descriptor dei socket di comunicazione coi vari client
    volatile int session_id; //id della sessione
    volatile int8_t n_players; //numero giocatori collegati
    volatile int8_t next_id; //id prossimo giocatore che si collega
    volatile int8_t game_started; //indica lo stato della parita
    
    struct room playing_room; //dati della stanza
    struct player player_data[MAXPLAYERS]; //dati dei giocatori
};

pthread_mutex_t global_mutex; //serve per proeggere i dati globali (accounts)
struct account_data* accounts;
char** mappe;
int8_t n_mappe;
int n_accounts;

void write_accounts() //scrive su file gli account
{
    FILE* f;
    char* buffer;
    int i;
    pthread_mutex_lock(&global_mutex);
    f = fopen("./accounts", "wb");
    buffer = malloc(PLAYERDATASIZE * n_accounts);
    for(i = 0; i < n_accounts; i++)
    {
        serialize_account(buffer + i * PLAYERDATASIZE, &accounts[i]);
    }
    fwrite(buffer, PLAYERDATASIZE, n_accounts, f);
    fclose(f);
    free(buffer);
    pthread_mutex_unlock(&global_mutex);
}

void read_accounts() //legge da file gli account
{
    FILE* f;
    char* buffer;
    int i;
    printf("Reading accounts\n");
    pthread_mutex_lock(&global_mutex);
    f = fopen("./accounts", "rb+");
    if(!f)
        f = fopen("./accounts", "wb+");
    fseek(f, 0, SEEK_END);
	n_accounts = ftell(f) / PLAYERDATASIZE;
    buffer = malloc(PLAYERDATASIZE * n_accounts);
	fseek(f, 0, SEEK_SET);
    fread(buffer, PLAYERDATASIZE, n_accounts, f);
    fclose(f);

	accounts = malloc(n_accounts * sizeof(struct account_data));
    for(i = 0; i < n_accounts; i++)
    {
        unserialize_account(buffer + i * PLAYERDATASIZE, &accounts[i]);
    }

    free(buffer);
    pthread_mutex_unlock(&global_mutex);
}

void read_mappe() //legge le mappe (in teoria da file ma per semplicità non c'è)
{
    int i;
    n_mappe = 2;
    mappe = malloc(sizeof(char*));
    for(i = 0; i < n_mappe; i++)
    {
        mappe[i] = malloc(MAXMAPNAME);
    }
    strncpy(mappe[0], "Room number 1", 31);
    strncpy(mappe[1], "Airplane", 31);
}

int find_accunt(const struct account_data* usr) //trova un account, se non c'è lo crea, se la password è sbagliata fallisce
{                                               //se esiste l'account ritorna l'id + 1, se lo crea -(id + 1), se fallisce ritorna 0
    struct account_data* accounts2;
    int retval;
    int i;
    pthread_mutex_lock(&global_mutex);
    for(i = 0; i < n_accounts; i++)
    {
        if(strcmp(usr->username, accounts[i].username) == 0 && strcmp(usr->password, accounts[i].password) == 0)
        {
            pthread_mutex_unlock(&global_mutex);
            return i + 1;
        }
        if(strcmp(usr->username, accounts[i].username) == 0)
        {
            pthread_mutex_unlock(&global_mutex);
            return 0;
        }
    }
    
    n_accounts++;
	accounts2 = malloc(n_accounts * sizeof(struct account_data));
    accounts2[n_accounts - 1] = *usr;
    for(i = 0; i < n_accounts - 1; i++)
    {
        accounts2[i] = accounts[i];
    }
    free(accounts);
    accounts = accounts2;
    retval = - n_accounts;
    pthread_mutex_unlock(&global_mutex);
    write_accounts();
    return retval;
}

void free_memory(struct server_data* data) //cancella dalla memoria le strutture dati quando la sessione viene chiusa
{
    int i;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(data->threads[i] != pthread_self())
        {
            pthread_cancel(data->threads[i]);
            printf("Deleted thread %d (session %d)\n", i, data->session_id);
        }
        if(data->cd[i] >= 0)
        {
            close(data->cd[i]);
            printf("Closed client socket (session %d)\n", data->session_id);
        }
    }
    if(data->parent != pthread_self())
    {
        pthread_cancel(data->parent);
        printf("Deleted parent thread (session %d)\n", data->session_id);
    }
    if(data->timer_tread != pthread_self())
    {
        pthread_cancel(data->timer_tread);
        printf("Deleted timer thread (session %d)\n", data->session_id);
    }
    if(data->win_thread != pthread_self())
    {
        pthread_cancel(data->win_thread);
        printf("Deleted win thread (session %d)\n", data->session_id);
    }
    pthread_mutex_destroy(&data->mutex);
    pthread_mutex_destroy(&data->timer_mutex);
    pthread_mutex_destroy(&data->win);
    pthread_cond_destroy(&data->game_start);
    printf("Closed listening socket (session %d)\n", data->session_id);
    close(data->sd);
    free(data);
}

int8_t inc_players(struct server_data* data, int8_t* player_id, int usr, int cd) //incrementa il numero di giocatori in una sessione, usr è l'id dell account del giocatore che si è connesso
{
    int i;
    pthread_t my_id = pthread_self();
    pthread_mutex_lock(&data->mutex);
    if(data->game_started == 2) //se game_started è 2 vuol dire che il gioco è iniziato quindi nessun altro può entrare
    {
        pthread_mutex_unlock(&data->mutex);
        return 0;
    }
    data->n_players++;
    data->next_id++;
    data->game_started = 1;
    printf("Connected player %d! (session %d)\n", *player_id = data->next_id, data->session_id);
    data->account_id[*player_id - 1] = usr;
    data->cd[*player_id - 1] = cd;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(data->threads[i] == my_id)
        {
            data->threads[i] = data->threads[*player_id - 1];
            data->threads[*player_id - 1] = my_id;
        } 
    }

    send_byte(*player_id, cd);
    printf("Sent player_id byte (session %d)\n", data->session_id);

    pthread_mutex_unlock(&data->mutex);
    return 1;
}

void dec_players(struct server_data* data, int8_t player_id) //decrementa il numero di giocatori quando un giocatore si disconnette
{
    pthread_mutex_lock(&data->mutex);
    printf("Discsonnected player %d! (session %d)\n", player_id, data->session_id);
    data->account_id[player_id - 1] = -1;
    data->cd[player_id - 1] = -1;
    data->n_players--;
    if(data->n_players == 0) //se la sessione è vuota la chiudo
    {      
        printf("Session %d ended: every player has disconnected\n", data->session_id);
        free_memory(data);
    }
    else
        pthread_mutex_unlock(&data->mutex);
}

int login(int cd, int session_id) //controlla l'account e ritorna l'id dell'account che si è connesso, continua a aspettare stringhe finché il login non avviene con successo
{
    while(1)
    {
        char buffer[PLAYERDATASIZE];
        int result;
        struct account_data pl;
        
        if(recv_string(buffer, cd, PLAYERDATASIZE, 0) < 0)
            return -1;
        printf("Received login info (session %d)\n", session_id);
        unserialize_account(buffer, &pl);

        result = find_accunt(&pl);
        if(result > 0)
        {
            send_byte(0, cd);
            printf("Sent loginok byte (session %d)\n", session_id);
            return result - 1;
        }
        if(result < 0)
        {
            send_byte(1, cd);
            printf("Sent loginok byte, new account created (session %d)\n", session_id);
            return -result - 1;
        }
        send_byte(2, cd);
        printf("Sent loginfail byte (session %d)\n", session_id);
    }
}

void load_room(int8_t selected_room, struct server_data* data) //carica i dati di una stanza in memoria per poterla giocare
{
    int i, j;
    data->playing_room.n_tokens = 0;
    if(selected_room == 0)
    {
        data->playing_room.n_max_tokens = 1;
        data->playing_room.time = 190;
        data->playing_room.obj_per_player = 8;
        strcpy(data->playing_room.look_answ, "There's ++Location1++ and ++Location2++");

        //LOCATIONS
        data->playing_room.n_locations = 2;

        strcpy(data->playing_room.locations[0].name, "Location1");
        strcpy(data->playing_room.locations[0].look_answ, "You can see **Object1**");

        strcpy(data->playing_room.locations[1].name, "Location2");
        strcpy(data->playing_room.locations[1].look_answ, "There's nothing here");

        //OBJECTS
        data->playing_room.n_objects = 2;

        strcpy(data->playing_room.objects[0].name, "Object1");
        data->playing_room.objects[0].can_look = 1;
        data->playing_room.objects[0].can_pickup = 1;
        data->playing_room.objects[0].locked = 0;
        data->playing_room.objects[0].give_token = 0;
        data->playing_room.objects[0].look_change = 0;
        data->playing_room.objects[0].is_new_answ_location = 1;
        strcpy(data->playing_room.objects[0].take_answ, "You took **Object1**");
        strcpy(data->playing_room.objects[0].look_answ, "**Object1** look answer");
        strcpy(data->playing_room.objects[0].new_look_answ, "There's nothing here the object's been taken");

        strcpy(data->playing_room.objects[1].name, "Object2");
        data->playing_room.objects[1].can_look = 0;
        data->playing_room.objects[1].can_pickup = 1;
        data->playing_room.objects[1].locked = 1;
        data->playing_room.objects[1].give_token = 1;
        data->playing_room.objects[1].look_change = -1;
        data->playing_room.objects[1].is_new_answ_location = 1;
        strcpy(data->playing_room.objects[1].take_answ, "You took **Object2**");
        strcpy(data->playing_room.objects[1].look_answ, "**Object2** look answer");
        strcpy(data->playing_room.objects[1].new_look_answ, "");
    
        //EFFECTS
        data->playing_room.n_effects = 1;

        data->playing_room.effects[0].give_token = 0;
        data->playing_room.effects[0].obj1 = 0;
        data->playing_room.effects[0].obj2 = -1;
        data->playing_room.effects[0].unlocks1 = 1;
        data->playing_room.effects[0].unlocks2 = -1;
        data->playing_room.effects[0].location_change = -1;
        strcpy(data->playing_room.effects[0].new_look_answ1, "**Object2** look answer");
        strcpy(data->playing_room.effects[0].new_look_answ2, "");
        strcpy(data->playing_room.effects[0].new_look_ans_location, "");
        strcpy(data->playing_room.effects[0].use_answ, "You used **Object1**! **Object2** just appeared");
        strcpy(data->playing_room.effects[0].use_answ_wrong, "You used **Object1**, but nothing happened");
        data->playing_room.effects[0].r.type = 1;
        strcpy(data->playing_room.effects[0].r.question, "What is 1+11 in binary");
        strcpy(data->playing_room.effects[0].r.answer, "100");
        data->playing_room.effects[0].used = 0;
    }
    if(selected_room == 1)
    {
        data->playing_room.n_max_tokens = 3;
        data->playing_room.time = 250;
        data->playing_room.obj_per_player = 2;
        strcpy(data->playing_room.look_answ, "You're trapped on a Ryanair Boeing 737-800 airplane! You need to get out but the doors are all locked. You could go in the ++cabin++, in the ++cockpit++ or in the ++lavatory++");

        //LOCATIONS
        data->playing_room.n_locations = 3;

        strcpy(data->playing_room.locations[0].name, "cabin");
        strcpy(data->playing_room.locations[0].look_answ, "The **door** is locked. Everything else looks fairly normal, except for the left overhead **bin** above row 27 that is partly open, but it looks stuck");

        strcpy(data->playing_room.locations[1].name, "cockpit");
        strcpy(data->playing_room.locations[1].look_answ, "Looking around, you can see a **safe**, there may be something useful inside");

        strcpy(data->playing_room.locations[2].name, "lavatory");
        strcpy(data->playing_room.locations[2].look_answ, "There seems to be nothing useful in here, unless you need to use the **toilet**");

        //OBJECTS
        data->playing_room.n_objects = 6;

        strcpy(data->playing_room.objects[0].name, "door");
        data->playing_room.objects[0].can_look = 1;
        data->playing_room.objects[0].can_pickup = 0;
        data->playing_room.objects[0].locked = 1;
        data->playing_room.objects[0].give_token = 0;
        data->playing_room.objects[0].look_change = -1;
        data->playing_room.objects[0].is_new_answ_location = 1;
        strcpy(data->playing_room.objects[0].take_answ, "");
        strcpy(data->playing_room.objects[0].look_answ, "The **door** looks locked, you can't open it");
        strcpy(data->playing_room.objects[0].new_look_answ, "");

        strcpy(data->playing_room.objects[1].name, "bin");
        data->playing_room.objects[1].can_look = 1;
        data->playing_room.objects[1].can_pickup = 0;
        data->playing_room.objects[1].locked = 1;
        data->playing_room.objects[1].give_token = 0;
        data->playing_room.objects[1].look_change = -1;
        data->playing_room.objects[1].is_new_answ_location = 1;
        strcpy(data->playing_room.objects[1].take_answ, "");
        strcpy(data->playing_room.objects[1].look_answ, "It looks stuck, you might not be able to get it open without a tool");
        strcpy(data->playing_room.objects[1].new_look_answ, "");

        strcpy(data->playing_room.objects[2].name, "safe");
        data->playing_room.objects[2].can_look = 1;
        data->playing_room.objects[2].can_pickup = 0;
        data->playing_room.objects[2].locked = 1;
        data->playing_room.objects[2].give_token = 0;
        data->playing_room.objects[2].look_change = -1;
        data->playing_room.objects[2].is_new_answ_location = 1;
        strcpy(data->playing_room.objects[2].take_answ, "");
        strcpy(data->playing_room.objects[2].look_answ, "It's locked, but there's a screen, you need to answer a question to open it");
        strcpy(data->playing_room.objects[2].new_look_answ, "");

        strcpy(data->playing_room.objects[3].name, "toilet");
        data->playing_room.objects[3].can_look = 1;
        data->playing_room.objects[3].can_pickup = 0;
        data->playing_room.objects[3].locked = 0;
        data->playing_room.objects[3].give_token = 0;
        data->playing_room.objects[3].look_change = -1;
        data->playing_room.objects[3].is_new_answ_location = 1;
        strcpy(data->playing_room.objects[3].take_answ, "");
        strcpy(data->playing_room.objects[3].look_answ, "It looks like a regualr airplane **toilet**");
        strcpy(data->playing_room.objects[3].new_look_answ, "");

        strcpy(data->playing_room.objects[4].name, "knife");
        data->playing_room.objects[4].can_look = 0;
        data->playing_room.objects[4].can_pickup = 1;
        data->playing_room.objects[4].locked = 1;
        data->playing_room.objects[4].give_token = 1;
        data->playing_room.objects[4].look_change = 2;
        data->playing_room.objects[4].is_new_answ_location = 0;
        strcpy(data->playing_room.objects[4].take_answ, "You took the **knife**. Be careful it's very sharp");
        strcpy(data->playing_room.objects[4].look_answ, "");
        strcpy(data->playing_room.objects[4].new_look_answ, "It's open, but there's nothing left inside");

        strcpy(data->playing_room.objects[5].name, "key");
        data->playing_room.objects[5].can_look = 0;
        data->playing_room.objects[5].can_pickup = 1;
        data->playing_room.objects[5].locked = 1;
        data->playing_room.objects[5].give_token = 1;
        data->playing_room.objects[5].look_change = 1;
        data->playing_room.objects[5].is_new_answ_location = 0;
        strcpy(data->playing_room.objects[5].take_answ, "You took the **key**");
        strcpy(data->playing_room.objects[5].look_answ, "");
        strcpy(data->playing_room.objects[5].new_look_answ, "There's nothing left in here");
    
        //EFFECTS
        data->playing_room.n_effects = 4;

        data->playing_room.effects[0].give_token = 0;
        data->playing_room.effects[0].obj1 = 4;
        data->playing_room.effects[0].obj2 = 1;
        data->playing_room.effects[0].unlocks1 = 5;
        data->playing_room.effects[0].unlocks2 = 1;
        data->playing_room.effects[0].location_change = 0;
        strcpy(data->playing_room.effects[0].new_look_answ1, "It's a metal **key**. It might be useful to unlock something");
        strcpy(data->playing_room.effects[0].new_look_answ2, "There's a metal **key** inside");
        strcpy(data->playing_room.effects[0].new_look_ans_location, "The **door** is locked. Everything else looks fairly normal, the left overhead **bin** above row 27 is now open");
        strcpy(data->playing_room.effects[0].use_answ, "You used the **knife** to pry open the overhead **bin**! There may be something in it you'd better look");
        strcpy(data->playing_room.effects[0].use_answ_wrong, "");
        data->playing_room.effects[0].r.type = 0;
        strcpy(data->playing_room.effects[0].r.question, "");
        strcpy(data->playing_room.effects[0].r.answer, "");
        data->playing_room.effects[0].used = 0;

        data->playing_room.effects[1].give_token = 0;
        data->playing_room.effects[1].obj1 = 3;
        data->playing_room.effects[1].obj2 = -2;
        data->playing_room.effects[1].unlocks1 = -1;
        data->playing_room.effects[1].unlocks2 = -1;
        data->playing_room.effects[1].location_change = -1;
        strcpy(data->playing_room.effects[1].new_look_answ1, "");
        strcpy(data->playing_room.effects[1].new_look_answ2, "");
        strcpy(data->playing_room.effects[1].new_look_ans_location, "");
        strcpy(data->playing_room.effects[1].use_answ, "You flushed the **toilet**");
        strcpy(data->playing_room.effects[1].use_answ_wrong, "");
        data->playing_room.effects[1].r.type = 0;
        strcpy(data->playing_room.effects[1].r.question, "");
        strcpy(data->playing_room.effects[1].r.answer, "");
        data->playing_room.effects[1].used = -1;

        data->playing_room.effects[2].give_token = 0;
        data->playing_room.effects[2].obj1 = 2;
        data->playing_room.effects[2].obj2 = -2;
        data->playing_room.effects[2].unlocks1 = 2;
        data->playing_room.effects[2].unlocks2 = 4;
        data->playing_room.effects[2].location_change = -1;
        strcpy(data->playing_room.effects[2].new_look_answ1, "It's open, there's a big **knife** inside");
        strcpy(data->playing_room.effects[2].new_look_answ2, "It's a big **knife**, you might use it to pry something open");
        strcpy(data->playing_room.effects[2].new_look_ans_location, "");
        strcpy(data->playing_room.effects[2].use_answ, "Your answer is correct! The **safe** just opened, you should take a look at what's inside");
        strcpy(data->playing_room.effects[2].use_answ_wrong, "Wrong answer. Nothing happened");
        data->playing_room.effects[2].r.type = 1;
        strcpy(data->playing_room.effects[2].r.question, "The security question to open the **safe** is: 'What is the biggest commercial airliner in the world?'");
        strcpy(data->playing_room.effects[2].r.answer, "A380");
        data->playing_room.effects[2].used = 0;

        data->playing_room.effects[3].give_token = 1;
        data->playing_room.effects[3].obj1 = 5;
        data->playing_room.effects[3].obj2 = 0;
        data->playing_room.effects[3].unlocks1 = 0;
        data->playing_room.effects[3].unlocks2 = -1;
        data->playing_room.effects[3].location_change = -1;
        strcpy(data->playing_room.effects[3].new_look_answ1, "");
        strcpy(data->playing_room.effects[3].new_look_answ2, "");
        strcpy(data->playing_room.effects[3].new_look_ans_location, "");
        strcpy(data->playing_room.effects[3].use_answ, "You opened the **door**!");
        strcpy(data->playing_room.effects[3].use_answ_wrong, "The **door** didn't open");
        data->playing_room.effects[3].r.type = 1;
        strcpy(data->playing_room.effects[3].r.question, "This is the right **key**! But there's another layer of security, you need to answer this question: 'Where is the life jacekt stowed on this airplane?\nA - In the panel above your head\nB - Under the seat in front of you\nC - In a pocket beneath your seat'");
        strcpy(data->playing_room.effects[3].r.answer, "C");
        data->playing_room.effects[3].used = 0;
    }

    for(i = 0; i < MAXPLAYERS; i++)
    {
        for(j = 0; j < data->playing_room.obj_per_player; j++)
            data->player_data[i].objects[j] = -1;
    }
    for(i = 0; i < data->playing_room.n_objects; i++)
    {
        data->playing_room.objects[i].interacting = 0;
        data->playing_room.objects[i].still_there = 1;
    }
}

int8_t get_location_id(char* str, struct server_data* data)
{
    int i;
    for(i = 0; i < data->playing_room.n_locations; i++)
    {
        if(strcmp(str, data->playing_room.locations[i].name) == 0)
            return i;
    }
    return -1;
}

int8_t get_obj_id(char* str, struct server_data* data)
{
    int i;
    for(i = 0; i < data->playing_room.n_objects; i++)
    {
        if(strcmp(str, data->playing_room.objects[i].name) == 0)
            return i;
    }
    return -1;
}

int8_t get_effect_id(int8_t obj1, int8_t obj2, struct server_data* data)
{
    int i;
    for(i = 0; i < data->playing_room.n_effects; i++)
    {
        if(data->playing_room.effects[i].obj1 == obj1 && data->playing_room.effects[i].obj2 == obj2)
            return i;
    }
    return -1;
}

int8_t has(int8_t obj, int8_t plid, struct server_data* data)
{
    int i;
    if(obj < 0)
        return -1;
    for(i = 0; i < data->playing_room.obj_per_player; i++)
    {
        if(data->player_data[plid].objects[i] == obj)
        {
            return i;
        }
    }
    return -1;
}

int8_t pickup(int8_t obj2, int8_t plid, struct server_data* data) //prende l'oggetto, se possibile
{
    int i;
    pthread_mutex_lock(&data->mutex);
    for(i = 0; i < data->playing_room.obj_per_player; i++)
    {
        if(data->player_data[plid].objects[i] < 0)
        {
            data->player_data[plid].objects[i] = obj2;
            data->playing_room.objects[obj2].still_there = 0;
            if(data->playing_room.objects[obj2].look_change >= 0) //qua modifico le descrizioni
            {
                if(data->playing_room.objects[obj2].is_new_answ_location)
                    strcpy(data->playing_room.locations[data->playing_room.objects[obj2].look_change].look_answ, data->playing_room.objects[obj2].new_look_answ);
                else                    
                    strcpy(data->playing_room.objects[data->playing_room.objects[obj2].look_change].look_answ, data->playing_room.objects[obj2].new_look_answ);
            }
            if(data->playing_room.objects[obj2].give_token)
            {
                data->playing_room.n_tokens++;
                if(data->playing_room.n_tokens == data->playing_room.n_max_tokens) //se ho ottenuto tutti i token ho vinto
                {
                    pthread_mutex_unlock(&data->mutex); //quindi sblocco il thread che manda il byte della vittoria a tutti i client
                    return 2;
                }
            }
            pthread_mutex_unlock(&data->mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&data->mutex);
    return 0;
}

int8_t execute_effect(int8_t e, int8_t plid, struct server_data* data, int cd) //eseguo un effetto, e nel caso faccio anche l'indovinello
{
    pthread_mutex_lock(&data->mutex);
    if(data->playing_room.effects[e].r.type) //se esiste un indovinello prima faccio quello
    {
        send_byte(0x02, cd); //mando il codice di risposta 0x02 che indica un indovinello da risolvere
        printf("Code byte sent riddle (session %d)\n", data->session_id);
        send_string(data->playing_room.effects[e].r.question, cd, MAXRESPONSELENGHT);
        pthread_mutex_unlock(&data->mutex); //sblocco il mutex: possono arrivare byte asincroni, client gestisce con la select
        char answer[16];
        if(recv_string(answer, cd, 16, 0) < 0)
            return -1;
        printf("Answer received (session %d)\n", data->session_id);

        pthread_mutex_lock(&data->mutex);
        if(strcmp(answer, data->playing_room.effects[e].r.answer) != 0)
        {        
            send_byte(0x00, cd); //risposta sbagliata mando codice 0x00
            printf("Answer byte sent wrong answer (session %d)\n", data->session_id);
            pthread_mutex_unlock(&data->mutex);
            return 0;
        }  
        send_byte(0x01, cd); //risposta corretta mando codice 0x01
        printf("Answer byte sent correct answer (session %d)\n", data->session_id);
    }
    if(data->playing_room.effects[e].unlocks1 >= 0)
    {
        data->playing_room.objects[data->playing_room.effects[e].unlocks1].locked = 0;
        data->playing_room.objects[data->playing_room.effects[e].unlocks1].can_look = 1;
        strcpy(data->playing_room.objects[data->playing_room.effects[e].unlocks1].look_answ, data->playing_room.effects[e].new_look_answ1);
    }
    if(data->playing_room.effects[e].unlocks2 >= 0)
    {
        data->playing_room.objects[data->playing_room.effects[e].unlocks2].locked = 0;
        data->playing_room.objects[data->playing_room.effects[e].unlocks2].can_look = 1;
        strcpy(data->playing_room.objects[data->playing_room.effects[e].unlocks2].look_answ, data->playing_room.effects[e].new_look_answ2);
    }
    if(data->playing_room.effects[e].location_change >= 0)
    {
        strcpy(data->playing_room.locations[data->playing_room.effects[e].location_change].look_answ, data->playing_room.effects[e].new_look_ans_location);
    }
    if(data->playing_room.effects[e].used == 0)
        data->playing_room.effects[e].used = 1;

    if(data->playing_room.effects[e].give_token) 
    {
        data->playing_room.n_tokens++;
        if(data->playing_room.n_tokens == data->playing_room.n_max_tokens) //se ho preso tutti i token ho vinto
        {
            pthread_mutex_unlock(&data->mutex); //quindi sblocco il thread che manda il byte della vittoria a tutti i client
            return 2;
        }
    }
    pthread_mutex_unlock(&data->mutex);
    return 1;
}

int8_t interact(int8_t obj, struct server_data* data) //setta la variabile interact in modo che gli altri giocatori non possano interferire
{
    pthread_mutex_lock(&data->mutex);
    if(data->playing_room.objects[obj].interacting)
    {
        pthread_mutex_unlock(&data->mutex);
        return -1;
    }
    data->playing_room.objects[obj].interacting = 1;
    pthread_mutex_unlock(&data->mutex);
    return 0;
}

int8_t interact2(int8_t obj1, int8_t obj2, struct server_data* data) //stessa cosa ma con 2 oggetti insieme
{
    pthread_mutex_lock(&data->mutex);
    if(data->playing_room.objects[obj1].interacting || data->playing_room.objects[obj2].interacting)
    {
        pthread_mutex_unlock(&data->mutex);
        return -1;
    }
    data->playing_room.objects[obj1].interacting = 1;
    data->playing_room.objects[obj2].interacting = 1;
    pthread_mutex_unlock(&data->mutex);
    return 0;
}

void stop_interacting(int8_t obj1, struct server_data* data) //resetta la variabile interact quando ho finito di interagire con un oggetto
{    
    pthread_mutex_lock(&data->mutex);
    data->playing_room.objects[obj1].interacting = 0;
    pthread_mutex_unlock(&data->mutex);
}

void stop_interacting2(int8_t obj1, int8_t obj2, struct server_data* data) //stessa cosa ma con 2 oggetti
{    
    pthread_mutex_lock(&data->mutex);
    data->playing_room.objects[obj1].interacting = 0;
    data->playing_room.objects[obj2].interacting = 0;
    pthread_mutex_unlock(&data->mutex);
}

void send_server_data(char* string, int cd, struct server_data* data) //invia i dati di risposta a un comando ai client
{
    int8_t token_byte = 0; //token byte memorizza insieme i token raccolti e quelli rimanenti in modo da mandare un byte solo
    pthread_mutex_lock(&data->mutex); //in modo che non posso venire interrotto da byte asincroni ad esempio se scade il tempo
    token_byte = (data->playing_room.n_max_tokens - data->playing_room.n_tokens);
    token_byte |= (data->playing_room.n_tokens << 4);
    send_byte(0x01, cd); //mando il codice di risposta 0x01 (risposta normale)
    printf("Code byte sent (session %d)\n", data->session_id);
    send_byte(token_byte, cd); //mando il byte dei token
    printf("Token byte sent (session %d)\n", data->session_id);
    send_string(string, cd, MAXRESPONSELENGHT); //mando la stringa di risposta (lunghezza fissa 256 byte)
    printf("Response string sent (session %d)\n", data->session_id);
    pthread_mutex_unlock(&data->mutex);
}

void* player(void* attr) //funzione eseguita dai thread dei giocatori
{
    int8_t player_id;
    struct server_data* data = attr;
    struct sockaddr_in c_addr;
    int cd;
    int usr;
    unsigned c_addr_len = sizeof(c_addr);
    char buffer[MAXCOMMANDSTRING];
    printf("Thread started (session %d)\n", data->session_id);

    cd = accept(data->sd, (struct sockaddr*)&c_addr, &c_addr_len); //mi connetto a un client
    if(cd < 0)
        error("Error accepting connection");
    printf("Connection accepted (session %d)\n", data->session_id);

    usr = login(cd, data->session_id);
    if(usr < 0)
    {
        close(cd);
        printf("Connection closed (session %d)\n", data->session_id);
        return NULL;
    }
    if(!inc_players(data, &player_id, usr, cd)) //provo ad aggiungere un giocatore, se il gioco è già iniziato ritorna 0
    {
        close(cd);
        printf("Player kicked out: already started\n");
        printf("Connection closed (session %d)\n", data->session_id);
        return NULL;
    }

    if(player_id == 1) //se sono giocatore 1
    {
        int8_t selected_room;
        int i;
        send_byte(n_mappe, cd);
        printf("Sent number of maps (session %d)\n", data->session_id);
        for(i = 0; i < n_mappe; i++)
            send_string(mappe[i], cd, MAXMAPNAME);
        printf("Sent map names (session %d)\n", data->session_id);
        
        recv_byte(&selected_room, cd, 1);
        printf("Received selected map (session %d)\n", data->session_id);
        if(selected_room < 0 || selected_room > n_mappe)
            error("Bad room number");
        load_room(selected_room, data);
        send_byte(0, cd);
        printf("Sent ok byte (session %d)\n", data->session_id);
        send_short(data->playing_room.time, cd);
        printf("Sent game time (session %d)\n", data->session_id);
        pthread_mutex_unlock(&data->timer_mutex); //sblocco il thread del timer, in questo modo parte il timer che manderà in modo asincrono il byte 0xFE quando il tempo è finito
    }
    else //altrimenti
    {
        pthread_mutex_lock(&data->mutex);
        printf("Locked thread %d (session %d)\n", player_id, data->session_id);
        while(data->game_started != 2)
            pthread_cond_wait(&data->game_start, &data->mutex); //aspetto che il giocatore 1 abbia finito di selezionare la mappa
        send_byte(0, cd);
        printf("Sent ok byte (session %d)\n", data->session_id);
        send_short(data->playing_room.time, cd);
        printf("Sent game time (session %d)\n", data->session_id);
        pthread_mutex_unlock(&data->mutex);
        printf("Unlocked thread %d (session %d)\n", player_id, data->session_id);
    }

    while(1) //ciclo principale del gioco
    {
        char arg1[16], arg2[16];
        int8_t comm_code;
        if(recv_string(buffer, cd, MAXCOMMANDSTRING, 0) < 0) //il server aspetta un comando
        {
            close(cd);
            dec_players(data, player_id); //se il client si disconnette rimuovo il giocatore
            return NULL; //termino il thread
        }
        printf("Received command string (session %d)\n", data->session_id);
        comm_code = interpret_command(buffer, 2, arg1, arg2); //interpreto il comando
        switch (comm_code)
        {
            int8_t obj;
            int8_t location_obj;
            int8_t pickupret;
            int8_t need_to_have;
            int8_t effect;
            int8_t ee;
            int8_t obj2;
            char objs[MAXRESPONSELENGHT];
            int i;
        case 10:
            send_server_data(data->playing_room.look_answ, cd, data);
            break;
        case 11:
            location_obj = get_location_id(arg1, data);
            if(location_obj >= 0)
            {
                send_server_data(data->playing_room.locations[location_obj].look_answ, cd, data);
                break;
            }
            location_obj = get_obj_id(arg1, data);
            if(location_obj >= 0 && data->playing_room.objects[location_obj].can_look)
            {
                send_server_data(data->playing_room.objects[location_obj].look_answ, cd, data);
                break;
            }
            send_server_data("No such location/object in sight", cd, data);
            break;
        case 20:
            obj = get_obj_id(arg1, data);
            if(obj >= 0)
            {
                if(interact(obj, data) < 0)
                {
                    send_server_data("Another player's interacting with this object, wait for them to finish", cd, data);
                    break;
                }
                if(!data->playing_room.objects[obj].still_there)
                {
                    send_server_data("This object has already been picked up!", cd, data);
                    stop_interacting(obj, data);
                    break;
                }
                if(!data->playing_room.objects[obj].can_look)
                {
                    send_server_data("No such object in sight", cd, data);
                    stop_interacting(obj, data);
                    break;
                }
                if(!data->playing_room.objects[obj].can_pickup)
                {
                    send_server_data("You can't pick up this object", cd, data);
                    stop_interacting(obj, data);
                    break;
                }
                if(data->playing_room.objects[obj].locked)
                {
                    send_server_data("This object is locked, you have to unlock it to pick it up", cd, data);
                    stop_interacting(obj, data);
                    break;
                }
                pickupret = pickup(obj, player_id, data);
                if(pickupret)
                {
                    send_server_data(data->playing_room.objects[obj].take_answ, cd, data);
                    if(pickupret == 2)
                    {
                        pthread_mutex_unlock(&data->win);
                        return NULL;
                    }
                }
                else
                    send_server_data("Your inventory's full you can't pick up any more objects", cd, data);
                stop_interacting(obj, data);
                break;
            }
            send_server_data("No such object in sight", cd, data);
            break;
        case 21:
            obj = get_obj_id(arg1, data);
            need_to_have = 1;
            effect = get_effect_id(obj, -1, data);
            if(effect < 0)
            {
                effect = get_effect_id(obj, -2, data);
                if(effect >= 0)
                    need_to_have = 0;
            }
            if(has(obj, player_id, data) < 0 && need_to_have)
            {
                send_server_data("You don't have this object", cd, data);
                break;
            }
            if(obj < 0 || !data->playing_room.objects[obj].can_look)
            {
                send_server_data("You can't see this object", cd, data);
                break;
            }
            if(effect < 0)
            {
                send_server_data("You can't use this object", cd, data);
                break;
            }

            if(interact(obj, data) < 0)
            {
                send_server_data("Another player's interacting with this object, wait for them to finish", cd, data);
                break;
            }
            if(data->playing_room.effects[effect].used > 0)
            {
                send_server_data("Someone's already done this!", cd, data);
                stop_interacting(obj, data);
                break;
            }
            ee = execute_effect(effect, player_id, data, cd);
            if(ee == 2)
            {
                pthread_mutex_unlock(&data->win);
                return NULL; //il gioco è finito, termino il thread (gli altri verranno terminati dal win_thread)
            }
            if(ee == -1)
            {
                stop_interacting(obj, data);
                close(cd); //il client si è disconnesso, chiudo il socket
                printf("Socket closed (session %d)\n", data->session_id);
                dec_players(data, player_id); //e rimuovo il giocatore
                return NULL;
                break;
            }
            if(ee)
            {
                send_server_data(data->playing_room.effects[effect].use_answ, cd, data);
                stop_interacting(obj, data);
                break;
            }
            send_server_data(data->playing_room.effects[effect].use_answ_wrong, cd, data);
            stop_interacting(obj, data);
            break;

        case 22:
            obj = get_obj_id(arg1, data);
            obj2 = get_obj_id(arg2, data);
            effect = get_effect_id(obj, obj2, data);
            if(has(obj, player_id, data) < 0)
            {
                send_server_data("You don't have this object", cd, data);
                break;
            }
            if(obj2 < 0 || !data->playing_room.objects[obj2].can_look)
            {
                send_server_data("You can't see the second object", cd, data);
                break;
            }
            if(effect < 0)
            {
                send_server_data("You can't use this object on the second object", cd, data);
                break;
            }

            if(interact2(obj, obj2, data) < 0)
            {
                send_server_data("Another player's interacting with these objects, wait for them to finish", cd, data);
                break;
            }
            if(data->playing_room.effects[effect].used > 0)
            {
                send_server_data("Someone's already done this!", cd, data);
                stop_interacting2(obj, obj2, data);
                break;
            }
            ee = execute_effect(effect, player_id, data, cd);
            if(ee == 2)
            {
                pthread_mutex_unlock(&data->win);
                return NULL; //come sopra, ho vinto termino il thread
            }
            if(ee == -1)
            {
                stop_interacting2(obj, obj2, data);
                close(cd);
                printf("Socket closed (session %d)\n", data->session_id);
                dec_players(data, player_id); //come sopra, il client si è disconnesso
                return NULL;
                break;
            }
            if(ee)
            {
                send_server_data(data->playing_room.effects[effect].use_answ, cd, data);
                stop_interacting2(obj, obj2, data);
                break;
            }
            send_server_data(data->playing_room.effects[effect].use_answ_wrong, cd, data);
            stop_interacting2(obj, obj2, data);
            break;

        case 40:
            strcpy(objs, "Objects:\n");
            for(i = 0; i < data->playing_room.obj_per_player; i++)
            {
                if(data->player_data[player_id].objects[i] >= 0)
                {
                    strcat(objs, data->playing_room.objects[data->player_data[player_id].objects[i]].name);
                    strcat(objs, "\n");
                }
            }
            send_server_data(objs, cd, data);
            break;
        case 50: //il giocatore ha usato il comando end: chiudo il socket e lo rimuovo dal gioco
            close(cd);
            printf("Socket closed (session %d)\n", data->session_id);
            dec_players(data, player_id);
            return NULL; //termino il thread
            break;
        case -1:
            send_server_data("Invalid format\nFormats:\nlook [<location>|<object>]\ntake <object>\nuse <object1> [<object2>]\nobjs\nend", cd, data);
            break;
        case -2:
            send_server_data("Invalid command\nCommands:\nlook [<location>|<object>]\ntake <object>\nuse <object1> [<object2>]\nobjs\nend", cd, data);
            break;
        default:            
            send_server_data("Something weird happend", cd, data);//non dovrei mai ricevere questo codice, se succede probabilmente c'è un errore
            break;
        }
    }
    return NULL;
}

void* timer_thread(void* attr) //questo thread parte quando inizia la partita
{
    struct server_data* data = attr;
    int i;
    pthread_mutex_lock(&data->timer_mutex); //aspetto che venga sbloccato dal giocatore 1
    printf("Timer started\n");
    pthread_mutex_lock(&data->mutex);
    for(i = 1; i < MAXPLAYERS; i++)
    {
        if(data->cd[i] < 0)
        {
            pthread_cancel(data->threads[i]);
            printf("thread %d killed, no one logged in (session %d)\n", i, data->session_id); //ammazzo i thread dei giocatori che non si sono collegati
        }
    }
    data->game_started = 2;
    pthread_cond_broadcast(&data->game_start); //sblocco i semafori sui thread dei giocatori in attesa
    pthread_mutex_unlock(&data->mutex);
    
    sleep(data->playing_room.time); //mi fermo per il tempo del gioco
    pthread_mutex_lock(&data->mutex); //serve per non interferire con gli invii di dati
    printf("Time's up!\n");
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(data->cd[i] < 0)
            continue;
        send_byte(0xFE, data->cd[i]); //mando a tutti i client il bit di timeout
        printf("Sent timeout bit (session %d)\n", data->session_id);
    }
    free_memory(data);
    return NULL;
}

void* win_thread(void* attr) //questo thread serve per informare tutti i client quando un giocatore vince
{
    struct server_data* data = attr;
    int i;
    pthread_mutex_lock(&data->win); //questo viene risvegliato quando un giocatore vince
    pthread_mutex_lock(&data->mutex); //serve per non interferire con gli invii di dati
    
    printf("You won!\n");
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(data->cd[i] < 0)
            continue;
        send_byte(0xFF, data->cd[i]); //mando a tutti i client il bit di vittoria
        printf("Sent win bit (session %d)\n", data->session_id);
    }
    free_memory(data);
    return NULL;
}

int main()
{
    unsigned int port;
    int comm;
    int session = 0;
    int print = 1;
    printf("SERVER STARTED\n");

    pthread_mutex_init(&global_mutex, NULL);
    read_accounts();

    read_mappe();
    

    while(1)
    {
        if(print)
            comm = readcommand("1) start <port> -> starts a session using <port> as the port number\n2) stop -> shuts down the server\n", 0, &port, NULL);
        else
            comm = readcommand("", 0, &port, NULL);
        if(comm == 0)
        {
            int pid;
            if(port < 1024 || port > 65535)
            {
                printf("Can't strart session, port %d is reserved or doesn't exist\n", port);
                continue;
            }
            printf("Starting session, port %u\n", port);
            session++;
            pid = fork(); //in questo modo posso usare più sessioni (e quindi più partite) allo stesso tempo, su porte diverse
            if(pid == 0) //il figlio gestisce la sessione
            {
                struct server_data* data = malloc(sizeof(struct server_data)); //questa memoria mi serve per tenere i dati della sessione (e quindi della partita in corso)
                struct sockaddr_in s_addr;
                int i;
                memset(data, 0, sizeof(struct server_data));
                for(i = 0; i < MAXPLAYERS; i++)
                {
                    data->cd[i] = -1;
                    data->account_id[i] = -1;
                }

                data->sd = socket(AF_INET, SOCK_STREAM, 0);
                data->session_id = session;
                if(data->sd < 0)
                    error("Error creating socket");

                printf("Listening socket created\n");
                
                inet_pton(AF_INET, "127.0.0.1", &s_addr.sin_addr);
                s_addr.sin_family = AF_INET;
                s_addr.sin_port = htons(port);

                if(bind(data->sd, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0)
                    error("Error binding");
                printf("Bind successful (session %d)\n", data->session_id);

                if(listen(data->sd, MAXPLAYERS) < 0)
                    error("Error listening");
                printf("Listen successful (session %d)\n", data->session_id);

                printf("Session started, waiting for players\n");

                pthread_mutex_init(&data->mutex, NULL); //inizializzo tutti i semafori
                pthread_mutex_init(&data->timer_mutex, NULL);
                pthread_mutex_init(&data->win, NULL);
                pthread_cond_init(&data->game_start, NULL);

                pthread_mutex_lock(&data->timer_mutex); //e uso la lock in modo che quando i thread la riusino risultino bloccati, in attesa che qualcuno li risvegli
                pthread_mutex_lock(&data->win);

                data->parent = pthread_self();

                if(pthread_create(&data->timer_tread, NULL, timer_thread, data))
                    error("Error creating thread");

                if(pthread_create(&data->win_thread, NULL, win_thread, data))
                    error("Error creating thread");

                for(i = 0; i < MAXPLAYERS; i++)
                {
                    if(pthread_create(&data->threads[i], NULL, player, data))
                        error("Error creating thread");
                }

                sleep(60); //se dopo 1 minuto nessuno si è connesso chiudo la sessione
                pthread_mutex_lock(&data->mutex);
                if(data->game_started != 0)
                {
                    pthread_mutex_unlock(&data->mutex);
                    pthread_exit(NULL);
                }
                printf("Session %d aborted: no one logged in\n", data->session_id);
                free_memory(data);
                return 0;
            }
            else //il padre torna a aspettare comandi da input
                print = 0;
        }
        else if(comm == 1)
        {
            int i;
            printf("Shutting down\n");
            pthread_mutex_destroy(&global_mutex);
            free(accounts);
            for(i = 0; i < n_mappe; i++)
                free(mappe[i]);
            free(mappe);
            return 0;
        }
    }
}
