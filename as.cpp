#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<sys/wait.h>
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

#include "as.h"
#include "./common/constants.h"

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

    if (getaddrinfo(NULL, PORT, &hints_tcp, &res_tcp) != 0)
        exit(1);

    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_tcp == -1)
        exit(1);

    n = bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen);
    if (n==-1)
        exit(1);
    
    if (listen(fd_tcp, 5) == -1)
        exit(1);
    
    /* CREATE UDP SOCKET */

    memset(&hints_udp, 0, sizeof hints_udp );
    hints_udp.ai_family=AF_INET;
    hints_udp.ai_socktype=SOCK_DGRAM;
    hints_udp.ai_flags=AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints_udp, &res_udp) != 0)
        exit(1);

    fd_udp=socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1)
        exit(1);

    n=bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1)
        exit(1);

    /* SET UP THE FDSET */
    FD_ZERO(&fdset);

    while(1) {
        FD_SET(fd_tcp, &fdset);
        FD_SET(fd_udp, &fdset);

        select(max(fd_tcp, fd_udp)+1, &fdset, NULL, NULL, NULL);
    
        if ((n_auctions = count_auctions()) == -1) {
            if (verbose)    printf("Database error: directory \"AUCTIONS\" not found");
            return 1;
        }

        if (FD_ISSET(fd_tcp, &fdset)) {
            addrlen_tcp = sizeof(addr_tcp);
            if ((newfd=accept(fd_tcp, (struct sockaddr*)&addr_tcp, &addrlen_tcp)) == -1)
                exit(1);
            if ((child_pid = fork()) == 0) { 
                close(fd_tcp); 
                char command_word[COMMAND_WORD_SIZE+1];
                if (byte_reading(newfd, command_word, COMMAND_WORD_SIZE, false) < 0)    exit(1);

                /* REQUEST PROCESSING */
                string response;

                if (!strcmp(command_word, "OPA")) {
                    string status = open_auction(newfd);
                    response = "ROA " + status + "\n";
                }
                
                const char* response2 = response.c_str();
                if (write(newfd, (const char*)response2, strlen(response2)) < 0)
                    exit(1); 
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
            
            const char* response2 = response.c_str();

            n=sendto(fd_udp, response2, strlen(response2)+1, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if(n==-1)
                exit(1);
        } 
    }

    return 0;
}

/* REQUEST HANDLING FUNCTIONS */

string login(char arguments[]) {
    char UID[10], password[BUFFER_SIZE];

    sscanf(arguments, "%*s %s %s", UID, password);

    /* ARGUMENT PROCESSING*/

    if ((strlen(UID) != 6) || (strlen(password) != 8)) {
        if (verbose)
            printf("%s: new login; unsuccessful login, arguments with wrong size\n", UID);
        return "ERR";
    }
    for (int i=0; i < 8; i++) {
        if ((i < 6) && !isdigit(UID[i])) {
            if (verbose)    printf("%s: new login; unsuccessful login, UID must be six digits\n", UID);
            return "ERR";
        }
        if (!isdigit(password[i]) && (isalpha(password[i]) == 0)) {
            if (verbose)    printf("%s: new login; unsuccessful login, password must be eight alphanumeric digits\n", UID);
            return "ERR";
        }
    }

    /* COMMAND EXECUTION */

    char dir_name[BUFFER_SIZE];
    strcpy(dir_name, "./USERS/");
    strcat(dir_name, UID);

    string dir_name_str(dir_name);
    string user_file_name = dir_name_str + "/" + UID + "_login.txt";
    string password_file_name = dir_name_str + "/" + UID + "_password.txt";

    DIR* dir = opendir(dir_name);
    // User exists -> login
    if (dir) {
        closedir(dir);
        int file_exists = access(password_file_name.c_str(), F_OK);
        if (file_exists == 0) {
            FILE* fd_pass = fopen(password_file_name.c_str(), "r");

            char password_in_file[BUFFER_SIZE];
            memset(password_in_file, 0, sizeof(password_in_file));

            if (fread(password_in_file, sizeof(char), BUFFER_SIZE, fd_pass) < 0)
                return "";                             // CHANGE ERROR
            fclose(fd_pass);

            if (!strcmp(password_in_file, password)) {
                FILE* fd_user = fopen(user_file_name.c_str(), "w");
                fclose(fd_user);
                if (verbose)
                    printf("%s: new login; successful login\n", UID);
                return "OK";
            }
            else {
                if (verbose)
                    printf("%s: new login; unsuccessful login\n", UID);
                return "NOK";
            }
        } else if (file_exists == -1) {
            FILE* fd_user = fopen(user_file_name.c_str(), "w");
            FILE* fd_pass = fopen(password_file_name.c_str(), "w");

            fprintf(fd_pass, "%s", password);

            fclose(fd_user);
            fclose(fd_pass);

            if (verbose)
                printf("%s: new login; successful login\n", UID);
            return "OK";
        }

        return "ERR";

    }
    // User does not exist -> new registration
    else {
        string hosted = dir_name_str + "/HOSTED";
        string bidded = dir_name_str + "/BIDDED";

        if(mkdir(dir_name, S_IRWXU) == -1) {
            if (verbose)    printf("%s: new login; failed to create a user directory\n", UID);
            return "NOK";
        }
        
        if(mkdir(hosted.c_str(), S_IRWXU) == -1) {
            if (verbose)    printf("%s: new login; failed to create a user directory\n", UID);
            return "NOK";
        }

        if(mkdir(bidded.c_str(), S_IRWXU) == -1) {
            if (verbose)    printf("%s: new login; failed to create a user directory\n", UID);
            return "NOK";
        }

        FILE* fd_user = fopen(user_file_name.c_str(), "w");
        FILE* fd_pass = fopen(password_file_name.c_str(), "w");

        fprintf(fd_pass, "%s", password);

        fclose(fd_user);
        fclose(fd_pass);

        if (verbose)
            printf("%s: new registration; successfully registered\n", UID);
        return "REG";
    }
}

string logout(char arguments[]) {
    char UID[10], password[BUFFER_SIZE];
    sscanf(arguments, "%*s %s %s", UID, password);
    string str_UID(UID);

    /* COMMAND EXECUTION */

    string dir_name = "./USERS/" + str_UID;
    DIR* dir = opendir(dir_name.c_str());
    if (dir) {
        closedir(dir);
        string login_file_name = dir_name + "/" + str_UID + "_login.txt";
        if (access(login_file_name.c_str(), F_OK) == -1) {
            if (verbose)
                printf("%s: logout; user isn't logged in\n", UID);
            return "NOK";
        }
        else {
            if (remove(login_file_name.c_str()) != 0) {
                if (verbose)
                    printf("%s: logout; failed to remove user\n", UID);
                return "NOK";
            }
            else {
                if (verbose)
                    printf("%s: logout; logout successful\n", UID);
                return "OK";
            }
        }
    }
    else {
        if (verbose)
            printf("%s: logout; failed to locate user in the database\n", UID);
        return "UNR";
    }
}

string unregister(char arguments[]) {
    char UID[10], password[BUFFER_SIZE];
    sscanf(arguments, "%*s %s %s", UID, password);
    string str_UID(UID);

    /* COMMAND EXECUTION */

    string dir_name = "./USERS/" + str_UID;
    DIR* dir = opendir(dir_name.c_str());
    if (dir) {
        closedir(dir);
        string login_file_name = dir_name + "/" + str_UID + "_login.txt";
        string password_file_name = dir_name + "/" + str_UID + "_password.txt";
        if (access(login_file_name.c_str(), F_OK) == -1) {
            if (verbose)
                printf("%s: unregister; user isn't logged in\n", UID);
            return "NOK";
        }
        else {
            if ((remove(login_file_name.c_str()) != 0) || remove(password_file_name.c_str()) != 0) {
                if (verbose)
                    printf("%s: unregister; failed to unregister\n", UID);
                return "NOK";
            }
            else  {
                if (verbose)
                    printf("%s: unregister; successsful unregistration\n", UID);
                return "OK";
            }
        }
    }
    else  {
        if (verbose)
            printf("%s: logout; failed to locate user in the database\n", UID);
        return "UNR";
    }
}

string open_auction(int fd) {           // ADD NLG RESPONSE !!!!!!!!
    char UID[UID_SIZE+1], password[PASSWORD_SIZE+1], asset_name[ASSET_NAME_SIZE+1], start_value_str[START_VALUE_SIZE+1], 
        timeactive_str[TIMEACTIVE_SIZE+1], file_name[FILE_NAME_SIZE+1], file_size_str[FILE_SIZE_SIZE+1];

    // Reading the parameters
    if (byte_reading(fd, UID, UID_SIZE, false) == -1)   return "ERR";
    if (byte_reading(fd, password, PASSWORD_SIZE, false) == -1)   return "ERR";
    if (byte_reading(fd, asset_name, ASSET_NAME_SIZE, true) == -1)   return "ERR";
    if (byte_reading(fd, start_value_str, START_VALUE_SIZE, true) == -1)   return "ERR";
    if (byte_reading(fd, timeactive_str, TIMEACTIVE_SIZE, true) == -1)   return "ERR";
    if (byte_reading(fd, file_name, FILE_NAME_SIZE, true) == -1)   return "ERR";
    if (byte_reading(fd, file_size_str, FILE_SIZE_SIZE, true) == -1)   return "ERR";
    
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
            printf("ERROR: failed to read from socket\n");
            return "ERR";
        }
        memcpy(image+bytes_read, image_buffer, n);
        bytes_read += n;
    }

    // Reading the '\n' character at the end
    char read_char;
    n = read(fd, &read_char, 1);
    if (n < 0) {
        printf("ERROR: failed to read from socket\n");
        return "ERR";
    }
    if (read_char != '\n') {
        printf("ERROR: badly formatted message\n");
        return "ERR";
    }

    /* ARGUMENT PROCESSING */
    if ((start_value < 0) || (timeactive < 0)) {
        if (verbose)    printf("%s: open; values should be positive or 0", asset_name);
        return "NOK";
    }

    if (n_auctions == MAX_AUCTIONS) {
        if (verbose)    printf("%s: open; maximum number of auctions has been reached", asset_name);
        return "NOK";
    }

    /* COMMAND EXECUTION */

    (n_auctions)++;
    int n_AID = n_auctions;
    string s_AID = to_string(n_AID);
    string AID = string(MAX_DIGITS - s_AID.length(), '0') + s_AID;

    // Create .txt on User directory
    char dir_name[BUFFER_SIZE];
    strcpy(dir_name, "./USERS/");
    strcat(dir_name, UID);

    string hosted_file = string(dir_name) + "/HOSTED/" + AID + ".txt";
    FILE* fd_hosted = fopen(hosted_file.c_str(), "w");
    fclose(fd_hosted);

    // Create directory in AUCTIONS
    string auctions_dir = "./AUCTIONS/" + AID;
    string bids_dir = auctions_dir + "/BIDS";
    if (mkdir(auctions_dir.c_str(), S_IRWXU) == -1) {
        if (verbose)    printf("%s: new auction; failed to create a auction directory\n", AID.c_str());
        return "NOK";
    }
    if (mkdir(bids_dir.c_str(), S_IRWXU) == -1) {
        if (verbose)    printf("%s: new auction; failed to create a auction directory\n", AID.c_str());
        return "NOK";
    }

    // Create image
    string image_name = auctions_dir + "/" + file_name;
    FILE* fd_image = fopen(image_name.c_str(), "w");
    bytes_read = 0;
    while (bytes_read < file_size) {
        n = (file_size-bytes_read < IMAGE_BUFFER_SIZE ? file_size-bytes_read : IMAGE_BUFFER_SIZE);
        memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
        memcpy(image_buffer, image+bytes_read, n);
        n = fwrite(image_buffer, 1, n, fd_image);
        bytes_read += n;
    }
    fclose(fd_image);
    free(image);

    // Create Start file
    string start_file_name = auctions_dir + "/START_"+ AID +".txt";
    FILE* fd_start_file = fopen(start_file_name.c_str(), "w");
    string start_file_content = string(UID) + " " + asset_name + " " + file_name + " " + to_string(start_value) + " " +
        to_string(timeactive) + " ";
    fprintf(fd_start_file, "%s", start_file_content.c_str());
    time_t start_fulltime = time(NULL);
    struct tm *start_date_time = localtime(&start_fulltime);
    fprintf(fd_start_file, "%04d-%02d-%02d %02d:%02d:%02d %ld", start_date_time->tm_year+1900, start_date_time->tm_mon + 1, start_date_time->tm_mday,
        start_date_time->tm_hour, start_date_time->tm_min, start_date_time->tm_sec, start_fulltime);

    fclose(fd_start_file);
    if (verbose)    printf("%s: new auction; new auction successfully created\n", AID.c_str());
    return "OK " + AID;
}