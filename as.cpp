#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include <string>
#include <cctype>

#include "as.h"

using namespace std;

string as_port;                             // INCLUDE GROUP NUMBER!!!!!!
bool verbose=false;

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

                if (!strcmp(command_word, "LIN"))
                    response = "RLI " + login(buffer) + "\n";
                
                const char* response2 = response.c_str();
                if (write(newfd, (const char*)response2, sizeof(response2)) < 0)
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

    if ((strlen(UID) != 6) || (strlen(password) > 8)) {
        if (verbose)
            printf("%s: new login; unsuccessful login, arguments with wrong size\n", UID);
        return "NOK";
    }
    for (int i=0; i < 6; i++) {
        if (!isdigit(UID[i])) {
            if (verbose)
                printf("%s: new login; unsuccessful login, UID must be six digits\n", UID);
            return "NOK";
        }
    }

    char dir_name[BUFFER_SIZE];
    strcpy(dir_name, "./USERS/");
    strcat(dir_name, UID);

    string dir_name_str(dir_name);
    string user_file_name = dir_name_str + "/" + UID + "_login.txt";
    string password_file_name = dir_name_str + "/" + UID + "_password.txt";

    DIR* dir = opendir(dir_name);
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

    else {
        string hosted = dir_name_str + "/HOSTED";
        string bidded = dir_name_str + "/BIDDED";

        if(mkdir(dir_name, S_IRWXU) == -1)
            return "";                             // CHANGE ERROR
        
        if(mkdir(hosted.c_str(), S_IRWXU) == -1)
            return "";                             // CHANGE ERROR

        if(mkdir(bidded.c_str(), S_IRWXU) == -1)
            return "";                             // CHANGE ERROR

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