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
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    char buffer[BUFFER_SIZE], command_word[BUFFER_SIZE];
    fd_set fdset;

    pid_t child_pid;

    memset(&hints, 0, sizeof hints );
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags=AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0)
        exit(1);

    /* CREATE TCP SOCKET */
   
    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_tcp == -1)
        exit(1); 

    n = bind(fd_tcp, res->ai_addr, res->ai_addrlen);
    if (n==-1)
        exit(1);
    
    if (listen(fd_tcp, 5) == -1)
        exit(1);
    
    /* CREATE UDP SOCKET */

    fd_udp=socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1)
        exit(1);

    n=bind(fd_udp, res->ai_addr, res->ai_addrlen);
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
            addrlen = sizeof(addr);
            if ((newfd=accept(fd_tcp, (struct sockaddr*)&addr, &addrlen)) == -1)
                exit(1);
            if ((child_pid = fork()) == 0) { 
                close(fd_tcp); 
                bzero(buffer, sizeof(buffer)); 
                printf("Message From TCP client: "); 
                if (read(newfd, buffer, sizeof(buffer)) < 0)
                    exit(1); 
                puts(buffer);

                /* REQUEST PROCESSING */
                string response;
                sscanf(buffer, "%s", command_word);
                printf("Command word: %s\n", command_word); 

                if (!strcmp(command_word, "OPA")) {
                    string status = open_auction(buffer, newfd);
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
            addrlen = sizeof(addr);
            bzero(buffer, sizeof(buffer)); 
            n=recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr*)&addr, &addrlen);

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

            n=sendto(fd_udp, response2, strlen(response2)+1, 0, (struct sockaddr*) &addr, addrlen);
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
        return "NOK";
    }
    for (int i=0; i < 8; i++) {
        if ((i < 6) && !isdigit(UID[i])) {
            if (verbose)    printf("%s: new login; unsuccessful login, UID must be six digits\n", UID);
            return "NOK";
        }
        if (!isdigit(password[i]) && (isalpha(password[i]) == 0)) {
            if (verbose)    printf("%s: new login; unsuccessful login, password must be eight alphanumeric digits\n", UID);
            return "NOK";
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

string open_auction(char arguments[], int fd) {
    printf("I opened\n");
    char UID[10], name[20], asset_name[BUFFER_SIZE], image[MAX_JPG_SIZE];
    int start_value, timeactive, asset_size, n;
    sscanf(arguments, "%*s %s %*s %s %d %d %s %d", UID, name, &start_value, &timeactive, asset_name, &asset_size);

    /*printf("Image being received: %s\n", image);
    for (int i=asset_size; i > 0; i -= IMAGE_TRANSMISSION_SPEED) {
        printf("%d\n", i);
        n = read(fd, image+(asset_size-i), IMAGE_TRANSMISSION_SPEED);
        if (n == -1)
            return "";           
    }
    printf("Image received: %s\n", image);*/

    /* ARGUMENT PROCESSING */

    if ((start_value < 0) || (timeactive < 0)) {
        if (verbose)    printf("%s: open; values should be positive or 0", name);
        return "NOK";
    }

    if (n_auctions == MAX_AUCTIONS) {
        if (verbose)    printf("%s: open; maximum number of auctions has been reached", name);
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

    string start_file_name = auctions_dir + "/START_"+ AID +".txt";
    FILE* fd_start_file = fopen(start_file_name.c_str(), "w");
    string start_file_content = string(UID) + " " + name + " " + asset_name + " " + to_string(start_value) + " " +
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