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

int fd_UDP;
struct addrinfo hints_UDP, *res_UDP;

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

int setup_UDP_socket() {
    if ((fd_UDP = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        return -1;
    
    memset(&hints_UDP, 0, sizeof hints_UDP);
    hints_UDP.ai_family = AF_INET;
    hints_UDP.ai_socktype = SOCK_DGRAM;

    if ((getaddrinfo(as_ip_address.c_str(), as_port.c_str(), &hints_UDP, &res_UDP)) != 0)       // CHECK THE NAME/IP ADDRESS!!!!
        return -1;
    return 0;
}

int main(int argc, char** argv) {

    as_ip_address = get_as_ip_address();
    as_port = PORT;
    int end = 0;                       // Indicates of the user is ending                    

    if (argc > 1)
        process_arguments(argc, argv);
    
    if (setup_UDP_socket() == -1)
        return 1;

    while (!end) {
        char command_buffer[BUFFER_SIZE], command_word[20];
        if ((fgets(command_buffer, BUFFER_SIZE, stdin) == NULL))
            return -1;
        sscanf(command_buffer, "%s", command_word);

        if (user_ID == "") {
            if (!strcmp(command_word, "login"))
                login(command_buffer);
            else if (!strcmp(command_word, "exit"))
                exit(&end);
            else 
                printf("Please login first\n");
        }
        else {
            if (!strcmp(command_word, "login"))
                printf("User already logged in\n");
            else if (!strcmp(command_word, "logout"))
                logout();
            else if (!strcmp(command_word, "unregister"))
                unregister();
            else if (!strcmp(command_word, "exit"))
                exit(&end);
        }
        
    }
    freeaddrinfo(res_UDP);
    close(fd_UDP);
    return 0;
}

/*    COMMAND PROCESSING   */

void login(char arguments[]) {
    char UID[10], password[BUFFER_SIZE];

    sscanf(arguments, "%*s %s %s", UID, password);

    string message = "LIN " + string(UID) + " " + password + "\n";
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = UDPclient(message2, sizeof(message2));
    if (request_result == "")
        printf("ERROR: failed to write to socket\n");
    else {            // If login is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf("User successfully logged in\n"); user_ID = UID; user_password = password;}
        else if (!strcmp(response, "NOK"))    printf("User failed to log in\n"); 
        else if (!strcmp(response, "REG"))    {printf("New user successfully reggistered\n"); user_ID = UID; user_password = password;}
    }
}

void logout() {
    string message = "LOU " + user_ID + " " + user_password + "\n";
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = UDPclient(message2, sizeof(message2));
    if (request_result == "")
        printf("ERROR: failed to write to socket\n");                  // CHANGE ERROR HANDLING!!!!
    else {             // If logout is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf("User successfully logged out\n"); user_ID = ""; user_password = "";}
        else if (!strcmp(response, "NOK"))    printf("User failed to log out\n");
        else if (!strcmp(response, "UNR"))    printf("User not registered in the database\n");
    }
}

void unregister() {
    string message = "UNR " + user_ID + " " + user_password + "\n";
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = UDPclient(message2, sizeof(message2));
    if (request_result == "")
        printf("ERROR: failed to write to socket\n");                  // CHANGE ERROR HANDLING!!!!
    else {             // If unregistration is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf("User successfully unregistered\n"); user_ID = ""; user_password = "";}
        else if (!strcmp(response, "NOK"))    printf("User failed to unregister\n");
        else if (!strcmp(response, "UNR"))    printf("User not registered in the database\n");
    }
}

void exit(int* ending) {
    if (user_ID != "") {
        printf("Please, execute the logout command before exiting\n");
        return;
    }
    else{
        *ending = 1;
        return;
    }
}


/* SOCKET WRITING */

string UDPclient(char message[], unsigned int message_size) {      // Returns -1 if error, 0 if successful but denied, 1 if successful
    ssize_t n;
    socklen_t addrlen;
    struct sockaddr_in addr;
    char buffer[128];
    
    if ((n = sendto(fd_UDP, message, message_size, 0, res_UDP->ai_addr, res_UDP->ai_addrlen)) == -1)        // CHANGE MESSAGE!!!!!
        return "";
    
    addrlen = sizeof(addr);
    if ((n = recvfrom(fd_UDP, buffer, 128, 0, (struct sockaddr*) &addr, &addrlen)) == -1)
        return "";

    return buffer;
}

int TCPclient(char message[], unsigned int message_size) {
    int fd;
    ssize_t n;
    //socklen_t addrlen;
    struct addrinfo hints, *res;
    //struct sockaddr_in addr;
    char buffer[128];

    if ((fd=socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((getaddrinfo(as_ip_address.c_str(), as_port.c_str(), &hints, &res)) != 0)       // CHECK THE NAME/IP ADDRESS!!!!
        return -1;

    n = connect(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1)
        return -1;
    n = write(fd, message, message_size);
    if (n == -1)
        return -1;
    n = read(fd, buffer, 128);
    if (n == -1)
        return -1;
    
    if ((write(1, buffer, n)) == -1)
        return -1;

    freeaddrinfo(res);
    close(fd);

    return 1;
}