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

#include "as.h"

using namespace std;

string as_port;
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
            printf("\nMessage from UDP client: "); 
            n=recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr*)&addr, &addrlen); 
            puts(buffer); 

            /* REQUEST PROCESSING */
            string response;
            sscanf(buffer, "%s", command_word);

            if (!strcmp(command_word, "LIN"))
                response = "RLI " + login(buffer) + "\n";
            
            const char* response2 = response.c_str();

            n=sendto(fd_udp, response2, strlen(response2)+1, 0, (struct sockaddr*) &addr, addrlen);
            if(n==-1)
                exit(1);
        } 
    }

    return 0;
}

string login(char arguments[]) {
    char UID[10], password[BUFFER_SIZE], dir_name[BUFFER_SIZE];
    char user_file_name[BUFFER_SIZE], password_file_name[BUFFER_SIZE];

    sscanf(arguments, "%*s %s %s", UID, password);

    strcpy(dir_name, "./USERS/");
    strcat(dir_name, UID);

    strcpy(user_file_name, dir_name);
    strcat(user_file_name, "/");
    strcat(user_file_name, UID);
    strcat(user_file_name, "_login.txt");

    strcpy(password_file_name, dir_name);
    strcat(password_file_name, "/");
    strcat(password_file_name, UID);
    strcat(password_file_name, "_password.txt");

    if ((opendir(dir_name)) == NULL) {
        char hosted[BUFFER_SIZE], bidded[BUFFER_SIZE];
        strcpy(hosted, dir_name);
        strcat(hosted, "/HOSTED");
        strcpy(bidded, dir_name);
        strcat(bidded, "/BIDDED");

        if(mkdir(dir_name, S_IRWXU) == -1)
            return "";                             // CHANGE ERROR
        
        if(mkdir(hosted, S_IRWXU) == -1)
            return "";                             // CHANGE ERROR

        if(mkdir(bidded, S_IRWXU) == -1)
            return "";                             // CHANGE ERROR

        FILE* fd_user = fopen(user_file_name, "w");
        FILE* fd_pass = fopen(password_file_name, "w");

        fprintf(fd_pass, "%s", password);

        fclose(fd_user);
        fclose(fd_pass);

        return "REG";
    }

    else {
        FILE* fd_pass = fopen(password_file_name, "r");

        char password_in_file[BUFFER_SIZE];
        memset(password_in_file, 0, sizeof(password_in_file));

        if (fread(password_in_file, sizeof(char), strlen(password)+1, fd_pass) < 0)
            return "";                             // CHANGE ERROR
        fclose(fd_pass);

        if (!strcmp(password_in_file, password)) {
            FILE* fd_user = fopen(user_file_name, "w");
            fclose(fd_user);
            return "OK";
        }
        else {
            return "NOK";
        }
    }
}