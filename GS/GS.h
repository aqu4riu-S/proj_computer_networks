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
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <signal.h>
#include <math.h>

/* IP and Port */
#define GSPORT_D "58049"
#define PORT_MIN 1024
#define PORT_MAX 65536
#define PORT_SIZE 6

/* Size of word */
#define MAX_WORD_SIZE 31
#define MIN_WORD_SIZE 4
#define MAX_PLACEHOLDER_SIZE 60
#define MESSAGE_SIZE 128

/* Max errors */
#define MAX_ERRORS_LOW 7
#define MAX_ERRORS_MEDIUM 8
#define MAX_ERRORS_HIGH 9


#define FIRST_ACTION_SIZE 11
#define COMMAND_SIZE 60
#define COMMAND 4
#define STATUS 6
#define POSITIONS 60

#define BLOCK_SIZE 512
#define BUFFER_SIZE 128
#define PLAYS_SIZE 4
#define PLID_SIZE 7


/* Files */
#define FILENAME 25
#define FILESIZE 11

// ------------------------ COMMANDS ------------------------ //
/* Possible commands client -> server */
/* --------------- UDP ---------------*/

#define SNG "SNG"
#define PLG "PLG"
#define PWG "PWG"
#define QUT "QUT"
#define REV "REV"

/* --------------- TCP ---------------*/

#define GSB "GSB"
#define GHL "GHL"
#define STA "STA"

/* Possible commands server -> client */
/* --------------- UDP ---------------*/

#define RSG "RSG"
#define RLG "RLG"
#define RWG "RWG"
#define RQT "RQT"
#define RRV "RRV"

/* --------------- TCP ---------------*/
#define RSB "RSB"
#define RHL "RHL"
#define RST "RST"


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

#define MAX_TRIAL 99

#define BAD_PLAY 0
#define GOOD_PLAY 1
#define WIN_PLAY 2
#define LOSE_PLAY 3


#define CORRECT_PLAY_ST "TRUE"
#define INCORRECT_PLAY_ST "FALSE"

#define CORRECT_PLAY 1
#define INCORRECT_PLAY 0 

/* ----------- Play codes --------------*/

#define LETTER_CODE 'L'
#define GUESS_CODE 'G'
 

#define DIRNAME_SIZE 7
#define GAME_FILE 16


#define EXIT_GAME 1
#define QUIT_EXIT_OK 1
#define QUIT_EXIT_NOK 0
#define RESET 1
 

#define QUIT_MSG "Your game has been successfully closed!\n"
#define EXIT_MSG "Application successfully closed!\n"
#define SOLVED_SO_FAR "Solved so far: "
#define TERMINATION "Termination: "
#define TOP_10_SCORES "-------------------------------- TOP 10 SCORES --------------------------------\n\
    SCORE PLAYER     WORD                             GOOD TRIALS  TOTAL TRIALS\n\
                                                                                   \n"

#define SPACE1 40
#define SPACE2 13
#define SPACE3 6
#define MAX_PLAYERS 10

/* --------- File termination codes ------------*/

#define WIN_CODE 'W'
#define FAIL_CODE 'F'
#define QUIT_CODE 'Q'
#define WIN_MESSAGE "WIN"
#define FAIL_MESSAGE "FAIL"
#define QUIT_MESSAGE "QUIT"



/* ------------ DIRECTORIES / FILES ------------*/

#define ROOT_PATH "."
#define GAMES_PATH "./GAMES"
#define SCORES_PATH "./SCORES"
#define SCORES_FILE_PATH_SIZE 40 // ./SCORES/099_099187_DDMMYYYY_HHMMSS.txt
#define WORDS_FILE "word_eng.txt"
#define PLID_DIR_PATH_SIZE 15 // ./GAMES/099187
#define PLID_FILE_PATH_SIZE 26 // ./GAMES/099187/099187.txt
#define FINAL_FILE_PATH_SIZE 37 // ./GAMES/099187/YYYYMMDD_HHMMSS_Q.txt
#define SCORE_FILE_PATH_SIZE 42 // ./SCORES/score_099187_DDMMYYYY_HHMMSS.txt
#define MAX_LINE_SIZE_GAME_FILE 58 // word hint_name (30 1 25 1 1)
#define NO_GAMES_FILE "number_games.txt"
#define SCORE_LINE_SIZE 50 // plid word n_succ n_trials 
#define TIME_LINE_SIZE 20 // DDMMYYYY_HHMMSS.txt
#define SCORE_FILE_SIZE 33 // score_099187_DDMMYYYY_HHMMSS.txt
#define STATE_FILE "STATE_"
#define STATE_FILE_SIZE 17 // STATE_099187.txt
#define TOP_SCORES_FILE "TOPSCORES_"
#define TOP_SCORES_SIZE 22 // TOPSCORES_nnnnnnn.txt
#define TOP_SCORES_LINE_SIZE 85


struct SCORELIST {
    int score[MAX_PLAYERS];
    char PLID[MAX_PLAYERS][PLID_SIZE];
    char word[MAX_PLAYERS][MAX_WORD_SIZE];
    int n_succ[MAX_PLAYERS];
    int n_tot[MAX_PLAYERS];
    int n_scores;
};


// -------------------------- INITIAL ----------------------------- //

int ProcessServerInvocation(int argc, char *argv[], char *gsport);
void CreateAuxFile(int no_words);

// -------------------------- PROTOCOLS ----------------------------- //

void TCPConn(char *gsport, int verbose);
void UDPConn(char *gsport, int verbose);

// ----------------------------- UDP -------------------------------- //
/* Establishes UDP connection with the server. Returns the server response */

int ProcessUDP(char *buffer, char *msg, int verbose);
void ProcessTCP(char *buffer, char *msg, pid_t pid, int newfd, int verbose);


// ------------- FUNCTIONS TO PROCESS CLIENT MESSAGE ---------------- //

int ReadArg(char *message, char *buffer, int size, int precise);


// ------------------------------------------------------------------ //
// ------------------------ UDP COMMANDS ---------------------------- //
// ------------------------------------------------------------------ //

// ------------------------- START COMMAND -------------------------- //

void StartGame(char *plid, char *msg, int verbose);
void PickWord(int index, char *word, char *hint_fname);
int MaxErrors(int size);
int CreateDir(char plid[]);
int CheckIfDirExists(char *dir_path);
int CheckIfFileExists(char *plid, char *path);
void CreateGameFile(char plid[], char word[], char hint_file[]);
int GetNewWordIndex();


// ------------------------- PLAY COMMAND --------------------------- //

void ProcessPlay(char *plid, char *msg, char letter, int trial, int verbose);
void PositionsToString(char *str, int *pos, int no_pos);
int LetterPlayed(char *plid, char letter);
void WriteToGameFile(char plid[], char code, char play[], int correct_play);
void TransferGameFile(char plid[], char code);
int GetErrorsLeft(char *plid);
int IsCorrectLetter(char *plid, char letter);
int WonGame(char *plid);
int GetCorrectPositions(char *plid, char letter, int *positions);



// ------------------------ GUESS COMMAND --------------------------- //

void ProcessGuess(char *plid, char *msg, char *word, int trial, int verbose);


// ------------------------ REVEAL COMMAND -------------------------- //

void RevealWord(char *plid, char *msg);


// ------------------------- QUIT COMMAND --------------------------- //
// ------------------------- EXIT COMMAND --------------------------- //

void CloseGame(char *plid, char *msg, int verbose);




// ------------------------------------------------------------------ //
// ------------------------ TCP COMMANDS ---------------------------- //
// ------------------------------------------------------------------ //

// ------------------------ STATE COMMAND --------------------------- //

void ProcessState(char *plid, char *msg, int fd, int verbose);
int FileNoLines(char path[]);
void MakeStateFile(int no_lines, char plid[], char origin_path[], char dest_path[], int ongoing);
void ReadStateFile(char *buffer, char path[]);
int FindLastGame(char *PLID, char *fname);
void SendTCP(int fd, char *msg, size_t size);


// --------------------- SCOREBOARD COMMAND ------------------------- //

void CreateScoreFile(int n_succ, int n_tot, char word[], char plid[], int verbose);
void ReadScoreInfo(int *n_succ, int *n_tot, char *word, char plid[]);
void ProcessScoreBoard(char *msg, pid_t pid, int fd, int verbose);
int FindTopScores(struct SCORELIST *list);
size_t CreateTopScoresFile(int i_file, int process_id, struct SCORELIST *list, char *msg);


// ------------------------ HINT COMMAND ---------------------------- //

void ProcessHint(char *plid, char *msg, int fd, int verbose);
long int GetFileSize(char path[]);
void ReadHintFile(char path[], int fd);



