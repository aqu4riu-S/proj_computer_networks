#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>




/* IP and Port */
#define GSIP_D "tejo.tecnico.ulisboa.pt"
#define GSPORT_D "58049"
#define PORT_MIN 1024
#define PORT_MAX 65536


/* Size of word */
#define MAX_WORD_SIZE 31
#define MIN_WORD_SIZE 4
#define MAX_PLACEHOLDER_SIZE 60


/* Max errors */
#define MAX_ERRORS_LOW 7
#define MAX_ERRORS_MEDIUM 8
#define MAX_ERRORS_HIGH 9


#define FIRST_ACTION_SIZE 11
#define COMMAND_SIZE 60
#define COMMAND 4
#define STATUS 6


/* Files */
#define FILENAME 25
#define FILESIZE 11


/* Possible commands keyboard -> client */
#define START "start"
#define SG "sg"
#define PLAY "play"
#define PL "pl"
#define GUESS "guess"
#define GW "gw"
#define SCOREBOARD "scoreboard"
#define SB "sb"
#define HINT "hint"
#define H "h"
#define STATE "state"
#define ST "st"
#define REVEAL "rev"
#define QUIT "quit"
#define EXIT "exit"
#define SNG_SIZE 12
#define PLG_SIZE 16
#define PWG_SIZE 46


/* Possible commands server -> client */
#define RSG "RSG"
#define RLG "RLG"
#define RWG "RWG"
#define RQT "RQT"
#define RRV "RRV"
#define RSB "RSB"
#define RHL "RHL"
#define RST "RST"


/* Possible commands client -> server */
#define QUT "QUT"
#define REV "REV"
#define GSB "GSB"
#define GHL "GHL"
#define STA "STA"
#define SNG "SNG"
#define PLG "PLG"
#define PWG "PWG"


#define BLOCK_SIZE 256
#define BUFFER_SIZE 128
#define PLAYS_SIZE 4
#define PLID_SIZE 7


/* Status words */
#define OK "OK"
#define NOK "NOK"
#define WIN "WIN"
#define OVR "OVR"
#define INV "INV"
#define ERR "ERR"
#define DUP "DUP"
#define EMPTY "EMPTY"
#define ACT "ACT"
#define FIN "FIN"


/* Game control constants */
#define EXIT_GAME 1
#define QUIT_EXIT_OK 1
#define QUIT_EXIT_NOK 0
#define RESET 1
 

/* Useful display messages */
#define QUIT_MSG "Your game has been successfully closed!\n"
#define EXIT_MSG "Application successfully closed!\n"
#define BREAK_LINE "----------------------------------------------"




/* Holds info of a client game */
struct ClientInfo {
    char plid[PLID_SIZE];
    int n_letters;
    int max_errors;
    int errors_left;
    int trial;
    char word_guess[MAX_WORD_SIZE];
    int positions[MAX_WORD_SIZE];
    char placeholder[MAX_PLACEHOLDER_SIZE];
    char letter;
    char command[COMMAND_SIZE];
    int on_going;
};


/* Holds info of a file */
struct FileInfo {
    char fname[FILENAME];
    char fsize[FILESIZE];
    char command[COMMAND];
};




// --------------------- First command validation ------------------------ //

void ProcessGameInvocation(int argc, char *argv[], char *GSIP, char *GSPORT);


// --------------------- Player user interface validation ------------------------ //

int ProcessInput(char *message, char *input, struct ClientInfo *client);


// --------------------- Connections ------------------------ //

void UdpConnection(char message[], char *buffer);
void TcpConnection(char message[]);


// --------------------- Server response (UDP) ------------------------ //

int DecodeServerMessage(char *message, struct ClientInfo *client);


// --------------------- Functions to process server message ------------------------ //

// --------------------- UDP ------------------------ //

void ProcessRSG(char *status, int message_size, int *increase_trial, char *p, struct ClientInfo *client);
int ProcessRLG(char *status, int message_size, int *increase_trial, char *p, struct ClientInfo *client, char *message);
int ProcessRWG(char *status, int message_size, int *increase_trial, char *p, struct ClientInfo *client);
int ProcessRQT(char *status, int message_size);

// --------------------- TCP ------------------------ //

int ProcessTCP(char *msg, int message_size, struct FileInfo *file, char plid[]);


// --------------------- AUXILIARY FUNCTIONS ------------------------ //

int IsFileName(char filename[], int size);
int IsFileSize(char filesize[]);
int IsWord(char *word);
int IsNumber(char *word);
int IsValidWordSize(int size);
int IsValidMaxErrors(int size, int errors);
char* GetPlaceHolder(char *placeholder, int size);
int ProcessRLGOK(char *message, int *positions);
void UpdatePlaceHolder(char *placeholder, int *positions, int size, char letter);
void CompleteWord(char *placeholder, char letter, int n_letters);
int ReadWord(char *message, char *buffer, int max_size, int exact_size, int is_data);
void ReadFileData(char *msg, size_t bytes_to_read, struct FileInfo *file);
void RemoveFileIfExists(char filename[]);