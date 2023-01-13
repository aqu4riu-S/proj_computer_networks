#include "player.h"

int main(int argc, char *argv[]) {

    char GSIP[128], GSPORT[8];
    ProcessGameInvocation(argc, argv, GSIP, GSPORT);

    char str[COMMAND_SIZE], buffer[BUFFER_SIZE], message[PWG_SIZE];
    struct ClientInfo client;
    strcpy(client.plid, "");
    client.trial = 0;
    client.on_going = 0;

    while(1) {
        fgets(str, COMMAND_SIZE, stdin);
        /* Ignore enter key */
        if (str[0] == '\n') {
            printf("Wrong command.\n");
            continue;
        } 
        str[strlen(str)-1] = '\0';

        int str_size = strlen(str);

        int valid_input;
        valid_input = ProcessInput(message, str, &client);

        if (valid_input) {
            if (!strcmp(client.command, SCOREBOARD) || !strcmp(client.command, SB) || !strcmp(client.command, HINT) ||
            !strcmp(client.command, H) || !strcmp(client.command, STATE) || !strcmp(client.command, ST)) {
                TcpConnection(message);
            }
            else {
                UdpConnection(message, buffer);
                if(DecodeServerMessage(buffer, &client)) {
                    if (!strcasecmp(client.command, EXIT)) {
                        printf("%s", EXIT_MSG);
                        exit(0);
                    }
                    if (!strcasecmp(client.command, QUIT)) {
                        client.on_going = 0; 
                    }
                    /* Reset struct relative to client's previous game */
                    char plid_aux[PLID_SIZE];
                    strcpy(plid_aux, client.plid);
                    client = (const struct ClientInfo) { 0 };
                    strcpy(client.plid, "");
                    strcpy(client.plid, plid_aux);

                buffer[0] = '\0';
                } 
            }
        } else printf("Empty message. Wrong command.\n");
    }
    return 0;
}



/* Establishes UDP connection with the server. Returns the server response */
void UdpConnection(char message[128], char *buffer) {

    int fd, errcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;


    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd==-1) exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    // localhost
    errcode = getaddrinfo(GSIP_D, GSPORT_D, &hints, &res);
    if(errcode!=0) exit(1);

    n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
    if(n==-1) exit(1);

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, 128, 0, (struct sockaddr*)&addr, &addrlen);
    if (n < 0) {
        /* Timeout reached */
        printf("No server response was received within the timeout window (5 seconds). Please, try again.\n");
        n = 0;
    }
    else if (n==-1) exit(1);
    buffer[n] = '\0';

    freeaddrinfo(res);
    close(fd);
}


/* Fills vector of ints with the positions where the letter is present. Returns the number of hits */
int ProcessRLGOK(char *message, int *positions) {

    char command[4], status[3];
    int play_no, no_hits;
    int offset = 0;

    if (sscanf(message, "%s %s %d %d %n", command, status, &play_no, &no_hits, &offset) != 4) {
        printf("ERR\n");
    }

    char *p = message + offset;

    for (int prev = 0; prev < no_hits; prev++) {
        if (sscanf(p, "%d %n", &positions[prev], &offset) != 1) {
            printf("ERR\n");
        }
        p += offset;
    }
    return no_hits;
}


/* Returns 1 if keyboard input is valid; 0 otherwise */
int ProcessInput(char *message, char *input, struct ClientInfo *client) {
    char action[FIRST_ACTION_SIZE], arg2[100];
    char delim[] = " ";
    char *ptr = strtok(input, delim);
    int valid_command = 1;

    strcpy(action, ptr);
    ptr = strtok(NULL, delim);
    if (ptr == NULL) {
        /* Check one argument commands */
        if (!strcasecmp(action, SCOREBOARD) || !strcasecmp(action, SB)) {
            /* scoreboard command */
            sprintf(message, "%s\n", GSB);
        } else if (!strcasecmp(action, HINT) || !strcasecmp(action, H)) {
            if (!strcmp(client->plid, "")) {
                    printf("Trying to access hint without any ongoing game.\n");
                    valid_command = 0;
            }
            /* hint command */
            sprintf(message, "%s %s\n", GHL, client->plid);
        } else if (!strcasecmp(action, STATE) || !strcasecmp(action, ST)) {
            if (!strcmp(client->plid, "")) {
                    printf("Trying to access state without a plid.\n");
                    valid_command = 0;
            }
            /* state command */
            sprintf(message, "%s %s\n", STA, client->plid);
        } else if (!strcasecmp(action, QUIT) || !strcasecmp(action, EXIT)) {
            if (!strcmp(client->plid, "")) {
                /* Trying to quit when there is no ongoing game */
                if (!strcasecmp(action, QUIT)) {
                    printf("Trying to quit a non existing game.\n");
                    valid_command = 0;
                }
                /* Trying to exit with no ongoing game -> just exits app; no msg sent to server */ 
                else if (!strcasecmp(action, EXIT)) {
                    printf("%s", EXIT_MSG);
                    exit(0);
                }
            }
            /* trying to exit when already quitted game -> just exits app; no msg sent to server */
            else if (!strcasecmp(action, EXIT) && client->on_going == 0) {
                printf("%s", EXIT_MSG);
                exit(0);
            }
            sprintf(message, "%s %s\n", QUT, client->plid);
        } else if (!strcasecmp(action, REVEAL)) {
            sprintf(message, "%s %s\n", REV, client->plid);
        }
        else {
            printf("Wrong command\n"); 
            valid_command = 0;
        }
    } else {
        strcpy(arg2, ptr);
        ptr = strtok(NULL, delim);
        if (ptr != NULL) {
            printf("Wrong command.\n");
            valid_command = 0;
        }
        else {
            int size_arg2 = strlen(arg2);
            /* Check two argument commands */
            if ((!strcasecmp(action, PLAY) || !strcasecmp(action, PL)) && size_arg2 == 1 && isalpha(arg2[0])) {
                /* play command */
                client->letter = arg2[0];
                sprintf(message, "%s %s %c %d\n", PLG, client->plid, client->letter, client->trial);
            } else if ((!strcasecmp(action, START) || !strcasecmp(action, SG)) && size_arg2 == PLID_SIZE - 1 && IsNumber(arg2)) {
                /* start command */
                strcpy(client->plid, arg2);
                sprintf(message, "%s %s\n", SNG, client->plid);
            } else if ((!strcasecmp(action, GUESS) || !strcasecmp(action, GW)) && IsWord(arg2) && IsValidWordSize(size_arg2)) {
                /* guess command */
                strcpy(client->word_guess, arg2);
                sprintf(message, "%s %s %s %d\n", PWG, client->plid, arg2, client->trial);
            } else {
                printf("Wrong command\n"); 
                valid_command = 0;
            }
        }
    }
    if (valid_command) strcpy(client->command, action);
    return valid_command;
}


/* Translates server response into an adequate output message */
int DecodeServerMessage(char *message, struct ClientInfo *client) {

    /* Remove \n from message */
    int message_size = strlen(message);
    if (message_size == 0) return 0;
    message[message_size - 1] = '\0';
    --message_size;

    char command[4], status[MAX_WORD_SIZE];
    int offset = 0, increase_trial = 0, quit_ok = QUIT_EXIT_NOK;

    if (sscanf(message, "%s %s %n", command, status, &offset) != 2) printf("ERR\n");

    char *p = message + offset;

    if (!strcmp(command, RSG)) {
        /* RSG status [n_letters max_errors] */
        ProcessRSG(status, message_size, &increase_trial, p, client);
    } else if (!strcmp(command, RLG)) {
        /* RLG status trial [n pos*] */
        if(ProcessRLG(status, message_size, &increase_trial, p, client, message)) quit_ok = RESET;
    } else if (!strcmp(command, RWG)) {
        /* RWG status trials */
        if(ProcessRWG(status, message_size, &increase_trial, p, client)) quit_ok = RESET;
    } else if (!strcmp(command, RQT)) {
        /* RQT status */
        if(ProcessRQT(status, message_size)) quit_ok = RESET;
    //} else if (!strcmp(command, RRV)) {
        /* RRV word/status */
        /* RRV palavra */
        //ProcessRRV(status, message_size);
    } else if (!strcmp(command, RSB)) {
        /* RSB status [Fname Fsize Fdata] */
        // ProcessRSB(status, message_size, p, client);
    } else printf("Unexpected error. Check command.\n");

    if (increase_trial) client->trial++;

    return quit_ok;
}


void ProcessRSG(char *status, int message_size, int *increase_trial, char *p, struct ClientInfo *client) {
    int n_letters, max_errors;
    char s_n_letters[3], s_max_errors[2];

    if (!strcmp(status, NOK) && message_size == 7) {
        printf("You already have an ongoing game with PLID = %s\n", client->plid);
    }
    else if (!strcmp(status, OK)) {
        sscanf(p, "%d %d", &n_letters, &max_errors);
        sprintf(s_n_letters, "%d", n_letters);
        sprintf(s_max_errors, "%d", max_errors);

        if (IsValidWordSize(n_letters) && IsValidMaxErrors(n_letters, max_errors) && message_size == 8 + strlen(s_n_letters) + strlen(s_max_errors)) {
            GetPlaceHolder(client->placeholder, n_letters);
            client->n_letters = n_letters;
            client->max_errors = max_errors;
            client->errors_left = max_errors + 1;
            client->on_going = 1;
            *increase_trial = 1;
            char filename[40];
            sprintf(filename, "STATE_%s.txt", client->plid);
            RemoveFileIfExists(filename);
            printf("New game started (max %d errors): %s\n", client->max_errors, client->placeholder);
        } 
        else printf("Error when trying to start a new game. Check command.\n"); 
    }
    printf("\n%s\n\n", BREAK_LINE);
}


int ProcessRLG(char *status, int message_size, int *increase_trial, char *p, struct ClientInfo *client, char *message) {
    int no_hits, server_trial;
    int reset = 0;

    if ((!strcmp(status, ERR)) && message_size == 7) printf("ERR\n");
    else if (sscanf(p, "%d", &server_trial) != 1) printf("ERR. Couldn't read trial number.\n");
    else if (!strcmp(status, DUP)) {
        printf("Letter = %c already chosen. Please, choose a different one. %s\n", client->letter, client->placeholder);
    }
    else if (server_trial != client->trial) printf("ERR. Server and client trial numbers do not match.\n");
    else if (!strcmp(status, WIN)) {
        CompleteWord(client->placeholder, client->letter, client->n_letters);
        printf("WELL DONE! You guessed: %s\n", client->placeholder);
        reset = 1;
    }
    else if (!strcmp(status, OK)) {
        no_hits = ProcessRLGOK(message, client->positions);
        UpdatePlaceHolder(client->placeholder, client->positions, no_hits, client->letter);
        printf("Yes, '%c' is part of the word: %s\n", client->letter, client->placeholder);
        *increase_trial = 1;
    }
    else if (!strcmp(status, NOK)) {
        printf("Incorrect letter (%c). Word: %s\nYou still have %d attempts left.\n", client->letter, client->placeholder, --client->errors_left);
        *increase_trial = 1;
    }
    else if ((!strcmp(status, OVR))) {
        printf("Incorrect letter (%c). Game over.\n", client->letter);
        reset = 1;
    }
    else if ((!strcmp(status, INV))) printf("Invalid command.\n");
    else if ((!strcmp(status, ERR))) printf("ERR\n");
    else printf("Error when trying to guess a letter. Check command.\n");

    printf("\n%s\n\n", BREAK_LINE);
    return reset;
}


int ProcessRWG(char *status, int message_size, int *increase_trial, char *p, struct ClientInfo *client) {
    int server_trial;
    int reset = 0;
        
    if ((!strcmp(status, ERR)) && message_size == 7) printf("ERR\n");
    else if (sscanf(p, "%d", &server_trial) != 1) printf("ERR. Couldn't read trial number.\n");
    else if (server_trial != client->trial) printf("ERR. Server and client trial numbers do not match.\n");
    else if (!strcmp(status, WIN)) {
        printf("WELL DONE! You guessed: %s\n", client->word_guess);
        reset = 1;
    }
    else if (!strcmp(status, NOK)) {
        printf("The word = %s is not correct. Word: %s\nYou still have %d attempts left\n", client->word_guess, client->placeholder, --client->errors_left);
        *increase_trial = 1;
    }
    else if ((!strcmp(status, OVR))) {
        printf("The word = %s is not correct. Game over.\n", client->word_guess);
        reset = 1;
    }
    else if ((!strcmp(status, INV))) printf("Invalid command.\n");
    else printf("Error when trying to guess the word. Check command.\n");

    printf("\n%s\n\n", BREAK_LINE);
    return reset;
}


int ProcessRQT(char *status, int message_size) {
    if (!strcmp(status, OK) && message_size == 6) {
        printf("%s", QUIT_MSG);
        printf("\n%s\n\n", BREAK_LINE);
        return 1;
    }
    else if (!strcmp(status, ERR) && message_size == 7) printf("ERR\n");
    else printf("Error when trying to exit / quit. Check command.\n");
    printf("\n%s\n\n", BREAK_LINE);
    return 0;
}


// void ProcessRRV(char *status, int message_size) {
//     if (!strcmp(status, OK) && message_size == 6) printf("OK\n");
//     else if (!strcmp(status, ERR) && message_size == 7) printf("ERR\n");
//     else if (strcmp(status, OK) && strcmp(status, ERR) && IsValidWordSize(strlen(status))) printf("Word: %s\n", status);
//     else printf("Error when trying to check the status / reveal the word. Check command.\n");
//     printf("\n%s\n\n", BREAK_LINE);
// }


void TcpConnection(char message[]) {

    int fd, errcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    size_t bytes_read = 0;
    char buffer[BLOCK_SIZE + 1];
    struct FileInfo file;
    int left_to_read;
    size_t bytes_read_aux = 0;
    char command[COMMAND_SIZE];
    char plid[PLID_SIZE];

    fd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
    if (fd==-1) exit(1);

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    /* localhost, 58011 */
    errcode = getaddrinfo(GSIP_D, GSPORT_D, &hints, &res);
    if(errcode != 0) exit(1); /* error */

    n = connect(fd, res->ai_addr, res->ai_addrlen);
    if (n==-1) exit(1); /* error */

    sscanf(message, "%s %s", command, plid);

    n = write(fd, message, strlen(message));
    if (n==-1) exit(1); /* error */
    

    n = read(fd, buffer, BLOCK_SIZE - bytes_read);
    bytes_read += n;
    while (bytes_read < BLOCK_SIZE && n > 0) {
        n = read(fd, buffer + bytes_read, BLOCK_SIZE - bytes_read);
        bytes_read += n;
    }

    if (n <= BLOCK_SIZE) {
        buffer[bytes_read] = '\0';
        if(!ProcessTCP(buffer, bytes_read, &file, plid)) return;
    }

    left_to_read = atoi(file.fsize) - bytes_read;
  
    while (left_to_read > 0) {
        bytes_read_aux = 0;
        n = read(fd, buffer, BLOCK_SIZE);
        if (n==-1) break;
        bytes_read += n;
        bytes_read_aux += n;

        while (bytes_read_aux < BLOCK_SIZE && n > 0) {
            n = read(fd, buffer + bytes_read_aux, BLOCK_SIZE - bytes_read_aux);
            if (n==-1) break;
            bytes_read += n;
            bytes_read_aux += n;
        }
        if (left_to_read - bytes_read_aux <= 0) bytes_read_aux -= 1;
        ReadFileData(buffer, bytes_read_aux, &file);
        left_to_read -= bytes_read_aux;
    }

    if (!strcmp(file.command, RST) || !strcmp(file.command, RHL)) {
        printf("\n");
        printf("Filename: %s\n", file.fname);
        printf("Filesize: %d\n", atoi(file.fsize));
    }
    printf("\n%s\n\n", BREAK_LINE);
    
    freeaddrinfo(res);
    close(fd);

}


/* Reads word with maximum size of max_size from message to buffer. Returns number of bytes read; -1 if error */
/* exact_size = 1 if we know exactly how many bytes we want to read; 0 otherwise */
/* is_data = 1 if we are reading file data; 0 otherwise */
int ReadWord(char *message, char *buffer, int max_size, int exact_size, int is_data) {

    int i = 0;

    while (message[i] != '\0' && message[i] != ' ' && message[i] != '\n' && i < max_size) {
        buffer[i] = message[i];
        i++;
    }

    if (i == 0 && message[i] == '\0') {
        return 0;
    }
    if (i == max_size && message[i] != ' ' && message[i] != '\n' && !is_data) {
        /* case: word in message surpasses max_size allowed, next char is a space */
        buffer[i] = '\0';
        printf("Word surpasses max_size allowed.\n");
        return -1;
    }
    else if (message[i] == '\n' && !is_data && i <= max_size && !exact_size) {
        buffer[i] = '\0';
        return i+1;
    }
    else if (i == max_size && message[i] != ' ' && is_data) {
        /* case: reading data and could not read till the end so the next char is not a space */
        buffer[i] = '\0';
        return i;
    }
    else if (buffer[i-1] != ' ' && (message[i] == ' ' || message[i] == '\0')  && ((i == max_size && exact_size) || (i <= max_size && !exact_size))) {
        /* word read successfully */
        buffer[i] = '\0';
        return i+1;
    }
    return -1;
}


/* Read file data */
/* nao vamos escrever os dados para buffer, simplesmente damos print e, no final, escrevemos para o file */
/* ou vamos tbm escrevendo para o file e, caso haja um erro, eliminamos o file */
void ReadFileData(char *msg, size_t bytes_to_read, struct FileInfo *file) {

    FILE *fp;
    fp = fopen(file->fname, "a");

    if (fp==NULL) {
        printf("Error trying open the file.\n");
        return;
    }
    /* write data to file */
    fwrite(msg, sizeof(char), bytes_to_read, fp);
    if (strcmp(file->command, RHL)) write(1, msg, bytes_to_read);

    fclose(fp);
    return;
}


int ProcessTCP(char *msg, int message_size, struct FileInfo *file, char plid[]) {

    char command[COMMAND], status[STATUS];
    size_t bytes_read = 0;
    size_t bytes_read_aux = 0;
    int is_valid_msg = 0;

    /* Command */
    bytes_read_aux = ReadWord(msg, command, COMMAND - 1, 1, 0);
    if (bytes_read_aux == -1) {
        printf("Error trying to read command.\n");
        return 0;
    }
    bytes_read += bytes_read_aux;
    /* Validate command */
    if (strcmp(command, RSB) && strcmp(command, RHL) && strcmp(command, RST)) {
        printf("Invalid command (three initial letters).\n");
        return 0;
    }

    /* Status */
    bytes_read_aux = ReadWord(msg + bytes_read, status, STATUS - 1, 0, 0);
    if (bytes_read_aux == -1) {
        printf("Error trying to read status.\n");
        return 0;
    }
    bytes_read += bytes_read_aux;
    /* Validate status */
    if (!(strcmp(command, OK) && strcmp(command, NOK) && strcmp(command, EMPTY) && strcmp(command, ACT) && strcmp(command, FIN))) {
        printf("Invalid status.\n");
        return 0;
    }

    /* Filename */
    bytes_read_aux = ReadWord(msg + bytes_read, file->fname, FILENAME - 1, 0, 0);
    if (bytes_read_aux == -1) {
        printf("Error trying to read filename.\n");
        return 0;
    }
    else if (bytes_read_aux == 0) {
        /* End of command. No filename to be read. */
        if (!strcmp(command, RSB) && !strcmp(status, EMPTY) && message_size == 10) {
            /* RSB EMPTY */
            printf("%s %s\n", RSB, EMPTY);
            printf("No game was yet won by any player. Empty file.\n");
        }
        else if (!strcmp(command, RHL) && !strcmp(status, NOK) && message_size == 8) {
            /* RHL NOK */
            printf("%s %s\n", RHL, NOK);
            printf("No file to be sent or some other error occurred. You might not have an active game.\n");
        }
        else if (!strcmp(command, RST) && !strcmp(status, NOK) && message_size == 8) {
            /* RST NOK */
            printf("%s %s\n", RST, NOK);
            printf("GS server did not find any active or finished games for PLID = xxx \n");
        }
        return 0;
    }


    bytes_read += bytes_read_aux;
    /* Validate filename */
    for (int j = 0; file->fname[j] != '\0'; j++) {
        if (!isalnum(file->fname[j]) && file->fname[j] != '-' && file->fname[j] != '_' && file->fname[j] != '.') {
            printf("Invalid filename. Filename can only contain alphanumeric characters and '-', '_', '.'\n");
            return 0;
        }
    }

    /* Filesize */
    bytes_read_aux = ReadWord(msg + bytes_read, file->fsize, FILESIZE - 1, 0, 0);
    if (bytes_read_aux == -1) {
        printf("Error trying to read filesize.\n");
        return 0;
    }
    
    bytes_read += bytes_read_aux;
    /* Validate filesize */
    if (!IsNumber(file->fsize)) {
        printf("Invalid filesize. File size must be a number.\n");
        return 0;
    }

    /* Check possible valid combinations */
    if (!strcmp(command, RST) && (!strcmp(status, ACT) || !strcmp(status, FIN))) {
        /* Read file data */
        char filename[FILENAME_MAX];
        sprintf(filename, "STATE_%s.txt", plid);
        RemoveFileIfExists(filename);
        is_valid_msg = 1;
    }
    else if (!strcmp(command, RHL) && !strcmp(status, OK)) {
        /* Read file data */
        is_valid_msg = 1;
    }
    else if (!strcmp(command, RSB) && !strcmp(status, OK)) {
        /* Read file data */
        is_valid_msg = 1;
    }
    else {
        printf("Invalid message.\n");
        return 0;
    }

    if (is_valid_msg) {
        strcpy(file->command, command);
        ReadFileData(msg + bytes_read, message_size - bytes_read, file);
        return 1;
    } else return 0;

}



/* Deletes a file if it exists */
void RemoveFileIfExists(char filename[]) {
    if (!access(filename, F_OK)) {
        if (remove(filename)) printf("Error trying to delete existing file.\n");
    } 
}


/* Returns 1 if word is a valid word, with no numbers or other chars; 0 otherwise */
int IsWord(char *word) {
    for (int i = 0; word[i] != '\0'; i++) {
        if (!isalpha(*(word + i))) return 0;
    }
    return 1;
}


/* Returns 1 if word is only composed of numbers; 1 otherwise */
int IsNumber(char *word) {
    for (int i = 0; word[i] != '\0'; i++) {
        if (word[i] > '9' || word[i] < '0') return 0;
    }
    return 1;
}


/* Returns 1 if filename is a valid one; 0 otherwise */
int IsFileName(char filename[], int size) {
    if (size > FILESIZE - 1) {
        printf("Invalid filename. Filename is limited to a total of 24 characters.\n");
        return 0;
    }
    for (int i = 0; i < size; i++) {
        if (!isalpha(filename[i]) || filename[i] != '-' || filename[i] != '_' || filename[i] != '.') {
            printf("Invalid filename. Filename can only contain alphanumeric characters and '-', '_', '.'\n");
            return 0;
        }
    }
    return 1;
}


/* Returns 1 if filesize is a valid one; 0 otherwise */
int IsFileSize(char filesize[]) {
    if (!IsNumber(filesize)) {
        printf("Invalid filesize. File size must be a number.\n");
        return 0;
    }
    if (strlen(filesize) > FILESIZE - 1) {
        printf("Invalid filesize. Size of the file is limited to 1GiB.\n");
        return 0;
    }
    return 1;
}


/* _ _ _ _ -> _ l l _ (letter 'l' in word "hello") */
/* Updates the placeholder with the letter in the correct positions of the word */
void UpdatePlaceHolder(char *placeholder, int *positions, int size, char letter) {
    for (int i = 0; i < size; i++) {
        placeholder[(positions[i] - 1) * 2] = letter;
        /**(placeholder + ((*(positions + i)) * 2)) = letter;*/
    }
}


/* Client won. Completes the placeholder with the letter left */
void CompleteWord(char *placeholder, char letter, int n_letters) {
    for (int i = 0; i < n_letters * 2; i=i+2) {
        if (placeholder[i] == '_') placeholder[i] = letter;
    }
}


/* Returns 1 is size is a valid size for the word; 0 otherwise */
int IsValidWordSize(int size) {
    if (size >= MIN_WORD_SIZE - 1 && size <= MAX_WORD_SIZE - 1) return 1;
    else return 0;
}


/* Returns 1 is errors if a valid number for max_errors, according to size of the word; 0 otherwise */
int IsValidMaxErrors(int size, int errors) {
    if (size <= 6) return MAX_ERRORS_LOW;
    else if (size >= 7 && size <= 10) return MAX_ERRORS_MEDIUM;
    else return MAX_ERRORS_HIGH;
}


/* "_ _ _ _" -> for word of size 4 */
/* Creates and returns the initial placeholder for a word of length size */
char* GetPlaceHolder(char *placeholder, int size) {
    for (int i = 0; i < size * 2 - 1; i++) {
        if (i % 2 == 0) { placeholder[i] = '_'; }
        else { placeholder[i] = ' '; }
    }
    placeholder[2 * size - 1] = '\0';
    return placeholder;
}


/* Validates IP address and port number given in the initial command */
void ProcessGameInvocation(int argc, char *argv[], char *GSIP, char *GSPORT) {
    /* ./player [-n GSIP] [-p GSport] */

    /* Command line validation */
    if (argc == 1 ) {
        strcpy(GSIP, GSIP_D);
        strcpy(GSPORT, GSPORT_D);
        printf("IP: %s\nPort: %s\n", GSIP, GSPORT);
    }
    else if (argc == 3) {
        if (!strcmp(argv[1], "-n")) {
            strcpy(GSIP, argv[2]);
            strcpy(GSPORT, GSPORT_D);
            printf("IP: %s\nPort: %s\n", argv[2], GSPORT_D);
        }
        else if (!strcmp(argv[1], "-p") && atoi(argv[2]) > PORT_MIN && atoi(argv[2]) < PORT_MAX) {
            strcpy(GSIP, GSIP_D);
            strcpy(GSPORT, argv[2]);
            printf("IP: %s\nPort: %s\n", GSIP_D, argv[2]);
        }
        else {
            printf("Invalid port number = %s\n", argv[2]); 
            exit(0);
        }
    }
    else if (argc == 5) {
        if (atoi(argv[4]) > PORT_MIN && atoi(argv[4]) < PORT_MAX) {
            strcpy(GSIP, argv[2]);
            strcpy(GSPORT, argv[4]);
            printf("IP: %s\nPort: %s\n", argv[2], argv[4]);
        } else {
            printf("Invalid port number = %s\n", argv[4]); 
            exit(0);
        }
    }
    else {
        printf("Invalid arguments.\n"); 
        exit(0);
    }
}