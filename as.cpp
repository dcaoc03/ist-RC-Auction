#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>

#include <string>
#include <cctype>
#include <list>

#include "as.hpp"
#include "./common/constants.h"
#include "./common/utils.hpp"
#include "./common/database_utils.hpp"
#include "./common/as_stdout_messages.h"

using namespace std;

string as_port;
bool verbose=false;

int fd_udp_global=-1, fd_tcp_global=-1;
int end_as=0;

int n_child_processes=0;



/*  +----------------------------------+
    |                                  |
    |          MAIN FUNCTIONS          |
    |                                  |
    +----------------------------------+  */

/* end_AS: signal handler for SIGINT, SIGTERM and SIGTSTP, ends the AS's execution
*/
static void end_AS(int sig) {
    end_as = 1;

    // Disables the child_finished signal handler
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGCHLD, &act, NULL) == -1) {
        printf(SIGNAL_HANDLING_ERROR);
        exit(1);
    }

    // Waits for all child processes to finish
    while (n_child_processes > 0) {
        printf("Waiting for %d processes to finish\n", n_child_processes);
        wait(NULL);
        n_child_processes--;
    }

    // Closes the UDP and TCP sockets
    close(fd_tcp_global);
    close(fd_udp_global);

    // Deletes all created semaphores
    unlink_semaphores();

    printf(AS_CLOSING_MESSAGE);
    exit(0);
}

/* child_finished: signal handler used for SIGCHLD, decreases the global variable n_child_processes by one
   when a child process finishes
*/
static void child_finished(int sig) {
    n_child_processes--;
}

/* process_arguments: processes the command line arguments
   - argc: the number of command line arguments
   - argv: array of strings holding the arguments
   If they are specified, the one marked with '-p' will be used as the default port of the server and, if the
   flag '-v' is used, the server will run in verbose mode
*/
void process_arguments(int argc, char** argv) {         // processes the arguments given by launching the User
    for (int i=1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) 
            as_port = argv[++i];
        else if (!strcmp(argv[i], "-v"))
            verbose = true;
    }
}

/* MAIN FUNCTION */
int main(int argc, char** argv) {
    as_port = PORT;

    if (argc > 1)
        process_arguments(argc, argv);

    /* SET UP SOCKET ENVIRONMENT */

    int fd_tcp, fd_udp, newfd, select_value;
    ssize_t n;
    socklen_t addrlen_tcp, addrlen_udp;
    struct addrinfo hints_tcp, hints_udp, *res_tcp, *res_udp;
    struct sockaddr_in addr_tcp, addr_udp;
    char buffer[UDP_BUFFER_SIZE], command_word[COMMAND_WORD_SIZE+1];
    fd_set fdset;

    pid_t child_pid;

    /* SIGNAL HANDLING */
    struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = SIG_IGN;
    if (signal(SIGCHLD, child_finished) == SIG_ERR) {
        printf(SIGNAL_HANDLING_ERROR);
        exit(1);
    }
    if (sigaction(SIGPIPE, &act, NULL) == -1) {
        printf(SIGNAL_HANDLING_ERROR);
        exit(1);
    }
    if (signal(SIGINT, end_AS) == SIG_ERR){
        printf(SIGNAL_HANDLING_ERROR);
        exit(1);
    }
    if (signal(SIGTSTP, end_AS) == SIG_ERR){
        printf(SIGNAL_HANDLING_ERROR);
        exit(1);
    }
    if (signal(SIGTERM, end_AS) == SIG_ERR){
        printf(SIGNAL_HANDLING_ERROR);
        exit(1);
    }

    /* CREATE TCP SOCKET */
   
    memset(&hints_tcp, 0, sizeof hints_tcp );
    hints_tcp.ai_family=AF_INET;
    hints_tcp.ai_socktype=SOCK_STREAM;
    hints_tcp.ai_flags=AI_PASSIVE;

    if (getaddrinfo(NULL, as_port.c_str(), &hints_tcp, &res_tcp) != 0)     {printf(SOCKET_CREATION_ERROR, "TCP"); exit(1);}

    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_tcp == -1)     {printf(SOCKET_CREATION_ERROR, "TCP"); exit(1);}

    n = bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen);
    if (n==-1)     {printf(SOCKET_CREATION_ERROR, "TCP"); exit(1);}
    
    if (listen(fd_tcp, 5) == -1)     {printf(SOCKET_CREATION_ERROR, "TCP"); exit(1);}
    fd_tcp_global = fd_tcp;
    freeaddrinfo(res_tcp);
    
    /* CREATE UDP SOCKET */

    memset(&hints_udp, 0, sizeof hints_udp );
    hints_udp.ai_family=AF_INET;
    hints_udp.ai_socktype=SOCK_DGRAM;
    hints_udp.ai_flags=AI_PASSIVE;

    if (getaddrinfo(NULL, as_port.c_str(), &hints_udp, &res_udp) != 0)     {printf(SOCKET_CREATION_ERROR, "UDP"); exit(1);}

    fd_udp=socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1)     {printf(SOCKET_CREATION_ERROR, "UDP"); exit(1);}

    n=bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1)     {printf(SOCKET_CREATION_ERROR, "UDP"); exit(1);}
    fd_udp_global = fd_udp;
    freeaddrinfo(res_udp);

    /* SET UP THE FDSET */
    FD_ZERO(&fdset);

    /* SET UP AUCTION DATABASE*/
    if (setup_auctions_dir() == -1) {
        if (verbose)    printf(AUCTIONS_NOT_FOUND_ERROR);
        exit(1);
    }

    /* MAIN CYCLE: FILE DESCRIPTOR READING */
    while(!end_as) {
        FD_SET(fd_tcp, &fdset);
        FD_SET(fd_udp, &fdset);

        select_value= select(max(fd_tcp, fd_udp)+1, &fdset, NULL, NULL, NULL);

        // One of the sockets is ready for reading
        if (select_value != -1) {
            if (FD_ISSET(fd_tcp, &fdset)) {
                // TCP socket is ready to be read
                addrlen_tcp = sizeof(addr_tcp);
                if ((newfd=accept(fd_tcp, (struct sockaddr*)&addr_tcp, &addrlen_tcp)) == -1) {
                    printf(SOCKET_CREATION_ERROR, "TCP");
                    exit(1);
                }

                // Setting socket timer
                struct timeval timeout;
                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                if (setsockopt(newfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
                    printf(SOCKET_CREATION_ERROR, "TCP");
                    exit(1);
                };

                if (verbose) {
                    int errcode;
                    char sender[NI_MAXHOST], port[NI_MAXSERV];

                    if((errcode=getnameinfo((struct sockaddr *)&addr_tcp, addrlen_tcp, sender, sizeof sender, port, sizeof port, NI_NUMERICHOST))!=0)
                        fprintf(stderr, "error: getnameinfo: %s\n", gai_strerror(errcode));
                    else
                        printf(REQUEST_RECEIVED, "TCP", sender, port);

                }

                child_pid = fork();
                if (child_pid == -1) {
                    // fork() error
                    fprintf(stderr, CHILD_PROCESS_ERROR, strerror(errno));
                    const char* response2 = "ERR\n";
                    if (write(newfd, (const char*)response2, strlen(response2)) < 0) {
                        printf(SOCKET_WRITING_ERROR, "TCP");
                        exit(1);
                    }
                }
                else if (child_pid == 0) { 

                    /* ------ CHILD PROCESS PROGRAM ------ */

                    close(fd_tcp); 
                    close(fd_udp);

                    /* SIGNAL HANDLING */
                    struct sigaction act;
                    memset(&act, 0, sizeof act);
                    act.sa_handler = SIG_IGN;
                    if (sigaction(SIGINT, &act, NULL) == -1) {
                        printf(SIGNAL_HANDLING_ERROR);
                        exit(1);
                    }
                    if (sigaction(SIGTSTP, &act, NULL) == -1) {
                        printf(SIGNAL_HANDLING_ERROR);
                        exit(1);
                    }
                    if (sigaction(SIGTERM, &act, NULL) == -1) {
                        printf(SIGNAL_HANDLING_ERROR);
                        exit(1);
                    }
                    
                    char command_word[COMMAND_WORD_SIZE+1];
                    int asset_fd = -1;      // Used for show_asset(); is -1 unless show_asset() is executed successfully

                    if (byte_reading(NULL, newfd, command_word, COMMAND_WORD_SIZE, false, false) < 0)    exit(1);

                    /* REQUEST PROCESSING */
                    string response;

                    if (!strcmp(command_word, "OPA")) {
                        if (verbose)    printf(ISSUED_REQUEST, "open");
                        string status = open_auction(newfd);
                        response = "ROA " + status + "\n";
                    }

                    else if (!strcmp(command_word, "CLS")) {
                        if (verbose)    printf(ISSUED_REQUEST, "close");
                        string status = close_auction(newfd);
                        response = "RCL " + status + "\n";
                    }

                    else if (!strcmp(command_word, "BID")) {
                        if (verbose)    printf(ISSUED_REQUEST, "bid");
                        response = "RBD " + bid(newfd) + "\n";
                    }

                    else if (!strcmp(command_word, "SAS")) {
                        if (verbose)    printf(ISSUED_REQUEST, "show_asset");
                        response = "RSA " + show_asset(newfd, &asset_fd);
                        if (asset_fd < 0)
                            response += "\n";
                    }

                    else {
                        if (verbose)    printf(UNKNOWN_REQUEST, command_word);
                        response = "ERR\n";
                    }
                
                    /* TCP SOCKET WRITING */
                    const char* response2 = response.c_str();
                    if (write(newfd, (const char*)response2, strlen(response2)) < 0) {
                        printf(SOCKET_WRITING_ERROR, "TCP");
                        exit(1);
                    }
                    // If asset_fd != -1, show_asset() was successful,  an asset needs to be sent
                    if (asset_fd >= 0) {
                        long image_size, bytes_read=0;
                        char image_buffer[IMAGE_BUFFER_SIZE];
                        sscanf(response2, "%*s %*s %*s %ld", &image_size);
                        while (bytes_read < image_size) {
                            memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
                            n = read(asset_fd, image_buffer, IMAGE_BUFFER_SIZE);
                            if (n<0)    {printf(IMAGE_FILE_DESCRIPTOR_ERROR);  exit(1);}
                            n = write(newfd, image_buffer, n);
                            if (n<0)    {printf(SOCKET_WRITING_ERROR, "TCP");  exit(1);}
                            bytes_read += n;
                        }
                        char new_line_char = '\n';
                        n = write(newfd, &new_line_char, 1);
                        close(asset_fd);
                    }
                    close(newfd); 
                    exit(0); 
                }

                // The parent process increases the number of active child processes
                n_child_processes++;
                close(newfd);
            }

            if (FD_ISSET(fd_udp, &fdset)) { 
                // UDP socket is ready to be read
                addrlen_udp = sizeof(addr_udp);
                bzero(buffer, sizeof(buffer)); 
                n=recvfrom(fd_udp, buffer, UDP_BUFFER_SIZE, 0, (struct sockaddr*)&addr_udp, &addrlen_udp);
                if (n==-1) {
                    if (verbose)    printf(SOCKET_READING_ERROR, "UDP");
                    exit(1);
                }
                
                if (verbose) {
                    int errcode;
                    char sender[NI_MAXHOST],port[NI_MAXSERV];

                    if((errcode=getnameinfo((struct sockaddr *)&addr_udp, addrlen_udp, sender, sizeof sender, port, sizeof port, NI_NUMERICHOST | NI_DGRAM))!=0)
                        fprintf(stderr, "error: getnameinfo: %s\n", gai_strerror(errcode));
                    else
                        printf(REQUEST_RECEIVED, "UDP", sender, port);

                }

                /* REQUEST PROCESSING */
                string response;
                if (byte_reading(buffer, -1, command_word, COMMAND_WORD_SIZE, false, false) < 0)    exit(1);

                if (!strcmp(command_word, "LIN")) {
                    if (verbose)    printf(ISSUED_REQUEST, "login");
                    string status = login(buffer);
                    response = "RLI " + status + "\n";
                }

                else if (!strcmp(command_word, "LOU")) {
                    if (verbose)    printf(ISSUED_REQUEST, "logout");
                    response = "RLO " + logout(buffer) + "\n";
                }
                
                else if (!strcmp(command_word, "UNR")) {
                    if (verbose)    printf(ISSUED_REQUEST, "unregister");
                    response = "RUR " + unregister(buffer) + "\n";
                }

                else if (!strcmp(command_word, "LMA")) {
                    if (verbose)    printf(ISSUED_REQUEST, "myauctions");
                    response = "RMA " + myauctions_or_mybids(buffer, 'a') + "\n";
                }

                else if (!strcmp(command_word, "LMB")) {
                    if (verbose)    printf(ISSUED_REQUEST, "mybids");
                    response = "RMB " + myauctions_or_mybids(buffer, 'b') + "\n";
                }

                else if (!strcmp(command_word, "LST")) {
                    if (verbose)    printf(ISSUED_REQUEST, "list");
                    response = "RLS " + list_auctions(buffer) + "\n";
                }
                
                else if (!strcmp(command_word, "SRC")) {
                    if (verbose)    printf(ISSUED_REQUEST, "show_record");
                    response = "RRC " + show_record(buffer) + "\n";
                }

                else {
                    if (verbose)    printf(UNKNOWN_REQUEST, command_word);
                    response = "ERR\n";
                }
                
                // UDP SOCKET WRITING
                const char* response2 = response.c_str();

                n=sendto(fd_udp, response2, strlen(response2), 0, (struct sockaddr*) &addr_udp, addrlen_udp);
                if (n==-1) {
                    if (verbose)    printf(SOCKET_WRITING_ERROR, "UDP");
                    exit(1);
                }
            } 
        }
        else {
            // Ignore SIGCHLD
        }
    }

    close(fd_tcp);
    close(fd_udp);

    printf(AS_CLOSING_MESSAGE);

    return 0;
}



/*  +--------------------------------------+
    |                                      |
    |          REQUEST PROCESSING          |
    |                                      |
    +--------------------------------------+  */

/*  +------------ UDP Commands ------------+ */

/* login:
   Reads: LIN UID password
   Writes: RLI status
*/
string login(char arguments[]) {
    // ARGUMENT READING
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1];
    int bytes_read = COMMAND_WORD_SIZE+1, n;

    if ((n = byte_reading(arguments+bytes_read, -1, UID, UID_SIZE, false, false)) == -1)   return "ERR";
    bytes_read += n;
    if ((n = byte_reading(arguments+bytes_read, -1, password, PASSWORD_SIZE, false, true)) == -1)   return "ERR";
    bytes_read += n;

    // ARGUMENT PROCESSING 
    if ((strlen(UID) != 6) || (strlen(password) != 8)) {
        if (verbose)        printf(UNSUCCESSFUL_LOGIN, UID, ARGUMENTS_WRONG_SIZE_ERROR);
        return "ERR";
    }
    for (int i=0; i < 8; i++) {
        if ((i < 6) && !isdigit(UID[i])) {
            if (verbose)    printf(UNSUCCESSFUL_LOGIN, UID, UID_DIGITS_ERROR);
            return "ERR";
        }
        if (!isdigit(password[i]) && (isalpha(password[i]) == 0)) {
            if (verbose)    printf(UNSUCCESSFUL_LOGIN, UID, PASSWORD_DIGITS_ERROR);
            return "ERR";
        }
    }

    /* COMMAND EXECUTION */
    DIR* dir = does_user_exist(UID);
    // User exists (dir != NULL) -> login
    if (dir) {
        closedir(dir);

        int password_file_exists = is_user_registered(UID);
        if (password_file_exists == 0) {
            int password_check = user_password_check(UID, password);
            if (password_check == 1) {
                if (verbose)    printf(SUCCESSFUL_LOGIN, UID);
                return "OK";
            }
            else if (password_check == 0) {
                if (verbose)    printf(UNSUCCESSFUL_LOGIN, UID, WRONG_PASSWORD_ERROR);
                return "NOK";
            }
            else if (password_check == -1) {
                if (verbose)    printf(UNSUCCESSFUL_LOGIN, UID, GENERIC_PASSWORD_ERROR);
                return "ERR";
            }
        } else if (password_file_exists == -1) {
            if (create_user(UID, password, false) < 0) {
                if (verbose)    printf(UNSUCCESSFUL_REGISTRATION, UID);
                return "NOK";
            }

            if (verbose)        printf(SUCCESSFUL_REGISTRATION, UID);
            return "REG";
        }

        return "ERR";

    }
    // User does not exist -> new registration
    else {
        if (create_user(UID, password, true) < 0) {
            if (verbose)    printf(UNSUCCESSFUL_REGISTRATION, UID);
            return "NOK";
        }

        if (verbose)    printf(SUCCESSFUL_REGISTRATION, UID);
        return "REG";
    }
}

/* logout:
   Reads: LOU UID password
   Writes: RLO status
*/
string logout(char arguments[]) {
    // ARGUMENT READING
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1];
    int bytes_read = COMMAND_WORD_SIZE+1, n;

    if ((n = byte_reading(arguments+bytes_read, -1, UID, UID_SIZE, false, false)) == -1)   return "ERR";
    bytes_read += n;
    if ((n = byte_reading(arguments+bytes_read, -1, password, PASSWORD_SIZE, false, true)) == -1)   return "ERR";
    bytes_read += n;
    string str_UID(UID);

    /* COMMAND EXECUTION */
    DIR* dir = does_user_exist(UID);
    if (dir) {
        closedir(dir);
        int was_logout_successful = logout_user(UID);
        if (was_logout_successful == 1) {
            if (verbose)    printf(UNSUCCESSFUL_LOGOUT, UID, USER_NOT_LOGGED_IN_ERROR);
            return "NOK";
        }
        else if (was_logout_successful == 0) {
            if (verbose)    printf(SUCCESSFUL_LOGOUT, UID);
            return "OK";
        }
        if (was_logout_successful == -1) {
            if (verbose)    printf(UNSUCCESSFUL_LOGOUT, UID, GENERIC_LOGOUT_ERROR);
            return "ERR";
        }
    }
    else {
        if (verbose)    printf(UNSUCCESSFUL_LOGOUT, UID, USER_NOT_REGISTERED_ERROR);
        return "UNR";
    }
    return "NOK";
}

/* unregister:
   Reads: UNR UID password
   Writes: RUR status
*/
string unregister(char arguments[]) {
    // ARGUMENT READING
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1];
    int bytes_read = COMMAND_WORD_SIZE+1, n;

    if ((n = byte_reading(arguments+bytes_read, -1, UID, UID_SIZE, false, false)) == -1)   return "ERR";
    bytes_read += n;
    if ((n = byte_reading(arguments+bytes_read, -1, password, PASSWORD_SIZE, false, true)) == -1)   return "ERR";
    bytes_read += n;
    string str_UID(UID);

    /* COMMAND EXECUTION */
    DIR* dir = does_user_exist(UID);
    if (dir) {
        closedir(dir);
        if (is_user_logged_in(UID) == -1) {
            if (verbose)    printf(UNSUCCESSFUL_UNREGISTRATION, UID, USER_NOT_LOGGED_IN_ERROR);
            return "NOK";
        }
        else {
            if (unregister_user(UID) != 0) {
                if (verbose)    printf(UNSUCCESSFUL_UNREGISTRATION, UID, GENERIC_UNREGISTRATION_ERROR);
                return "NOK";
            }
            else  {
                if (verbose)    printf(SUCCESSFUL_UNREGISTRATION, UID);
                return "OK";
            }
        }
    }
    else  {
        if (verbose)    printf(UNSUCCESSFUL_UNREGISTRATION, UID, USER_NOT_REGISTERED_ERROR);
        return "UNR";
    }
}

/* myauctions / ma:                             mybids / mb:
   Reads: LMA UID                               Reads: LMB UID
   Writes: RMA status[ AID state]*              Writes: RMB status[ AID state]*
*/
string myauctions_or_mybids(char arguments[], char mode) {
    // MODE CHECK
    if ((mode != 'a') && (mode != 'b')) {
        printf(NON_EXISTING_MODE_ERROR);
        return "ERR";
    }

    // ARGUMENT READING
    char UID[UID_SIZE+1];
    int bytes_read = COMMAND_WORD_SIZE+1, n;

    if ((n = byte_reading(arguments+bytes_read, -1, UID, UID_SIZE, false, true)) == -1)   return "ERR";
    bytes_read += n;
    string str_UID(UID);

    /* COMMAND EXECUTION */
    DIR* dir = does_user_exist(UID);
    if (dir) {
        closedir(dir);
        if (is_user_logged_in(UID) == -1) {
            if (verbose && (mode == 'a'))    printf(UNSUCCESSFUL_MY_AUCTIONS, UID, USER_NOT_LOGGED_IN_ERROR);
            if (verbose && (mode == 'b'))    printf(UNSUCCESSFUL_MY_BIDS, UID, USER_NOT_LOGGED_IN_ERROR);
            return "NLG";
        }
        else {
            list <string> user_list = get_hosted_auctions_or_bids(UID, mode);
            if (user_list.empty()) {
                if (verbose && (mode == 'a'))    printf(USER_HAS_NO_ONGOING_AUCTIONS, UID);
                if (verbose && (mode == 'b'))    printf(USER_HAS_NO_ONGOING_BIDS, UID);
                return "NOK";
            }
            string response = "";
            for (list<string>::iterator it = user_list.begin(); it != user_list.end(); it++) {
                string AID = *it;
                int ongoing = is_auction_ongoing(AID);
                if (ongoing == -1) {
                    if (verbose && (mode == 'a'))    printf(UNSUCCESSFUL_MY_AUCTIONS, UID, GENERIC_MY_AUCTIONS_ERROR);
                    if (verbose && (mode == 'b'))    printf(UNSUCCESSFUL_MY_BIDS, UID, GENERIC_MY_BIDS_ERROR);
                    return "ERR";
                }
                response += " " + AID + (ongoing ? " 1" : " 0");
            }
            return "OK" + response;
        }
    }
    else {
        if (verbose && (mode == 'a'))    printf(UNSUCCESSFUL_MY_AUCTIONS, UID, USER_NOT_REGISTERED_ERROR);
        if (verbose && (mode == 'b'))    printf(UNSUCCESSFUL_MY_BIDS, UID, USER_NOT_REGISTERED_ERROR);
        return "ERR";
    }

}

/* list:
   Reads: LST
   Writes: RLS status[ AID state]*
*/
string list_auctions(char arguments[]) {
    /* COMMAND EXECUTION */
    list <string> auctions_list = get_hosted_auctions_or_bids("", 'l');
    if (auctions_list.empty()) {
        if (verbose)    printf(NO_AUCTIONS);
        return "NOK";
    }
    string response = "";
    for (list<string>::iterator it = auctions_list.begin(); it != auctions_list.end(); it++) {
        string AID = *it;
        int ongoing = is_auction_ongoing(AID);
        if (ongoing == -1) {
            if (verbose)    printf(UNSUCCESSFUL_LIST, GENERIC_LIST_ERROR);
            return "ERR";
        }
        response += " " + AID + (ongoing ? " 1" : " 0");
    }
    return "OK" + response;
}

/* show_record / sr:
   Reads: SRC UID 
   Writes: RRC status[ host_UID auction_name asset_fname start_value
   start_date-time timeactive][ B bidder_UID bid_value bid_date-time bid_sec_time]*
   [ E end_date-time end_sec_time]
*/
string show_record(char arguments[]) {
    // ARGUMENT READING
    char AID[MAX_DIGITS+1];
    int bytes_read = COMMAND_WORD_SIZE+1, n;

    if ((n = byte_reading(arguments+bytes_read, -1, AID, MAX_DIGITS, false, true)) == -1)   return "ERR";
    bytes_read += n;

    if (!does_auction_exist(AID)) {
        printf(UNSUCCESSFUL_SHOW_RECORD, AID, AUCTION_NOT_FOUND_ERROR);
        return "NOK";
    }

    /* COMMAND EXECUTION */
    string auction_info = get_auction_info(AID);
    if (auction_info == "") return "ERR";

    string bids = get_bids(AID);

    string end = get_auction_end_info(AID);

    return "OK " + auction_info + bids + end;
}


/*  +------------ TCP Commands ------------+ */

/* open:
   Reads: OPA UID password name start_value timeactive Fname Fsize Fdata
   Writes: ROA status[ AID]
*/
string open_auction(int fd) {
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1], asset_name[ASSET_NAME_SIZE+1], start_value_str[START_VALUE_SIZE+1], 
        timeactive_str[TIMEACTIVE_SIZE+1], file_name[FILE_NAME_SIZE+1], file_size_str[FILE_SIZE_SIZE+1];

    // Reading the parameters
    if (byte_reading(NULL, fd, UID, UID_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, password, PASSWORD_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, asset_name, ASSET_NAME_SIZE, true, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, start_value_str, START_VALUE_SIZE, true, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, timeactive_str, TIMEACTIVE_SIZE, true, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, file_name, FILE_NAME_SIZE, true, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, file_size_str, FILE_SIZE_SIZE, true, true) == -1)   return "ERR";
    
    // Converting the numeric parameters
    long start_value = atol(start_value_str);
    long timeactive = atol(timeactive_str);
    long file_size = atol(file_size_str);

    int n;

    /* ARGUMENT PROCESSING */
    if ((start_value < 0) || (timeactive < 0)) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, NUMERIC_VALUES_ERROR);
        return "NOK";
    }

    int n_AID = get_number_of_auctions();
    if (n_AID == MAX_AUCTIONS) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, MAX_NUM_AUCTIONS_ERROR);
        return "NOK";
    }

    if (is_user_logged_in(UID) < 0) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, USER_NOT_LOGGED_IN_ERROR);
        return "NLG";
    }

    /* COMMAND EXECUTION */
    string s_AID = to_string(n_AID);
    string AID = string(MAX_DIGITS - s_AID.length(), '0') + s_AID;

    // Create image
    if (copy_image(AID, file_name, file_size, fd) == -1) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, ASSET_CREATION_ERROR);
        return "ERR";
    }

    // Reading the '\n' character at the end
    char read_char;
    n = read(fd, &read_char, 1);
    if (n < 0) {
        if (verbose)    printf(SOCKET_READING_ERROR, "TCP");
        return "ERR";
    }
    if (read_char != '\n') {
        if (verbose)    printf(BADLY_FORMATTED_MESSAGE);
        return "ERR";
    }

    // Create Auction directory
    if (create_auction(AID, UID, asset_name, file_name, start_value, timeactive) == -1) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, AUCTION_DIRS_ERROR);
        return "ERR";
    }
    
    if (verbose)    printf(SUCCESSFUL_AUCTION_OPENING, asset_name, AID.c_str());
    return "OK " + AID;
}

/* close:
   Reads: CLS UID password AID
   Writes: RCL status
*/
string close_auction(int fd) {
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1], AID[MAX_DIGITS+1]; 

    // Reading the parameters
    if (byte_reading(NULL, fd, UID, UID_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, password, PASSWORD_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, AID, MAX_DIGITS, false, true) == -1)   return "ERR";

    /* ARGUMENT PROCESSING */
    if (is_user_logged_in(UID) < 0) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_CLOSING, AID, USER_NOT_LOGGED_IN_ERROR);
        return "NLG";
    }

    if (!does_auction_exist(AID)) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_CLOSING, AID, AUCTION_NOT_FOUND_ERROR);
        return "EAU";
    }

    if (!does_user_host_auction(AID, UID)) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_CLOSING, AID, AUCTION_OWNER_ERROR);
        return "EOW";
    }

    int auction_status = is_auction_ongoing(AID);
    if (auction_status == -1) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_CLOSING, AID, GENERIC_CLOSE_AUCTION_ERROR);
        return "ERR";
    }
    else if (!auction_status) {
        if (verbose)    printf(AUCTION_ALREADY_CLOSED, AID);
        return "END";
    }

    /* COMMAND EXECUTION */
    if (create_auction_end_file(AID) == -1) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_CLOSING, AID, GENERIC_CLOSE_AUCTION_ERROR);
        return "ERR";
    }

    if (verbose)    printf(SUCCESSFUL_AUCTION_CLOSING, AID);
    return "OK"; 
}

/* show_asset / sa:
   Reads: SAS AID
   Writes: RSA status[ Fname Fsize Fdata]
*/
string show_asset(int fd, int *image_fd) {
    char AID[MAX_DIGITS+1];

    // Reading the parameters
    if (byte_reading(NULL, fd, AID, MAX_DIGITS, false, true) == -1)   return "ERR";

    /* ARGUMENT PROCESSING */
    string file_name = get_auction_file_name(AID);
    if (file_name == "") {
        if (verbose)    printf(UNSUCCESSFUL_SHOW_ASSET, AID, ASSET_NOT_FOUND_ERROR);
        return "NOK";
    }

    /* COMMAND EXECUTION */
    string response = "OK " + file_name;
    string file_path = "./AUCTIONS/" + string(AID) + "/ASSET/" + file_name;
    // Writes the file's size to the message and gets the file's file descriptor
    *image_fd = image_processing((char*) file_path.c_str(), &response);
    if (*image_fd == -1) {
        if (verbose)    printf(UNSUCCESSFUL_SHOW_ASSET, AID, ASSET_NOT_FOUND_ERROR);
        return "NOK";
    }

    // Leaves the complexity of writing the file to the main loop
    return response;
}

/* bid:
   Reads: BID UID password AID value
   Writes: ROA status[ AID]
*/
string bid(int fd) {
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1], AID[MAX_DIGITS+1], value_char[START_VALUE_SIZE+1];

    // Reading the parameters
    if (byte_reading(NULL, fd, UID, UID_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, password, PASSWORD_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, AID, MAX_DIGITS, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, value_char, START_VALUE_SIZE, true, true) == -1)   return "ERR";

    long value = stol(value_char);

    /* ARGUMENT PROCESSING */
    if (!does_auction_exist(AID) || !is_auction_ongoing(AID)) {
        if (verbose)    printf(UNSUCCESSFUL_BID, value, AUCTION_NOT_ACTIVE_ERROR);
        return "NOK";
    }
    if ((does_user_exist(UID) == NULL) || (is_user_logged_in(UID) < 0)) {
        if (verbose)    printf(UNSUCCESSFUL_BID, value, USER_NOT_LOGGED_IN_ERROR);
        return "NLG";
    }
    if (does_user_host_auction(AID, UID)) {
        if (verbose)    printf(UNSUCCESSFUL_BID, value, BID_IN_OWN_AUCTION_ERROR);
        return "ILG";
    }
    int is_valid_bid = get_highest_bid(AID, value);
    if (is_valid_bid == -1) {
        if (verbose)    printf(UNSUCCESSFUL_BID, value, GENERIC_BID_ERROR);
        return "ERR";
    }
    if (is_valid_bid == 0) {
        if (verbose)    printf(UNSUCCESSFUL_BID, value, BID_IS_NOT_VALID_ERROR);
        return "REF";
    }
    
    /* COMMAND EXECUTION */
    string value_str = string(START_VALUE_SIZE - to_string(value).length(), '0') + to_string(value);
    if (create_bid_files(UID, AID, value, value_str) < 0) {
        if (verbose)    printf(UNSUCCESSFUL_BID, value, GENERIC_BID_ERROR);
        return "ERR";
    }

    if (verbose)    printf(SUCCESSFUL_BID, value);
    return "ACC";
}