#include "GS.h"


extern int errno;


int main(int argc, char *argv[]) {

    /* ./GS word_file [-p GSport] [-v] */
    char gsport[PORT_SIZE]; /* default: 58049 */
    int gs_invok, verbose = 0;
    gs_invok = ProcessServerInvocation(argc, argv, gsport);

    if (gs_invok == -1) exit(1);
    if (gs_invok) verbose = 1;

    /* On server start, creates an aux file with no_words and next word index */
    int no_lines = FileNoLines(WORDS_FILE);
    CreateAuxFile(no_lines);

    pid_t pid2;

    if((pid2 = fork()) == -1) exit(1);
    else if (pid2 == 0) UDPConn(gsport, verbose); /* UDP connection */

    /* TCP conn */
    TCPConn(gsport, verbose);

    return 0;
}



void UDPConn(char *gsport, int verbose) {
    struct addrinfo hints, *res;
    int fd, errcode;
    struct sockaddr_in addr;
    socklen_t addrlen;
    ssize_t n, nread;
    char buffer[MESSAGE_SIZE], msg[MESSAGE_SIZE];

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) exit(1);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((errcode = getaddrinfo(NULL, gsport, &hints, &res)) != 0) exit(1);
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) exit(1);
    while (1) {
        addrlen = sizeof(addr);
        nread = recvfrom(fd, buffer, 128, 0, (struct sockaddr*)&addr, &addrlen);
        if (nread == -1) exit(1);
        buffer[nread] = '\0';

        /* Process UDP */
        ProcessUDP(buffer, msg, verbose);

        n = sendto(fd, msg, strlen(msg), 0, (struct sockaddr*)&addr, addrlen);
        if (n == -1) exit(1);
    }
}


void TCPConn(char *gsport, int verbose) {

    int enable = 1;
    struct sigaction act;
    struct addrinfo hints, *res;
    int fd, newfd, ret;
    ssize_t n, nw = 0;
    struct sockaddr_in addr;
    socklen_t addrlen;
    char buffer[MESSAGE_SIZE];
    pid_t pid;
    /* Protect app against SIGPIPE signals */
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGCHLD, &act, NULL) == -1) exit(1);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) exit(1);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((ret = getaddrinfo(NULL, gsport, &hints, &res)) != 0) exit(1);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int) < 0);
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1) exit(1);
    if (listen(fd, 5) == -1) exit(1);
    freeaddrinfo(res);


    while (1) {
        char msg[MESSAGE_SIZE];
        addrlen = sizeof(addr);
        do newfd = accept(fd, (struct sockaddr*)&addr, &addrlen); // wait for a connection
        while (newfd == -1 && errno == EINTR);
        if (newfd == -1) exit(1);

        if ((pid = fork()) == -1) exit(1);
        else if (pid == 0) {
            // child TCP process
            close(fd);
            /* timer para este read... nao ficar eternamente a espera */
            n = read(newfd, buffer + nw, 128);
            if (n == -1) {
                printf("Error.\n");
                return;
            }
            if (n == -1) exit(1);
            nw += n;

            ProcessTCP(buffer, msg, getpid(), newfd, verbose);

            close(newfd);
            exit(0);
        }
        // parent TCP process
        do ret = close(newfd);
        while (ret == -1 && errno == EINTR);
        if (ret == -1) exit(1);
    }
}



int ProcessUDP(char *buffer, char *msg, int verbose) {

    /* Remove \n from message */
    int buffer_size = strlen(buffer);
    buffer[buffer_size - 1] = '\0';
    --buffer_size;

    char command[COMMAND], plid[PLID_SIZE], word[MAX_WORD_SIZE], trial_str[3];
    char letter;
    int trial, temp, file_exists;
    int bytes_read = 0;
    int new_plid = 1;

    char dir_path[PLID_DIR_PATH_SIZE];

    /* Read command */
    temp = ReadArg(buffer, command, COMMAND - 1, 1);
    if (temp <= 0) {
        sprintf(msg, "%s %s\n", RSG, ERR);
        if (verbose) printf("Could not read initial command.\n");
        return 1;
    }
    bytes_read += temp;

    /* Validate command */
    if (strcmp(command, SNG) && strcmp(command, PLG) && strcmp(command, PWG) && strcmp(command, QUT) && strcmp(command, REV)) {
        sprintf(msg, "%s %s\n", RSG, ERR);
        if (verbose) printf("That command does not match a valid one.\n");
        return 1;
    }

    /* Read PLID */
    temp = ReadArg(buffer + bytes_read, plid, PLID_SIZE - 1, 1);
    if (temp <= 0) return 0;
    bytes_read += temp;

    /* Validate PLID */
    /* check if plid has ongoing game */
    sprintf(dir_path, "%s/%s", GAMES_PATH, plid);

    file_exists = CheckIfFileExists(plid, dir_path);
    int n = !file_exists && !strcmp(command, QUT);
    printf("%d\n", n);
    if (!file_exists && (!strcmp(command, PLG) || !strcmp(command, PWG) || !strcmp(command, QUT))) {
        sprintf(msg, "%s %s\n", RSG, ERR);
        if (verbose) printf("No active game found for player with plid = %s.\n", plid);
        return 1;
    }

    if (buffer[bytes_read] == '\0') {
        if (!strcmp(command, SNG)) {
            if (!file_exists) {
                /* start new game */
                StartGame(plid, msg, verbose);
            } else {
                sprintf(msg, "%s %s\n", RSG, NOK);
                if (verbose) printf("plid = %s already has one ongoing game.\n", plid);
                return 1;
            }
        }
        else if (!strcmp(command, QUT)) {
            /* close game */
            CloseGame(plid, msg, verbose);
        }
        else if (!strcmp(command, REV)) {
            /* reveal word */
            printf("Reveal word\n");
            // RevealWord(plid, msg);
        }
    }
    else {
        /* continue reading */
        if (!strcmp(command, PLG)) {
            temp = ReadArg(buffer + bytes_read, &letter, 1, 1);
            if (temp == -1) {
                sprintf(msg, "%s %s\n", RSG, ERR);
                if (verbose) printf("Error while trying to read letter.\n");
                return 0;
            }
            /* check if is letter or number or something else */
        }
        else if (!strcmp(command, PWG)) {
            temp = ReadArg(buffer + bytes_read, word, MAX_WORD_SIZE - 1, 0);
            if (temp == -1) {
                sprintf(msg, "%s %s\n", RSG, ERR);
                if (verbose) printf("Error while trying to read word.\n");
                return 0;
            }
            /* check if word length makes sense to the game */
            if (strlen(word) < MIN_WORD_SIZE - 1) {
                sprintf(msg, "%s %s\n", RSG, ERR);
                if (verbose) printf("Invalid word size (min length = 3, max length = 30).\n");
                return 0;
            }
        }
        else {
            sprintf(msg, "%s %s\n", RSG, ERR);
            if (verbose) printf("Invalid command.\n");
            return 1;
        }

        bytes_read += temp;
        temp = ReadArg(buffer + bytes_read, trial_str, 2, 0);
        if (temp == -1) {
            sprintf(msg, "%s %s\n", RSG, ERR);
            if (verbose) printf("Error while trying to read trial.\n");
            return 1;
        }
        trial = atoi(trial_str);
        /* validate trial (if makes sense to the game) */
        if (trial > MAX_TRIAL) {
            sprintf(msg, "%s %s\n", RSG, ERR);
            if (verbose) printf("Invalid trial number.\n");
            return 0;
        }

        if (!strcmp(command, PLG)) {
            ProcessPlay(plid, msg, letter, trial, verbose);
        } else if (!strcmp(command, PWG)) {
            ProcessGuess(plid, msg, word, trial, verbose);
        } else {
            sprintf(msg, "%s %s\n", RSG, ERR);
            if (verbose) printf("Invalid command.\n");
            return 1;
        }
    }
    return 1;
}


/* Returns 1 if game file exists; 0 otherwise */
int CheckIfFileExists(char *plid, char *path) {
    char file_path[PLID_FILE_PATH_SIZE];
    sprintf(file_path, "%s/%s.txt", path, plid);
    if (access(file_path, F_OK) == 0) return 1;
    else return 0;
}


/* Returns the number of lines of a file */
int FileNoLines(char path[]) {

    FILE *fd;
    fd = fopen(path, "r");
    int no_lines = 0;
    char buffer[100];

    while (fgets(buffer, 100, fd) != NULL) {
        no_lines++;
    }

    fclose(fd);
    return no_lines;
}


/* Creates file containing no_words and current word_idx = 0 */
void CreateAuxFile(int no_words) {
    char buffer[10];
    FILE *fd;
    fd = fopen(NO_GAMES_FILE, "w");
    if (fd == NULL) {
        return;
    }
    sprintf(buffer, "%d 0\n", no_words);
    fwrite(buffer, sizeof(char), strlen(buffer), fd);

    fclose(fd);
}


/* Reads next idx to get a new word and updates it */
int GetNewWordIndex() {

    FILE *fd;
    char buffer[10];
    int no_words, word_idx = 0;
    int num = 26;
 
    if (access(NO_GAMES_FILE, F_OK) == 0) {
        /* File exists */
        fd = fopen(NO_GAMES_FILE, "r+");
        fgets(buffer, 10, fd);
        sscanf(buffer, "%d %d\n", &no_words, &word_idx);
        if (word_idx == no_words) word_idx = -1;
        sprintf(buffer, "%d %d\n", no_words, word_idx + 1);
        fseek(fd, 0, SEEK_SET);
        fwrite(buffer, sizeof(char), strlen(buffer), fd);
        fclose(fd);
    }

    if (word_idx == -1) return 0;
    return word_idx;
}



/* Creates directory with name = plid (path = ./GAMES/PLID) */
int CreateDir(char plid[]) {
    DIR *dir = opendir(GAMES_PATH);
    int dfd = dirfd(dir);
    errno = 0;

    if (!mkdirat(dfd, plid, 0777)) {
        return 1;
    } else {
        return 0;
    }
}


/* Checks if path ./GAMES/PLID exists */
/* Returns 1 if true; 0 otherwise */
int CheckIfDirExists(char *dir_path) {
    DIR *dir = opendir(dir_path);

    if (dir) {
        /* Directory exists */
        closedir(dir);
        return 1;
    } else if (ENOENT == errno) {
        /* Directory does not exist */
        return 0;
    }
    else {
        /* opendir() failed for some other reason. */
        return 0;
    }
}


/* Creates file with name = PLID.txt (path = ./GAMES/PLID/PLID.txt) */
void CreateGameFile(char plid[], char word[], char hint_file[]) {
    char line[100];
    char fname[PLID_FILE_PATH_SIZE];
    sprintf(line, "%s %s\n", word, hint_file);
    /* GAMES/099187/099187.txt */
    sprintf(fname, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    FILE *fp;

    fp = fopen(fname, "w");
    if (fp == NULL) printf("Could not create file with path = %s\n", fname);
    else {
        fwrite(line, sizeof(char), strlen(line), fp);
    }
    fclose(fp);
}


/* Picks line from word file at index = index */
/* Gets word and hint file name from that line */
void PickWord(int index, char *word, char *hint_fname) {

    FILE *fp;

    fp = fopen(WORDS_FILE, "r");
    if (fp == NULL) {
        return;
    }
    for (int i = 0; i <= index; i++) {
        if (fscanf(fp, "%s %s\n", word, hint_fname) != 2) {
            printf("Error reading the file.\n");
        }
    }
    fclose(fp);
}


/* Returns the number of max errors for a given length of a word */
int MaxErrors(int size) {
    if (size <= 6) { return 7; }
    else if (size >=7 && size <= 10) { return 8; }
    else { return 9; }
}


/* Returns number of attempts left (-1 on error) */
int GetErrorsLeft(char *plid) {

    char word[MAX_WORD_SIZE], path[PLID_FILE_PATH_SIZE], file_line[MAX_LINE_SIZE_GAME_FILE];
    int max_errors, failed_attempts = 0, file_code;
    FILE *fd;

    sprintf(path, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    fd = fopen(path, "r");
    if (fd == NULL) {
        printf("Could not open the file.\n");
        return -1;
    }

    fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd);
    sscanf(file_line, "%s %*s\n", word);
    max_errors = MaxErrors(strlen(word));


    while (fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd) != NULL) {
        sscanf(file_line, "%*c %*s %d\n", &file_code);
        if (!file_code) failed_attempts++;
    }

    fclose(fd);
    return max_errors - failed_attempts;
}


/* Tranfers file relative to finished game from /GAMES to /GAMES/PLID and renames it */
void TransferGameFile(char plid[], char code) {

    char buffer[FINAL_FILE_PATH_SIZE], currname[PLID_FILE_PATH_SIZE], newname[FINAL_FILE_PATH_SIZE];
    time_t t;
    struct tm *timeptr, result;

    /* GAMES/099187/099187.txt */
    sprintf(currname, "%s/%s/%s.txt", GAMES_PATH, plid, plid);

    t = time(NULL);
    timeptr = localtime(&t);
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", timeptr);
    sprintf(buffer, "%s_%c.txt", buffer, code);

    /* GAMES/099187/YYYYMMDD_HHMMSS_(code).txt */
    sprintf(newname, "%s/%s/%s", GAMES_PATH, plid, buffer);

    if(rename(currname, newname)) printf("File could not be renamed.\n");
}


/* Registers valid play in game file of player with plid = plid */
void WriteToGameFile(char plid[], char code, char play[], int correct_play) {
    
    char buffer[MAX_LINE_SIZE_GAME_FILE];
    char fname[PLID_FILE_PATH_SIZE];
    /* T i 0 */
    /* G ola 1 */
    sprintf(buffer, "%c %s %d\n", code, play, correct_play);
    /* GAMES/099187/099187.txt */
    sprintf(fname, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    FILE *fp;

    fp = fopen(fname, "a");
    fwrite(buffer, sizeof(char), strlen(buffer), fp);
    fclose(fp);
}


/* Returns 1 if letter was already played; 0 otherwise */
int LetterPlayed(char *plid, char letter) {

    char path[PLID_FILE_PATH_SIZE], file_line[MAX_LINE_SIZE_GAME_FILE];
    char file_code, file_letter;
    sprintf(path, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    FILE *fd;

    fd = fopen(path, "r");
    if (fd == NULL) {
        printf("Error opening the file.\n");
        return -1;
    }

    if (fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd) == NULL) {
        printf("File is empty.\n");
        return -1;
    }

    while (fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd) != NULL) {
        sscanf(file_line, "%c %c %*d\n", &file_code, &file_letter);
        if (file_code == LETTER_CODE && letter == file_letter) {
            /* Letter already played */\
            return 1;
        }  
    }
    return 0;
}


/* Converts the array of correct positions into a string of space-separated ints */
void PositionsToString(char *str, int *pos, int no_pos) {
    int n = 0;
    for (int i = 0; i < no_pos; i++) {
        n += sprintf(&str[n], "%d ", pos[i]);
    }
    str[n - 1] = '\n';
    str[n] = '\0';
}


/* Returns the number of correct positions and adds them in the array of int passed as an argument */
int GetCorrectPositions(char *plid, char letter, int *positions) {

    char path[PLID_FILE_PATH_SIZE], file_line[MAX_LINE_SIZE_GAME_FILE];
    char file_code, file_letter;
    int n = 0;
    char word[MAX_WORD_SIZE];
    sprintf(path, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    FILE *fd;

    fd = fopen(path, "r");
    if (fd == NULL) {
        printf("Error opening the file.\n");
        return -1;
    }

    if (fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd) == NULL) {
        printf("File is empty.\n");
        return -1;
    }

    fclose(fd);
    sscanf(file_line, "%s %*s\n", word);

    for (int i = 0; i < strlen(word); i++) {
        if (word[i] == letter) {
            positions[n] = i+1;
            n++;
        }
    }
    return n;
}


/* 
Returns the type of play:
GOOD_PLAY is letter is correct
BAD_PLAY if letter is incorrect
LOSE_PLAY if letter is incorrect and 0 attempts left 
*/
int PlayType(char *plid, char letter, int verbose) {

    int errors_left;

    if (IsCorrectLetter(plid, letter)) {
        /* Check if is winning play */
        if (verbose) printf("Correct play.\n");
        return GOOD_PLAY;
    } 
    else {
        /* Check if is losing play */
        errors_left = GetErrorsLeft(plid);
        if (errors_left == 0) {
            /* Game lost */
            if (verbose) printf("Game lost.\n");
            return LOSE_PLAY;
        }
        else {
            /* Bad play */ 
            if (verbose) printf("Bad play. Letter %c incorrect (%d attempts left).\n", letter, errors_left);
            return BAD_PLAY;
        }
    }
}


/* Returns 1 if player won the game; 0 otherwise */
int WonGame(char *plid) {
    char word[MAX_WORD_SIZE], word_user[MAX_WORD_SIZE];
    char file_word[MAX_WORD_SIZE], file_line[MAX_LINE_SIZE_GAME_FILE], path[PLID_FILE_PATH_SIZE];
    char file_code; 
    int word_size;
    sprintf(path, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    FILE *fd;

    fd = fopen(path, "r");
    if (fd == NULL) printf("Could not open the file.\n");

    fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd);
    sscanf(file_line, "%s %*s\n", word);
    word_size = strlen(word);
    memset(word_user, '-', word_size);

    while (fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd) != NULL) {
        sscanf(file_line, "%c %s %*c\n", &file_code, file_word);
        if (file_code == LETTER_CODE && strlen(file_word) == 1) {
            /* letter play */
            for (int i = 0; i < word_size; i++) {
                if (word[i] == file_word[0]) word_user[i] = file_word[0];
            }
        }
    }

    fclose(fd);

    for (int j = 0; j < word_size; j++) {
        /* did not win yet */
        if (word_user[j] == '-') return 0;
    }

    /* won the game */
    return 1;
}


/* Returns 1 if letter is part of the word; 0 otherwise */
int IsCorrectLetter(char *plid, char letter) {

    char path[PLID_FILE_PATH_SIZE], word[MAX_WORD_SIZE], file_line[SCORE_LINE_SIZE];
    sprintf(path, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    char code;
    char positions_str[60];
    FILE *fd;

    fd = fopen(path, "r");
    if (fd == NULL) printf("File error.\n");

    fgets(file_line, SCORE_LINE_SIZE, fd);
    sscanf(file_line, "%s %*s\n", word);

    for (int i = 0; i < strlen(word); i++) {
        if (word[i] == letter) return 1;
    }

    fclose(fd);
    return 0;
}


void ProcessPlay(char *plid, char *msg, char letter, int trial, int verbose) {

    int play_result, is_winning_play = 0, no_hits = 0;
    int positions[POSITIONS];
    char positions_str[POSITIONS];
    char letter_str[2];
    letter_str[0] = letter;
    letter_str[1] = '\0';

    char path[PLID_FILE_PATH_SIZE];
    sprintf(path, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    int file_trial = FileNoLines(path); 

    if (trial != file_trial) {
        if (verbose) printf("Client and server trial numbers do not match.\n");
        sprintf(msg, "%s %s\n", RLG, INV);
    }
    else if (LetterPlayed(plid, letter)) {
        sprintf(msg, "%s %s %d\n", RLG, DUP, trial);
        if (verbose) printf("Letter %c already played.\n", letter);
        /* Trial number is not increased ! */
    }
    else {
        play_result = PlayType(plid, letter, verbose);
        if (play_result == GOOD_PLAY) {
            /* Check if is winning play */
            WriteToGameFile(plid, LETTER_CODE, letter_str, CORRECT_PLAY);
            if (WonGame(plid)) {
                int n_succ = 0, n_tot = 0;
                char word[MAX_WORD_SIZE];
                sprintf(msg, "%s %s %d\n", RLG, WIN, trial);
                /* tranferir ficheiro com W de win */
                ReadScoreInfo(&n_succ, &n_tot, word, plid);
                CreateScoreFile(n_succ, n_tot, word, plid, verbose);
                TransferGameFile(plid, WIN_CODE);
                if (verbose) printf("Letter is correct. Game won.\n");
            } else {
                /* Check no_hits and positions */
                no_hits = GetCorrectPositions(plid, letter, positions);
                if (!no_hits && verbose) printf("0 correct positions: error.\n");
                PositionsToString(positions_str, positions, no_hits);
                sprintf(msg, "%s %s %d %d %s\n", RLG, OK, trial, no_hits, positions_str);
                if (verbose) printf("play letter '%s' - %d hits; word not guessed\n", letter_str, no_hits);
            }
        }
        else if (play_result == LOSE_PLAY) {
            WriteToGameFile(plid, LETTER_CODE, letter_str, INCORRECT_PLAY);
            sprintf(msg, "%s %s %d\n", RLG, OVR, trial);
            /* tranferir ficheiro com F de fail */
            TransferGameFile(plid, FAIL_CODE);
            /* close the game ? reset struct ? */
            if (verbose) printf("Letter is incorrect. Game lost.\n");
        }
        else if (play_result == BAD_PLAY) {
            WriteToGameFile(plid, LETTER_CODE, letter_str, INCORRECT_PLAY);
            sprintf(msg, "%s %s %d\n", RLG, NOK, trial);
            if (verbose) printf("play letter '%s' - 0 hits; word not guessed\n", letter_str);
        }
        else {
            if (verbose) printf("Error. ProcessPlay().\n");
        }
    }
}


/* Reads a word of max size = size or, if precise = 1, size = size */
/* Returns the number of bytes read on success; -1 on error */
int ReadArg(char *message, char *buffer, int size, int precise) {
    int i = 0;
    while (message[i] != ' ' && message[i] != '\0' && i < size) {
        buffer[i] = message[i];
        i++;
    }

    if (i == 0 && message[i] == '\0') {
        /* Nothing more to be read */
        return 0;
    }

    else if (buffer[i-1] != ' ' && (message[i] == ' ' || message[i] == '\0' || message[i] == '\n')  && ((i == size && precise) || (i <= size && !precise))) {
        /* Sucessfully read */
        buffer[i] = '\0';
        return i + 1;
    }

    else if (message[i] != ' ' || message[i] != '\0') {
        /* Error */
        return -1;
    }
    /* Error */
    return -1;
}


/* 
Creates a file with the score information (n_succ, n_tot and word) 
for the game won by the player with id = plid 
*/
void CreateScoreFile(int n_succ, int n_tot, char word[], char plid[], int verbose) {

    FILE *fd_score;
    char buffer[TIME_LINE_SIZE], score_fname[SCORES_FILE_PATH_SIZE], file_content[SCORE_LINE_SIZE];
    time_t t;
    int score;
    float n_succ_f = n_succ, n_tot_f = n_tot;
    char score_str[4];
    struct tm *timeptr;
    t = time(NULL);
    timeptr = localtime(&t);


    strftime(buffer, sizeof(buffer), "%d%m%Y_%H%M%S.txt", timeptr);

    score = roundf(n_succ_f / n_tot_f * 100);
    if (score < 10) sprintf(score_str, "00%d", score);
    else if (score < 100) sprintf(score_str, "0%d", score);
    else sprintf(score_str, "%d", score);
    sprintf(score_fname, "%s/%s%s_%s", SCORES_PATH, score_str, plid, buffer);

    fd_score = fopen(score_fname, "w");
    if (fd_score == NULL) printf("Could not create score file.\n");
    // PLID palavra n_succ n_trials
    sprintf(file_content, "%s %s %s %d %d\n", score_str, plid, word, n_succ, n_tot);
    if (verbose) printf("Score file created for player with plid = %s\n", plid);
    fwrite(file_content, sizeof(char), strlen(file_content), fd_score);

    fclose(fd_score);
}


/* Extracts n_succ, n_tot and word of ongoing game of the player with id = plid */
/* Called when a player wins (either by playing letter or guessing word) */
void ReadScoreInfo(int *n_succ, int *n_tot, char *word, char plid[]) {

    char file_line[MAX_LINE_SIZE_GAME_FILE], plid_fname[PLID_FILE_PATH_SIZE];
    int sucess_code, is_guess_last = 0;
    char code;
    sprintf(plid_fname, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    FILE *fd, *fd_score;
    fd = fopen(plid_fname, "r");

    /* Consume first line - word and hint file */
    fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd);
    sscanf(file_line, "%s %*s", word);
    /* Loop through plays */
    while (fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd) != NULL) {
        sscanf(file_line, "%c %*s %d\n", &code, &sucess_code);
        *n_tot += 1;
        if (code == LETTER_CODE && sucess_code == CORRECT_PLAY) {
            *n_succ += 1;
            is_guess_last = 0;
        }
        else if (code == GUESS_CODE) is_guess_last = 1;
    }

    if (is_guess_last) *n_succ += 1;
    fclose(fd);
}


void StartGame(char *plid, char *msg, int verbose) {

    /* Getting here, we know that there is no ongoing game for player */
    
    /* S_IRWXU */
    char word[MAX_WORD_SIZE], hint_fname[30];
    int no_words, new_word_idx, n_letters, max_errors;
    char dir_path[PLID_DIR_PATH_SIZE];
    sprintf(dir_path, "%s/%s", GAMES_PATH, plid);
   /*
    --> Everytime a new game starts:
    1) Get next word index -> GetNewWordIndex()
    2) Get word from word_eng.txt file -> PickWord()
    */
    new_word_idx = GetNewWordIndex();
    PickWord(new_word_idx, word, hint_fname);

    /*
    3) Create directory in ./GAMES folder (if it is first game of this player)
    4) Create file for new game in ./GAMES/PLID folder
    */
    if(!CheckIfDirExists(dir_path)) {
        CreateDir(plid);
    }
    CreateGameFile(plid, word, hint_fname);


    /* 5) Get word size and max errors for that word */
    n_letters = strlen(word);
    max_errors = MaxErrors(n_letters);

    if (verbose) printf("PLID=%s: new game; word= \"%s\" (%d letters)", plid, word, n_letters);
    sprintf(msg, "%s %s %d %d\n", RSG, OK, n_letters, max_errors);
}


void ProcessGuess(char *plid, char *msg, char *word, int trial, int verbose) {
    char path[PLID_FILE_PATH_SIZE], file_line[MAX_LINE_SIZE_GAME_FILE], file_word[MAX_WORD_SIZE];
    sprintf(path, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    int file_trial = FileNoLines(path); 
    int errors_left;
    FILE *fd;

    fd = fopen(path, "r");
    if (fd == NULL) {
        printf("Error opening the file.\n");
    }

    if (fgets(file_line, MAX_LINE_SIZE_GAME_FILE, fd) == NULL) {
        printf("Error reading the file.\n");
    }

    sscanf(file_line, "%s %*s\n", file_word);


    if (trial != file_trial) {
        if (verbose) printf("Client and server trial numbers do not match.\n");
        sprintf(msg, "%s %s %d\n", RWG, INV, trial);
    }
    else if (!strcmp(word, file_word)) {
        int n_succ = 0, n_tot = 0;
        sprintf(msg, "%s %s %d\n", RWG, WIN, trial);
        WriteToGameFile(plid, GUESS_CODE, word, CORRECT_PLAY);
        ReadScoreInfo(&n_succ, &n_tot, file_word, plid);
        CreateScoreFile(n_succ, n_tot, file_word, plid, verbose);
        /* tranferir ficheiro com W de win */
        TransferGameFile(plid, WIN_CODE);
        /* resets, acabar jogo ... */
        // CloseGame()
        if (verbose) printf("Word '%s' is correct. Game won.\n", word);
    }
    else {
        /* Increase trial number -> corresponde ao anotar da play no ficheiro */
        errors_left = GetErrorsLeft(plid);
        WriteToGameFile(plid, GUESS_CODE, word, INCORRECT_PLAY);
        if (errors_left == 0) {
            sprintf(msg, "%s %s %d\n", RWG, OVR, trial);
            /* tranferir ficheiro com F de fail */
            TransferGameFile(plid, FAIL_CODE);
            if (verbose) printf("Word '%s' is not correct. Game over.\n", word);
        }
        else if (errors_left == -1) {
            /* error occurred */
            if (verbose) printf("Error. ProcessGuess()\n");
        }
        else {
            sprintf(msg, "%s %s %d\n", RWG, NOK, trial);
            if (verbose) printf("Word '%s' is not correct. You still have %d errors left.\n", word, errors_left);
        }
    }
}


void CloseGame(char *plid, char *msg, int verbose) {
    /* Rename file and add code = QUIT */
    TransferGameFile(plid, QUIT_CODE);
    if (verbose) printf("PLID=%s: game closed\n", plid);
    sprintf(msg, "%s %s\n", RQT, OK);
}


/* Checks format and validity of server invocation */
int ProcessServerInvocation(int argc, char *argv[], char *gsport) {

    /* ./GS word_file [-p GSport] [-v] */
    /* ./player [-n GSIP] [-p GSport] */

    if (argc < 2) {
        printf("Few arguments. Format shoud be: ./GS word_file [-p GSport] [-v]\n");
        return -1;
    }

    if (argc == 2) {
        if (strcmp(argv[1], WORDS_FILE)) {
            printf("Wrong word_file.\n");
            return -1;
        } else strcpy(gsport, GSPORT_D);
    }

    else if (argc == 3) {
        if (!strcmp(argv[1], WORDS_FILE) && !strcmp(argv[2], "-v")) {
            strcpy(gsport, GSPORT_D);
            return 1;
        }
        else {
            printf("Wrong word_file and/or verbose flag.\n");
            return -1;
        }       
    }

    else if (argc == 4 && !strcmp(argv[2], "-p")) {
        if (atoi(argv[3]) > PORT_MIN && atoi(argv[3]) < PORT_MAX) {
            strcpy(gsport, argv[3]);
        } 
        else {
            printf("Invalid port number.\n");
            return -1;
        }
    }

    else if (argc == 5 && !strcmp(argv[2], "-p") && !strcmp(argv[4], "-v")) {
        if (atoi(argv[3]) > PORT_MIN && atoi(argv[3]) < PORT_MAX) {
            strcpy(gsport, argv[3]);
            return 1;
        } 
        else {
            printf("Invalid port number.\n");
            return -1;
        }
    }
    return 0;  
}


void ProcessTCP(char *buffer, char *msg, pid_t pid, int newfd, int verbose) {

    /* Remove \n from message */
    int buffer_size = strlen(buffer);
    buffer[buffer_size - 1] = '\0';
    --buffer_size;

    char command[COMMAND], plid[PLID_SIZE], word[MAX_WORD_SIZE], trial_str[3];
    char letter;
    int trial, no_lines, ongoing_game;
    int bytes_read = 0;
    int new_plid = 1;
    int temp, file_exists, sb_len;
    char buffer_aux[1024];

    char dir_path[PLID_DIR_PATH_SIZE];

    /* Read command */
    temp = ReadArg(buffer, command, COMMAND - 1, 1);
    if (temp <= 0) {
        sprintf(msg, "%s %s\n", RSG, ERR);
        if (verbose) printf("Could not read initial command.\n");
        return;
    }
    bytes_read += temp;

    /* Validate command */
    if (strcmp(command, SNG) && strcmp(command, STA) && strcmp(command, GHL) && strcmp(command, GSB)) {
        sprintf(msg, "%s %s\n", RSG, ERR);
        if (verbose) printf("That command does not match a valid one.\n");
        return;
    }

    if (buffer[bytes_read] == '\0') {
        if (!strcmp(command, GSB)) {
            ProcessScoreBoard(msg, pid, newfd, verbose);
        }
        else {
            /* Error */
            sprintf(msg, "%s %s\n", RSG, ERR);
            if (verbose) printf("Command with only one argument can only be GSB.\n");
            return;
        }
    }

    /* Read PLID */
    temp = ReadArg(buffer + bytes_read, plid, PLID_SIZE - 1, 1);
    if (temp <= 0) return;
    bytes_read += temp;

    /* Validate PLID */
    /* check if plid has ongoing game */
    sprintf(dir_path, "%s/%s", GAMES_PATH, plid);


    /* if there's more to read, then the command is wrong */
    if (buffer[bytes_read] != '\0') {
        /* Error */
        sprintf(msg, "%s %s\n", RSG, ERR);
        if (verbose) printf("Wrong command.\n");
        return;
    }

    char path[PLID_DIR_PATH_SIZE];
    sprintf(path, "%s/%s", GAMES_PATH, plid);
    ongoing_game = CheckIfFileExists(plid, path);

    if (!strcmp(command, GHL)) {
        /* hint command */
        ProcessHint(plid, msg, newfd, verbose);
    }
    else if (!strcmp(command, STA)) {
        ProcessState(plid, msg, newfd, verbose);
    }
    else {
        /* Error */
        sprintf(msg, "%s %s\n", RSG, ERR);
        if (verbose) printf("Command with two arguments can only be of type GHL or STA.\n");
        return;
    }
}


/* Creates file with top scores, following sb command received */
size_t CreateTopScoresFile(int i_file, int process_id, struct SCORELIST *list, char *msg) {

    FILE *fd;
    int word_size, line_size;
    char topscores_line[TOP_SCORES_LINE_SIZE], trial_str[4];
    strcpy(msg, TOP_10_SCORES);
    char first_line[252];
    char path[TOP_SCORES_SIZE];
    char score_str[4];
    int score;
    sprintf(path, "%s%d.txt", TOP_SCORES_FILE, process_id);
    int j, k, l;

    fd = fopen(path, "w");
    if (fd == NULL) {
        printf("File error.\n");
        return -1;
    }

    strcpy(first_line, TOP_10_SCORES);
    fwrite(first_line, sizeof(char), strlen(first_line), fd);

    for (int i = 0; i < i_file; i++) {
        word_size = strlen(list->word[i]);
        score = list->score[i];
        if (score < 10) sprintf(score_str, "00%d", score);
        else if (score < 100) sprintf(score_str, "0%d", score);
        else sprintf(score_str, "%d", score); 

        sprintf(topscores_line, " %d - %s %s %s", i+1, score_str, list->PLID[i], list->word[i]);
        strcat(msg, topscores_line);
        fwrite(topscores_line, sizeof(char), strlen(topscores_line), fd);

        /* Write spaces */
        for (j = 0; j < SPACE1 - word_size; j++) topscores_line[j] = ' ';
        topscores_line[j] = '\0';
        fwrite(topscores_line, sizeof(char), strlen(topscores_line), fd);
        strcat(msg, topscores_line);

        // Write n_succ
        sprintf(trial_str, "%d", list->n_succ[i]);
        fwrite(trial_str, sizeof(char), strlen(trial_str), fd);
        strcat(msg, trial_str);

        /* Write spaces */
        for (k = 0;  k < SPACE2; k++) topscores_line[k] = ' ';
        topscores_line[k] = '\0';
        fwrite(topscores_line, sizeof(char), strlen(topscores_line), fd);
        strcat(msg, topscores_line);

        // Write n_tot
        sprintf(trial_str, "%d", list->n_tot[i]);
        fwrite(trial_str, sizeof(char), strlen(trial_str), fd);
        strcat(msg, trial_str);

        /* Write spaces */
        for (l = 0; l < SPACE3; l++) topscores_line[l] = ' ';
        topscores_line[l] = '\n';
        topscores_line[l+1] = '\0';
        fwrite(topscores_line, sizeof(char), strlen(topscores_line), fd);
        strcat(msg, topscores_line);

        
    }

    /* Write final \n */
    size_t msg_size = strlen(msg);
    msg[msg_size] = '\n';
    msg[msg_size + 1] = '\0';

    fclose(fd);

    /* Delete scoreboard file */
    remove(path);


    return ++msg_size;
}


int FindTopScores(struct SCORELIST *list) {

    struct dirent **filelist;
    int n_entries, i_file;
    char fname[50];
    FILE *fp;

    n_entries = scandir("SCORES/", &filelist, 0, alphasort);

    i_file = 0;
    if (n_entries < 0) return(0);
    else {
        while (n_entries--) {
            if (strcmp(&filelist[n_entries]->d_name[0], ".")) {
                sprintf(fname, "SCORES/%s", filelist[n_entries]->d_name);
                fp = fopen(fname, "r");
                if(fp!=NULL) {
                    fscanf(fp, "%d %s %s %d %d", &list->score[i_file], list->PLID[i_file], 
                    list->word[i_file], &list->n_succ[i_file], &list->n_tot[i_file]);
                    fclose(fp);
                    ++i_file;
                }
            }

            free(filelist[n_entries]);
            if (i_file == 10) break;
        }

        free(filelist);
    }

    list->n_scores = i_file;
    return(i_file);
}


/* Creates a state file with path = dest_path from plid game file with path = origin_path */
/* ongoing = 1 if player has active game; ongoing = 0 otherwise */
void MakeStateFile(int no_lines, char plid[], char origin_path[], char dest_path[], int ongoing) {

    FILE *fd, *fd_state;
    char buffer[MAX_LINE_SIZE_GAME_FILE], word[MAX_WORD_SIZE], word_copy[MAX_WORD_SIZE],hint_fname[FILENAME];
    char msg[128];
    char file_code[2], file_word[MAX_WORD_SIZE];
    char last_line[100];
    char termination_code;
    int word_size, file_success_code;

    fd = fopen(origin_path, "r");
    fd_state = fopen(dest_path, "w");

    /* Get first line of game file */
    fgets(buffer, MAX_LINE_SIZE_GAME_FILE, fd);
    sscanf(buffer, "%s %s\n", word, hint_fname);
    word_size = strlen(word);
    memset(word_copy, '-', word_size);


    /* Writes header of file (different is game is active or terminated) */
    if (!ongoing) {
        sprintf(msg, "Last finalized game for player %s\nWord: %s; Hint file: %s\n--- Transactions found: %d ---\n", plid, word, hint_fname, no_lines - 1);
    } else sprintf(msg, "Active game found for player %s\n--- Transactions found: %d ---\n", plid, no_lines - 1);

    fwrite(msg, sizeof(char), strlen(msg), fd_state);

    // printf("Buffer content after reading line of file: %s\n", buffer);
    // printf("Initial message: %s\n", msg);


    /* Loop to read plays from game file */
    for (int i = 0; i < no_lines - 1; i++) {
        fgets(buffer, MAX_LINE_SIZE_GAME_FILE, fd);
        sscanf(buffer, "%s %s %d", file_code, file_word, &file_success_code);

        /* Write to state file formatted line */
        if (strlen(file_word) == 1 && file_code[0] == LETTER_CODE) {
            if (file_success_code) sprintf(msg, "Letter trial: %s - %s\n", file_word, CORRECT_PLAY_ST);
            else sprintf(msg, "Letter trial: %s - %s\n", file_word, INCORRECT_PLAY_ST);
            /* Fill the word_copy if game is active */
            if (ongoing) {
                for (int j = 0; j < word_size; j++) {
                    if (word[j] == file_word[0]) word_copy[j] = file_word[0];
                }
            }
        } else sprintf(msg, "Word guess: %s\n", file_word);

        fwrite(msg, sizeof(char), strlen(msg), fd_state);
    }

    /* Compose last part of the message */
    if (ongoing) {
        /* Get formatted string of word solved so far */
        sprintf(last_line, "%s: %s\n", SOLVED_SO_FAR, word_copy);
    }
    else {
        /* Get termination status from file name */
        termination_code = origin_path[strlen(origin_path) - 5];
        if (termination_code == WIN_CODE) sprintf(last_line, "%s: %s\n", TERMINATION, WIN_MESSAGE);
        else if (termination_code == FAIL_CODE) sprintf(last_line, "%s: %s\n", TERMINATION, FAIL_MESSAGE);
        else if (termination_code == QUIT_CODE) sprintf(last_line, "%s: %s\n", TERMINATION, QUIT_MESSAGE);
    }

    /* Write footer of the message to file */
    fwrite(last_line, sizeof(char), strlen(last_line), fd_state);

    fclose(fd);
    fclose(fd_state);
}


/* Returns 1 if a previous game (from player with plid = PLID) was found; 0 otherwise */
int FindLastGame(char *PLID, char *fname) {
    struct dirent **filelist; 
    int n_entries, found ;
    char dirname[20];
    sprintf(dirname, "GAMES/%s/", PLID);
    n_entries = scandir(dirname, &filelist, 0, alphasort);
    found = 0;

    if (n_entries <= 0) {
        return 0;
    }
    else {
        while (n_entries--) {
            if (filelist[n_entries]->d_name[0]!='.') {
                sprintf(fname, "GAMES/%s/%s", PLID, filelist[n_entries]->d_name);
                found = 1;
            }
            free(filelist[n_entries]);
            if (found) break;
        }
        free(filelist);
    }
    return(found);
}


/* Reads state file content to a buffer */
void ReadStateFile(char *buffer, char path[]) {

    FILE *fd;
    fd = fopen(path, "r");
    size_t bytes_read = 0;

    while (fgets(buffer + bytes_read, 100, fd) != NULL) {
        bytes_read = strlen(buffer);
    }

    fclose(fd);
}



/* Writes msg with size = size to socket with fd = fd */
void SendTCP(int fd, char *msg, size_t size) {
    size_t left_to_read = size, n;

    while (left_to_read > 0) {
        n = write(fd, msg, 128);
        if (n >= 0) left_to_read -= n;
    }
}


void ProcessScoreBoard(char *msg, pid_t pid, int fd, int verbose) {

    struct SCORELIST list;
    int sb_len;
    size_t msg_size;
    char filename[FILENAME];
    char buffer_aux[1024];
    sprintf(filename, "%s%ld.txt", TOP_SCORES_FILE, (long) pid);
    sb_len = FindTopScores(&list) - 1;
    if (sb_len == 0) {
        /* Empty scoreboard */
        sprintf(msg, "%s %s\n", RSB, EMPTY);
        if (verbose) printf("Empty scoreboard.\n");
    }
    else {
        msg_size = CreateTopScoresFile(sb_len, pid, &list, buffer_aux);
        if (msg_size == -1) {
            if (verbose) printf("Error ProcessScoreBoard().\n");
            return;
        }
        sprintf(msg, "%s %s %s %lu %s", RSB, OK, filename, msg_size, buffer_aux);
        if (verbose) printf("send scoreboard file '%s' (%lu)\n", filename, msg_size);
        SendTCP(fd, msg, strlen(msg));
    }
}


void ProcessState(char *plid, char *msg, int fd, int verbose) {

    int no_lines = 0;
    char state_fname[17];
    char plid_fname[PLID_FILE_PATH_SIZE];
    sprintf(plid_fname, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    char buffer[512];
    char origin_fname[FINAL_FILE_PATH_SIZE];

    int ongoing;
    /* Check if plid file exists */
    /* file exists */
    if (access(plid_fname, F_OK) == 0) {
        ongoing = 1;
        strcpy(origin_fname, plid_fname);
    }
    /* file does not exist */
    /* search last terminated game (if exists) */
    else if (FindLastGame(plid, origin_fname)) ongoing = 0;
    else {
        sprintf(msg, "%s %s\n", RST, NOK);
        if (verbose) printf("No games found (active or finished) for player with plid = %s", plid);
        return;
    }

    no_lines = FileNoLines(origin_fname);
    /* create state file */
    sprintf(state_fname, "STATE_%s.txt", plid);

    /* Convert struct into useful info to state file */
    MakeStateFile(no_lines, plid, origin_fname, state_fname, ongoing);
    /* Read file content to buffer */
    ReadStateFile(buffer, state_fname);

    size_t buffer_size = strlen(buffer);
    /* Create message to send to the client */
    if (ongoing) sprintf(msg, "%s %s %s %lu %s", RST, ACT, state_fname, buffer_size, buffer);
    else sprintf(msg, "%s %s %s %lu %s\n", RST, FIN, state_fname, buffer_size, buffer);

    if (verbose) printf("send state file '%s' (%lu bytes)\n", state_fname, buffer_size);
    SendTCP(fd, msg, strlen(msg));

    /* Delete state file */
    remove(state_fname);

}


/* Process hint request from player with plid = plid and socket = fd */
void ProcessHint(char *plid, char *msg, int client_fd, int verbose) {

    char fname[PLID_FILE_PATH_SIZE], file_line[MAX_LINE_SIZE_GAME_FILE], hint_fname[30];
    sprintf(fname, "%s/%s/%s.txt", GAMES_PATH, plid, plid);
    long int hint_fsize;
    FILE *fd;

    /* Check if plid file exists */
    if (access(fname, F_OK) == 0) {
        /* file exists */
        fd = fopen(fname, "r");
        fgets(file_line, 100, fd);
        /* Get hint file name */
        sscanf(file_line, "%*s %s\n", hint_fname);
        fclose(fd);

        /* Get hint file size */
        hint_fsize = GetFileSize(hint_fname);

        /* Create message for client */
        sprintf(msg, "%s %s %s %ld ", RHL, OK, hint_fname, hint_fsize);
        write(client_fd, msg, strlen(msg));

        /* Read chunks of data from hint file */
        ReadHintFile(hint_fname, client_fd);

        if (verbose) printf("send hint file '%s' (%ld)\n", hint_fname, hint_fsize);
    }
    else {
        /* file does not exist */
        sprintf(msg, "%s %s\n", RHL, NOK);
        if (verbose) printf("No file to be sent or some other error occurred.\n");
    }
}


/* Reads hint file and writes it to socket */
void ReadHintFile(char path[], int client_fd) {

    ssize_t bytes_read = 0;
    size_t to_write, n;
    char buffer[BLOCK_SIZE];
    FILE *fd;

    fd = fopen(path, "r");

    if (fd == NULL) {
        printf("Could not open file.\n");
        fclose(fd);
        return;
    }

    while (1) {
        bytes_read = fread(buffer, 1, sizeof(buffer), fd);
        if (bytes_read == -1) {
            printf("Error reading the file.\n");
            break;
        }
        else if (bytes_read == 0) {
            /* End of file reached. Nothing more to read */
            break;
        }

        to_write = BLOCK_SIZE;
        /* Write to socket */
        while (to_write > 0) {
            n = write(client_fd, buffer, BLOCK_SIZE);
            to_write -= n;
        }
    }
    fclose(fd);
}


/* Returns the size of a file with path = path (long int) */
long int GetFileSize(char path[]) {
    FILE *fd;
    fd = fopen(path, "r");

    fseek(fd, 0L, SEEK_END);
    long int size = ftell(fd);

    fclose(fd);
    return size;
}