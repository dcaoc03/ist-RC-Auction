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

#include <string>
#include <cctype>
#include <list>

#include "as.h"
#include "./common/constants.h"
#include "./common/utils.h"
#include "./common/database_utils.h"
#include "./common/as_stdout_messages.h"

using namespace std;

string as_port;                             // INCLUDE GROUP NUMBER!!!!!!
bool verbose=false;

int n_auctions=0;

void process_arguments(int argc, char** argv) {         // processes the arguments given by launching the User
    for (int i=1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) 
            as_port = argv[++i];
        else if (!strcmp(argv[i], "-v"))
            verbose = true;
    }
}

int count_auctions() {
    int count = 0;
    DIR* auctions_dir = opendir("./AUCTIONS");
    struct dirent* entry;

    if (auctions_dir == NULL)
        return -1;

    while ((entry = readdir(auctions_dir)) != NULL)
        count++;

    return count-2;         // The directory keeps the directories "." and ".."
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
    char buffer[BUFFER_SIZE], command_word[COMMAND_WORD_SIZE+1];
    fd_set fdset;

    pid_t child_pid;

    /* CREATE TCP SOCKET */
   
    memset(&hints_tcp, 0, sizeof hints_tcp );
    hints_tcp.ai_family=AF_INET;
    hints_tcp.ai_socktype=SOCK_STREAM;
    hints_tcp.ai_flags=AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints_tcp, &res_tcp) != 0)     {printf(SOCKET_CREATION_ERROR, "TCP"); exit(1);}

    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_tcp == -1)     {printf(SOCKET_CREATION_ERROR, "TCP"); exit(1);}

    n = bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen);
    if (n==-1)     {printf(SOCKET_CREATION_ERROR, "TCP"); exit(1);}
    
    if (listen(fd_tcp, 5) == -1)     {printf(SOCKET_CREATION_ERROR, "TCP"); exit(1);}
    
    /* CREATE UDP SOCKET */

    memset(&hints_udp, 0, sizeof hints_udp );
    hints_udp.ai_family=AF_INET;
    hints_udp.ai_socktype=SOCK_DGRAM;
    hints_udp.ai_flags=AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints_udp, &res_udp) != 0)     {printf(SOCKET_CREATION_ERROR, "UDP"); exit(1);}

    fd_udp=socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1)     {printf(SOCKET_CREATION_ERROR, "UDP"); exit(1);}

    n=bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1)     {printf(SOCKET_CREATION_ERROR, "UDP"); exit(1);}

    /* SET UP THE FDSET */
    FD_ZERO(&fdset);

    while(1) {
        FD_SET(fd_tcp, &fdset);
        FD_SET(fd_udp, &fdset);

        select(max(fd_tcp, fd_udp)+1, &fdset, NULL, NULL, NULL);
    
        if ((n_auctions = count_auctions()) == -1) {
            if (verbose)    printf(AUCTIONS_NOT_FOUND_ERROR);
            return 1;
        }

        if (FD_ISSET(fd_tcp, &fdset)) {
            addrlen_tcp = sizeof(addr_tcp);
            if ((newfd=accept(fd_tcp, (struct sockaddr*)&addr_tcp, &addrlen_tcp)) == -1) {
                printf(SOCKET_CREATION_ERROR, "TCP");
                exit(1);
            }
            if ((child_pid = fork()) == 0) { 
                close(fd_tcp); 
                char command_word[COMMAND_WORD_SIZE+1];
                if (byte_reading(newfd, command_word, COMMAND_WORD_SIZE, false, false) < 0)    exit(1);

                /* REQUEST PROCESSING */
                string response;

                if (!strcmp(command_word, "OPA")) {
                    string status = open_auction(newfd);
                    response = "ROA " + status + "\n";
                }

                if (!strcmp(command_word, "CLS")) {
                    string status = close_auction(newfd);
                    response = "RCL " + status + "\n";
                }
                
                const char* response2 = response.c_str();
                if (write(newfd, (const char*)response2, strlen(response2)) < 0) {
                    printf(SOCKET_WRITING_ERROR, "TCP");
                    exit(1);
                }
                close(newfd); 
                exit(0); 
            }

        }
        if (FD_ISSET(fd_udp, &fdset)) { 
            addrlen_udp = sizeof(addr_udp);
            bzero(buffer, sizeof(buffer)); 
            n=recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr*)&addr_udp, &addrlen_udp);

            /* REQUEST PROCESSING */
            string response;
            sscanf(buffer, "%s", command_word);

            if (!strcmp(command_word, "LIN")) {
                string status = login(buffer);
                response = "RLI " + status + "\n";
            }

            else if (!strcmp(command_word, "LOU")) {
                response = "RLO " + logout(buffer) + "\n";
            }
            
            else if (!strcmp(command_word, "UNR")) {
                response = "RUR " + unregister(buffer) + "\n";
            }

            else if (!strcmp(command_word, "LMA")) {
                response = "RMA " + myauctions(buffer) + "\n";
            }

            else if (!strcmp(command_word, "LMB")) {
                response = "RMB " + mybids(buffer) + "\n";
            }
            
            const char* response2 = response.c_str();

            n=sendto(fd_udp, response2, strlen(response2)+1, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n==-1) {
                printf(SOCKET_WRITING_ERROR, "TCP");
                exit(1);
            }
        } 
    }

    return 0;
}

/* REQUEST HANDLING FUNCTIONS */

string login(char arguments[]) {
    char UID[UID_SIZE*2], password[PASSWORD_SIZE*2];

    sscanf(arguments, "%*s %s %s", UID, password);

    /* ARGUMENT PROCESSING*/

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
            return "OK";
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
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1];
    sscanf(arguments, "%*s %s %s", UID, password);
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
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1];
    sscanf(arguments, "%*s %s %s", UID, password);
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

string open_auction(int fd) {
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1], asset_name[ASSET_NAME_SIZE+1], start_value_str[START_VALUE_SIZE+1], 
        timeactive_str[TIMEACTIVE_SIZE+1], file_name[FILE_NAME_SIZE+1], file_size_str[FILE_SIZE_SIZE+1];

    // Reading the parameters
    if (byte_reading(fd, UID, UID_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(fd, password, PASSWORD_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(fd, asset_name, ASSET_NAME_SIZE, true, false) == -1)   return "ERR";
    if (byte_reading(fd, start_value_str, START_VALUE_SIZE, true, false) == -1)   return "ERR";
    if (byte_reading(fd, timeactive_str, TIMEACTIVE_SIZE, true, false) == -1)   return "ERR";
    if (byte_reading(fd, file_name, FILE_NAME_SIZE, true, false) == -1)   return "ERR";
    if (byte_reading(fd, file_size_str, FILE_SIZE_SIZE, true, true) == -1)   return "ERR";
    
    // Converting the numeric parameters
    int start_value = atoi(start_value_str);
    int timeactive = atoi(timeactive_str);
    int file_size = atoi(file_size_str);

    // Reading the image
    char image_buffer[IMAGE_BUFFER_SIZE];
    char* image = (char*) malloc(sizeof(char)*file_size);
    int n, bytes_read=0;
    memset(image, 0, file_size);
    while (bytes_read < file_size) {
        memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
        n = (file_size-bytes_read < IMAGE_BUFFER_SIZE ? file_size-bytes_read : IMAGE_BUFFER_SIZE);
        n = read(fd, image_buffer, n);
        if (n < 0) {
            printf(SOCKET_READING_ERROR, "TCP");
            return "ERR";
        }
        memcpy(image+bytes_read, image_buffer, n);
        bytes_read += n;
    }

    // Reading the '\n' character at the end
    char read_char;
    n = read(fd, &read_char, 1);
    if (n < 0) {
        printf(SOCKET_READING_ERROR, "TCP");
        return "ERR";
    }
    if (read_char != '\n') {
        printf(BADLY_FORMATTED_MESSAGE);
        return "ERR";
    }

    /* ARGUMENT PROCESSING */
    if ((start_value < 0) || (timeactive < 0)) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, NUMERIC_VALUES_ERROR);
        return "NOK";
    }

    if (n_auctions == MAX_AUCTIONS) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, MAX_NUM_AUCTIONS_ERROR);
        return "NOK";
    }

    if (is_user_logged_in(UID) < 0) {
        if (verbose)    printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, USER_NOT_LOGGED_IN_ERROR);
        return "NLG";
    }

    /* COMMAND EXECUTION */

    (n_auctions)++;
    int n_AID = n_auctions;
    string s_AID = to_string(n_AID);
    string AID = string(MAX_DIGITS - s_AID.length(), '0') + s_AID;

    // Create .txt on User directory
    if (create_auction_dirs(AID, UID) == -1) {
        printf(UNSUCCESSFUL_AUCTION_OPENING, asset_name, AUCTION_DIRS_ERROR);
        return "ERR";
    }

    // Create image
    copy_image(AID, file_name, file_size, image_buffer, image);
    free(image);

    // Create Start file
    create_auction_start_file(AID, UID, asset_name, file_name, start_value, timeactive);
    
    if (verbose)    printf(SUCCESSFUL_AUCTION_OPENING, asset_name, AID.c_str());
    return "OK " + AID;
}

string close_auction(int fd) {
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1], AID[MAX_DIGITS+1];
    if (byte_reading(fd, UID, UID_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(fd, password, PASSWORD_SIZE, false, false) == -1)   return "ERR";
    if (byte_reading(fd, AID, MAX_DIGITS, false, true) == -1)   return "ERR";

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

string myauctions(char arguments[]) {
    char UID[UID_SIZE+1];
    sscanf(arguments, "%*s %s", UID);
    string str_UID(UID);

    /* COMMAND EXECUTION */

    DIR* dir = does_user_exist(UID);
    if (dir) {
        closedir(dir);
        if (is_user_logged_in(UID) == -1) {
            if (verbose)    printf(UNSUCCESSFUL_MY_AUCTIONS, UID, USER_NOT_LOGGED_IN_ERROR);
            return "NLG";
        }
        else {
            list <string> hosted_list = get_hosted_auctions(UID);
            if (hosted_list.empty()) {
                if (verbose)    printf(USER_HAS_NO_ONGOING_AUCTIONS, UID);
                return "NOK";
            }
            string response = "";
            for (list<string>::iterator it = hosted_list.begin(); it != hosted_list.end(); it++) {
                string AID = *it;
                int ongoing = is_auction_ongoing(AID);
                if (ongoing == -1) {
                    if (verbose)    printf(UNSUCCESSFUL_MY_AUCTIONS, UID,GENERIC_MY_AUCTIONS_ERROR);
                    return "ERR";
                }
                response += " " + AID + (ongoing ? " 1" : " 0");
            }
            return "OK" + response;
        }
    }
    else {
        if (verbose)    printf(UNSUCCESSFUL_MY_AUCTIONS, UID, USER_NOT_REGISTERED_ERROR);
        return "ERR";
    }

}

string mybids(char arguments[]) {           // Update to use file abstraction !!!!!!!
    char UID[UID_SIZE+1];
    sscanf(arguments, "%*s %s", UID);
    string str_UID(UID);

    /* COMMAND EXECUTION */

    DIR* dir = does_user_exist(UID);
    if (dir) {
        closedir(dir);
        if (is_user_logged_in(UID) == -1) {
            if (verbose)    printf(UNSUCCESSFUL_MY_BIDS, UID, USER_NOT_LOGGED_IN_ERROR);
            return "NLG";
        }
        else {
            string bids_dir = "./USERS/" + string(UID) + "/BIDDED";
            DIR* bids = opendir(bids_dir.c_str());
            list <string> bids_list;
            struct dirent* entry;
            while ((entry = readdir(bids)) != NULL) {
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
                    bids_list.push_back(string(entry->d_name).substr(0, strlen(entry->d_name) - 4));
            }
            closedir(bids);
            if (bids_list.empty()) {
                if (verbose)    printf(USER_HAS_NO_ONGOING_BIDS, UID);
                return "NOK";
            }
            string auc_dir = "./AUCTIONS/";
            string response = "";
            for (list<string>::iterator it = bids_list.begin(); it != bids_list.end(); it++) {
                string AID = *it;
                int ongoing = is_auction_ongoing(AID);
                if (ongoing == -1) {
                    if (verbose)    printf(UNSUCCESSFUL_MY_BIDS, UID, GENERIC_MY_BIDS_ERROR);
                    return "ERR";
                }
                response += " " + AID + (ongoing ? " 1" : " 0");
            }
            return "OK" + response;
        }
    }
    else {
        if (verbose)
            printf("%s: mybids; failed to locate user in the database\n", UID);
        return "ERR";
    }
}