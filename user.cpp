#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#include <string>

#include "user.h"

using namespace std;

string as_ip_address;
string as_port;                             // INCLUDE GROUP NUMBER!!!!!!

string user_ID="";                     // When user_ID= "", the user isn't logged in
string user_password="";

/*  MAIN FUNCTIONS   */

string get_as_ip_address() {            // gets the machine's IP address and uses it as the default IP address for the AS
    string ip_address;
    char host_name[128];
    if(gethostname(host_name, 128)==-1)
        fprintf(stderr,"error: %s\n",strerror(errno));

    struct addrinfo hints,*res,*p;
    int errcode;
    char buffer[INET_ADDRSTRLEN];
    struct in_addr *addr;

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags=AI_CANONNAME;

    if((errcode=getaddrinfo(host_name,NULL,&hints,&res))!=0)
        fprintf(stderr,"error: getaddrinfo: %s\n",gai_strerror(errcode));
    else{
        for(p=res;p!=NULL;p=p->ai_next){
            addr=&((struct sockaddr_in *)p->ai_addr)->sin_addr;
            ip_address = inet_ntop(p->ai_family,addr,buffer,sizeof buffer);
        }
        freeaddrinfo(res);
    }
    return ip_address;
}

void process_arguments(int argc, char** argv) {         // processes the arguments given by launching the User
    for (int i=1; i < argc; i++) {
        if (!strcmp(argv[i], "-n")) 
            as_ip_address = argv[++i];
        else if (!strcmp(argv[i], "-p")) 
            as_port = argv[++i];
    }
}

int main(int argc, char** argv) {

    as_ip_address = get_as_ip_address();
    as_port = PORT;

    if (argc > 1)
        process_arguments(argc, argv);

    while (1) {
        char command_buffer[BUFFER_SIZE], command_word[20];
        if ((fgets(command_buffer, BUFFER_SIZE, stdin) == NULL))
            return -1;
        sscanf(command_buffer, "%s", command_word);

        if (user_ID == "") {
            if (!strcmp(command_word, "login"))
                login(command_buffer);
            else 
                printf("Please login first\n");
        }
        else {
            if (!strcmp(command_word, "login"))
                printf("User already logged in\n");
            if (!strcmp(command_word, "logout"))
                logout();
            if (!strcmp(command_word, "unregister"))
                unregister();
        }
        
    }
    return 0;
}

/*    COMMAND PROCESSING   */

void login(char arguments[]) {
    char UID[10], password[BUFFER_SIZE];
    char message[BUFFER_SIZE];

    sscanf(arguments, "%*s %s %s", UID, password);

    strcpy(message, "LIN");
    strcat(message, " ");
    strcat(message, UID);
    strcat(message, " ");
    strcat(message, password);
    strcat(message, "\n");

    int request_result = UDPclient(message, sizeof(message));
    if (request_result < 0)
        printf("error\n");                  // CHANGE ERROR HANDLING!!!!
    else if (request_result == 1) {            // If login is successful
        user_ID = UID;                         // sets the user UID
        user_password = password;              // sets the user password
    }
}

void logout() {
    char message[BUFFER_SIZE];

    strcpy(message, "LOU");
    strcat(message, " ");
    strcat(message, user_ID.c_str());
    strcat(message, " ");
    strcat(message, user_password.c_str());
    strcat(message, "\n");

    int request_result = UDPclient(message, sizeof(message));
    if (request_result < 0)
        printf("error\n");                  // CHANGE ERROR HANDLING!!!!
    else if (request_result == 1) {             // If logout is successful
        user_ID = "";                           // reset the user UID
        user_password = "";                     // reset the user password
    }
}

void unregister() {
    string message = "UNR " + user_ID + " " + user_password + "\n";
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    int request_result = UDPclient(message2, sizeof(message));
    if (request_result < 0)
        printf("error\n");                  // CHANGE ERROR HANDLING!!!!
    else if (request_result == 1) {             // If logout is successful
        user_ID = "";                           // reset the user UID
        user_password = "";                     // reset the user password
    }
}


/* SOCKET WRITING */

int UDPclient(char message[], unsigned int message_size) {      // Returns -1 if error, 0 if successful but denied, 1 if successful
    int fd;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    char buffer[128];

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        return -1;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((getaddrinfo(as_ip_address.c_str(), as_port.c_str(), &hints, &res)) != 0)       // CHECK THE NAME/IP ADDRESS!!!!
        return -1;
    
    if ((n = sendto(fd, message, message_size, 0, res->ai_addr, res->ai_addrlen)) == -1)        // CHANGE MESSAGE!!!!!
        return -1;;
    
    addrlen = sizeof(addr);
    if ((n = recvfrom(fd, buffer, 128, 0, (struct sockaddr*) &addr, &addrlen)) == -1)
        return -1;
    if ((write(1, buffer, n)) == -1)
        return -1;

    freeaddrinfo(res);
    close(fd);

    char code[10];
    sscanf(buffer, "%*s %s", code);
    if (!strcmp(code, "NOK"))
        return 0;

    return 1;
}