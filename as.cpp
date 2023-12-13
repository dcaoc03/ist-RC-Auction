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



/*  +----------------------------------+
    |                                  |
    |          MAIN FUNCTIONS          |
    |                                  |
    +----------------------------------+  */

static void end_AS(int sig) {
    end_as = 1;
    close(fd_tcp_global);
    close(fd_udp_global);
    unlink_semaphores();

    printf(AS_CLOSING_MESSAGE);
    exit(0);
}

void process_arguments(int argc, char** argv) {         // processes the arguments given by launching the User
    for (int i=1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) 
            as_port = argv[++i];
        else if (!strcmp(argv[i], "-v"))
            verbose = true;
    }
}

int main(int argc, char** argv) {
    as_port = PORT;

    if (argc > 1)
        process_arguments(argc, argv);

    /* SET UP SOCKET ENVIRONMENT */

    int fd_tcp, fd_udp, newfd;
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
    if (sigaction(SIGCHLD, &act, NULL) == -1) {
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

    /* -----TODO-----: kill all child processes smoothly */

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

    /* SET UP THE FDSET */
    FD_ZERO(&fdset);

    /* SET UP AUCTION DATABASE*/
    if (setup_auctions_dir() == -1) {
        if (verbose)    printf(AUCTIONS_NOT_FOUND_ERROR);
        exit(1);
    }

    while(!end_as) {
        FD_SET(fd_tcp, &fdset);
        FD_SET(fd_udp, &fdset);

        select(max(fd_tcp, fd_udp)+1, &fdset, NULL, NULL, NULL);

        if (FD_ISSET(fd_tcp, &fdset)) {
            addrlen_tcp = sizeof(addr_tcp);
            if ((newfd=accept(fd_tcp, (struct sockaddr*)&addr_tcp, &addrlen_tcp)) == -1) {
                printf(SOCKET_CREATION_ERROR, "TCP");
                exit(1);
            }
            if (verbose) {
                int errcode;
                char sender[NI_MAXHOST], port[NI_MAXSERV];

                if((errcode=getnameinfo((struct sockaddr *)&addr_tcp, addrlen_tcp, sender, sizeof sender, port, sizeof port, NI_NUMERICHOST))!=0)
                    fprintf(stderr, "error: getnameinfo: %s\n", gai_strerror(errcode));
                else
                    printf(REQUEST_RECEIVED, "TCP", sender, port);

            }
            child_pid = fork();
            if (child_pid == -1) 
                fprintf(stderr, CHILD_PROCESS_ERROR, strerror(errno));
            else if (child_pid == 0) { 
                close(fd_tcp); 
                close(fd_udp);
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
            
                
                const char* response2 = response.c_str();
                if (write(newfd, (const char*)response2, strlen(response2)) < 0) {
                    printf(SOCKET_WRITING_ERROR, "TCP");
                    exit(1);
                }
                if (asset_fd >= 0) {                    // If asset_fd != -1, show_asset() was successful, so we send the asset
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
            close(newfd);
        }
        if (FD_ISSET(fd_udp, &fdset)) { 
            addrlen_udp = sizeof(addr_udp);
            bzero(buffer, sizeof(buffer)); 
            n=recvfrom(fd_udp, buffer, UDP_BUFFER_SIZE, 0, (struct sockaddr*)&addr_udp, &addrlen_udp);
            
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
            
            const char* response2 = response.c_str();

            n=sendto(fd_udp, response2, strlen(response2), 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n==-1) {
                if (verbose)    printf(SOCKET_WRITING_ERROR, "UDP");
                exit(1);
            }
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


string list_auctions(char arguments[]) {
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

    string auction_info = get_auction_info(AID);
    if (auction_info == "") return "ERR";

    string bids = get_bids(AID);

    string end = get_auction_end_info(AID);

    return "OK " + auction_info + bids + end;
}

/*  +------------ TCP Commands ------------+ */

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
    // Reading the image
    /*char image_buffer[IMAGE_BUFFER_SIZE];
    char* image = (char*) malloc(sizeof(char)*file_size);
    long bytes_read=0; 
    memset(image, 0, file_size);
    while (bytes_read < file_size) {
        memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
        n = (file_size-bytes_read < IMAGE_BUFFER_SIZE ? file_size-bytes_read : IMAGE_BUFFER_SIZE);
        n = read(fd, image_buffer, n);
        if (n < 0) {
            if (verbose)    printf(SOCKET_READING_ERROR, "TCP");
            return "ERR";
        }
        memcpy(image+bytes_read, image_buffer, n);
        bytes_read += n;
    }*/

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
    // free(image);

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

string close_auction(int fd) {
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1], AID[MAX_DIGITS+1]; 
    if (byte_reading(NULL, fd, UID, UID_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, password, PASSWORD_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, AID, MAX_DIGITS, false, true) == -1)   return "ERR";

    // Check arguments
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

    if (create_auction_end_file(AID) == -1) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_CLOSING, AID, GENERIC_CLOSE_AUCTION_ERROR);
        return "ERR";
    }

    if (verbose)    printf(SUCCESSFUL_AUCTION_CLOSING, AID);
    return "OK"; 
}

string show_asset(int fd, int *image_fd) {
    char AID[MAX_DIGITS+1];

    if (byte_reading(NULL, fd, AID, MAX_DIGITS, false, true) == -1)   return "ERR";

    string file_name = get_auction_file_name(AID);
    if (file_name == "") {
        if (verbose)    printf(UNSUCCESSFUL_SHOW_ASSET, AID, ASSET_NOT_FOUND_ERROR);
        return "NOK";
    }

    string response = "OK " + file_name;
    string file_path = "./AUCTIONS/" + string(AID) + "/ASSET/" + file_name;
    *image_fd = image_processing((char*) file_path.c_str(), &response);
    if (*image_fd == -1) {
        if (verbose)    printf(UNSUCCESSFUL_SHOW_ASSET, AID, ASSET_NOT_FOUND_ERROR);
        return "NOK";
    }

    return response;
}

string bid(int fd) {
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1], AID[MAX_DIGITS+1], value_char[START_VALUE_SIZE+1];

    if (byte_reading(NULL, fd, UID, UID_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, password, PASSWORD_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, AID, MAX_DIGITS, false, false) == -1)   return "ERR";
    if (byte_reading(NULL, fd, value_char, START_VALUE_SIZE, true, true) == -1)   return "ERR";

    long value = stol(value_char);

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
    
    string value_str = string(START_VALUE_SIZE - to_string(value).length(), '0') + to_string(value);
    if (create_bid_files(UID, AID, value, value_str) < 0) {
        if (verbose)    printf(UNSUCCESSFUL_BID, value, GENERIC_BID_ERROR);
        return "ERR";
    }

    if (verbose)    printf(SUCCESSFUL_BID, value);
    return "ACC";
}