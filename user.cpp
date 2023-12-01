#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>

#include <string>

#include "user.h"
#include "./common/constants.h"

using namespace std;

/* TEJO IP ADDRESS:
    - IP Address = 193.136.138.142
    - Port = 58011
*/

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
            else if (!strcmp(command_word, "open"))
                open_auction(command_buffer);
            else if (!strcmp(command_word, "close"))
                close_auction(command_buffer);
            else if (!strcmp(command_word, "myauctions") || !strcmp(command_word, "ma"))
                myauctions();
            else if (!strcmp(command_word, "mybids") || !strcmp(command_word, "mb"))
                mybids();
        }
        
    }
    freeaddrinfo(res_UDP);
    close(fd_UDP);
    return 0;
}

/*    COMMAND PROCESSING   */

void login(char arguments[]) {
    char UID[10], password[10];

    sscanf(arguments, "%*s %s %s", UID, password);

    char message[BUFFER_SIZE];
    sprintf(message, "LIN %s %s\n", UID, password);

    string request_result = UDPclient(message, sizeof(message));
    if (request_result == "ERR")
        printf("ERROR: failed to write to socket\n");
    else {            // If login is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf("User successfully logged in\n"); user_ID = UID; user_password = password;}
        else if (!strcmp(response, "NOK"))    printf("User failed to log in\n"); 
        else if (!strcmp(response, "REG"))    {printf("New user successfully reggistered\n"); user_ID = UID; user_password = password;}
        else if (!strcmp(response, "ERR"))    printf("ERROR: something wrong happened while logging in\n");
    }
}

void logout() {
    char message[BUFFER_SIZE];
    sprintf(message, "LOU %s %s\n", user_ID.c_str(), user_password.c_str());

    string request_result = UDPclient(message, sizeof(message));
    if (request_result == "ERR")
        printf("ERROR: failed to write to socket\n");                  // CHANGE ERROR HANDLING!!!!
    else {             // If logout is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf("User successfully logged out\n"); user_ID = ""; user_password = "";}
        else if (!strcmp(response, "NOK"))    printf("User failed to log out\n");
        else if (!strcmp(response, "UNR"))    printf("User not registered in the database\n");
        else if (!strcmp(response, "ERR"))    printf("ERROR: something wrong happened while logging out\n");
    }
}

void unregister() {
    char message[BUFFER_SIZE];
    sprintf(message, "UNR %s %s\n", user_ID.c_str(), user_password.c_str());

    string request_result = UDPclient(message, sizeof(message));
    if (request_result == "ERR")
        printf("ERROR: failed to write to socket\n");                  // CHANGE ERROR HANDLING!!!!
    else {             // If unregistration is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf("User successfully unregistered\n"); user_ID = ""; user_password = "";}
        else if (!strcmp(response, "NOK"))    printf("User failed to unregister\n");
        else if (!strcmp(response, "UNR"))    printf("User not registered in the database\n");
        else if (!strcmp(response, "ERR"))    printf("ERROR: something wrong happened while unregistering the user\n");
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

int image_processing(char image_name[], string* message) {
    // Get image size
    FILE* jpg_pointer = fopen(image_name, "rb");
    int fd_jpg;
    if (jpg_pointer == NULL) {
        printf("ERROR: image file not found\n"); 
        return -1;
    }
    fseek(jpg_pointer, 0, SEEK_END);
    unsigned int jpg_size = ftell(jpg_pointer);
    fseek(jpg_pointer, 0, SEEK_SET);
    if (jpg_size > MAX_JPG_SIZE) {
        printf("ERROR: file size exceeds 10 MB\n"); 
        return -1;
    }
    fclose(jpg_pointer);
    *message += " " + to_string(jpg_size) + " ";

    if((fd_jpg = open(image_name, S_IRUSR)) < 0) {
        printf("ERROR: failed to open jpg file\n");
        return -1;
    }
    return fd_jpg;
}

void open_auction(char arguments[]) {
    string message = "OPA " + user_ID + " " + user_password;
    char asset_name[20], file_name[BUFFER_SIZE];
    int start_value, timeactive, jpg_fd;

    sscanf(arguments, "%*s %s %s %d %d", asset_name, file_name, &start_value, &timeactive);

    if (strlen(asset_name) > ASSET_NAME_SIZE) {
        printf("ERROR: file asset_name exceeds %d alphanumerical characters\n", ASSET_NAME_SIZE); 
        return;
    }
    if (strlen(file_name) > FILE_NAME_SIZE) {
        printf("ERROR: file asset_name exceeds %d alphanumerical characters\n", FILE_NAME_SIZE); 
        return;
    }

    message += " " + string(asset_name) + " " + to_string(start_value) + " " + to_string(timeactive)
        + " " + file_name;

    if ((jpg_fd = image_processing(file_name, &message)) == -1) {
        printf("ERROR: failed to open jpg\n"); 
        return;
    }

    const char* message2 = message.c_str();
    string request_result = TCPclient(message2, strlen(message2), &jpg_fd);
    if (request_result == "ERR")
        printf("ERROR: failed to write to socket\n");
    else {
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK")) {
            char AID[COMMAND_WORD_SIZE+1];
            sscanf(request_result.c_str(), "%*s %*s %s", AID);
            printf("Auction %s successfully started\n", AID); 
        }
        else if (!strcmp(response, "NOK"))    printf("Auction failed to start\n");
        else if (!strcmp(response, "NLG"))    printf("User is not logged in\n");
        else if (!strcmp(response, "ERR"))    printf("ERROR: something wrong happened while creating an auction\n");
    }

    close(jpg_fd);
}

void close_auction(char arguments[]) {
    char AID[MAX_DIGITS+1];
    sscanf(arguments, "%*s %s", AID);

    string message = "CLS " + user_ID + " " + user_password +" " + AID + '\n';
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = TCPclient(message2, sizeof(message2), NULL);
    if (request_result == "ERR")
        printf("ERROR: failed to write to socket\n");                  // CHANGE ERROR HANDLING!!!!
    else {
        char response[COMMAND_WORD_SIZE+1];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))         printf("Auction %s successfully closed\n", AID);
        if (!strcmp(response, "NLG"))        printf("Used user not logged in\n");
        if (!strcmp(response, "EAU"))        printf("Auction %s does not exist\n", AID);
        if (!strcmp(response, "EOW"))        printf("Auction %s is not owned by user %s\n", AID, user_ID.c_str());
        if (!strcmp(response, "END"))        printf("Auction %s has already ended\n", AID);
        else if (!strcmp(response, "ERR"))    printf("ERROR: something wrong happened while closing an auctiont\n");
    }
     
}

void myauctions() {
    string message = "LMA " + user_ID + "\n";
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = UDPclient(message2, sizeof(message2));
    if (request_result == "ERR")
        printf("ERROR: failed to write to socket\n");                  // CHANGE ERROR HANDLING!!!!
    else {             // If myauctions is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "NOK"))         printf("User has no ongoing auctions\n");
        else if (!strcmp(response, "NLG"))    printf("User is not logged in\n");
        else if (!strcmp(response, "ERR"))    printf("ERROR: something went wrong while listing the auctions\n");
        else {
            char auctions_list[BUFFER_SIZE];
            sscanf(request_result.c_str(), "%*s %*s %[^\n]", auctions_list);
            printf("%s\n", auctions_list);
        }
    }

}

void mybids() {
    string message = "LMB " + user_ID + "\n";
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = UDPclient(message2, sizeof(message2));
    if (request_result == "ERR")
        printf("ERROR: failed to write to socket\n");                  // CHANGE ERROR HANDLING!!!!
    else {             // If mybids is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "NOK"))         printf("User has no ongoing bids\n");
        else if (!strcmp(response, "NLG"))    printf("User is not logged in\n");
        else if (!strcmp(response, "ERR"))    printf("ERROR: something went wrong while listing the bids\n");
        else {
            char auctions_list[BUFFER_SIZE];
            sscanf(request_result.c_str(), "%*s %*s %[^\n]", auctions_list);
            printf("%s\n", auctions_list);
        }
    }
}


/* SOCKET WRITING */

string UDPclient(char message[], unsigned int message_size) {      // Returns -1 if error, 0 if successful but denied, 1 if successful
    ssize_t n;
    socklen_t addrlen;
    struct sockaddr_in addr;
    char buffer[128];
    
    if ((n = sendto(fd_UDP, message, strlen(message), 0, res_UDP->ai_addr, res_UDP->ai_addrlen)) == -1)
        return "ERR";
    
    addrlen = sizeof(addr);
    if ((n = recvfrom(fd_UDP, buffer, 128, 0, (struct sockaddr*) &addr, &addrlen)) == -1)
        return "ERR";

    return buffer;
}




string TCPclient(const char message[], unsigned int message_size, int *image_fd) {
    int fd;
    ssize_t n;
    struct addrinfo hints, *res;
    char buffer[128];

    fd=socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)   {printf("ERROR: failed to create TCP socket\n");  return "ERR";}

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    n = getaddrinfo(as_ip_address.c_str(), as_port.c_str(), &hints, &res);
    if (n != 0)     {printf("ERROR: failed to create TCP socket\n");  return "ERR";}
    n = connect(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1)    {printf("ERROR: failed to create TCP socket\n");  return "ERR";}
    n = write(fd, message, strlen(message));
    if (n == -1)    {printf("ERROR: failed to create TCP socket\n");  return "ERR";}

    // Sending the Image
    if (image_fd != NULL) {
        int image_size, bytes_read=0;
        char image_buffer[IMAGE_BUFFER_SIZE];
        sscanf(message, "%*s %*s %*s %*s %*d %*d %*s %d", &image_size);
        while (bytes_read < image_size) {
            memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
            n = read(*image_fd, image_buffer, IMAGE_BUFFER_SIZE);
            if (n<0)    {printf("ERROR: failed to read from socket\n");  return "ERR";}
            n = write(fd, image_buffer, n);
            if (n<0)    {printf("ERROR: failed to read from socket\n");  return "ERR";}
            bytes_read += n;
        }
        char new_line_char = '\n';
        n = write(fd, &new_line_char, 1);
    }

    // Await the answer
    n = read(fd, buffer, 128);
    if (n == -1)
        return "ERR";

    freeaddrinfo(res);
    close(fd);

    return buffer;
}