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

#include "user.hpp"
#include "./common/constants.h"
#include "./common/user_stdout_messages.h"
#include "./common/utils.hpp"

using namespace std;

/* TEJO IP ADDRESS:
    - IP Address = 193.136.138.142
    - Port = 58011
*/

string as_ip_address;
string as_port;

string user_ID="";                     // When user_ID= "", the user isn't logged in
string user_password="";



/*  +----------------------------------+
    |                                  |
    |          MAIN FUNCTIONS          |
    |                                  |
    +----------------------------------+  */

string get_as_ip_address() {            // gets the machine's IP address and uses it as the default IP address for the AS (unnecesarilly complicated)
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
    int end = 0;                       // Indicates of the user is ending                    

    if (argc > 1)
        process_arguments(argc, argv);

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
                printf(LOGIN_FIRST);
        }
        else {
            if (!strcmp(command_word, "login"))
                printf(ALREADY_LOGGED_IN);
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
            else if (!strcmp(command_word, "list") || !strcmp(command_word, "l"))
                list_auctions();
            else if (!strcmp(command_word, "bid") || !strcmp(command_word, "b"))
                bid(command_buffer);
            else if (!strcmp(command_word, "show_asset") || !strcmp(command_word, "sa"))
                show_asset(command_buffer);
            else if (!strcmp(command_word, "show_record") || !strcmp(command_word, "sr"))
                show_record(command_buffer);
        }
        
    }
    return 0;
}



/*  +--------------------------------------+
    |                                      |
    |          COMMAND PROCESSING          |
    |                                      |
    +--------------------------------------+  */

/*  +------- Flux Control Functions -------+  */

void exit(int* ending) {
    if (user_ID != "") {
        printf(EXIT_BEFORE_LOGOUT_ERROR);
        return;
    }
    else{
        *ending = 1;
        return;
    }
}


/*  +------------ UDP Commands ------------+ */

void login(char arguments[]) {
    char UID[10], password[10];

    sscanf(arguments, "%*s %s %s", UID, password);

    char message[BUFFER_SIZE];
    sprintf(message, "LIN %s %s\n", UID, password);

    string request_result = UDPclient(message, sizeof(message));
    if (request_result == "ERR")
        return;
    else {            // If login is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf(SUCCESSFUL_LOGIN_USER); user_ID = UID; user_password = password;}
        else if (!strcmp(response, "NOK"))    printf(UNSUCCESSFUL_LOGIN_USER); 
        else if (!strcmp(response, "REG"))    {printf(SUCCESSFUL_RESGISTRATION_USER); user_ID = UID; user_password = password;}
        else if (!strcmp(response, "ERR"))    printf(GENERIC_LOGIN_ERROR_USER);
    }
}

void logout() {
    char message[BUFFER_SIZE];
    sprintf(message, "LOU %s %s\n", user_ID.c_str(), user_password.c_str());

    string request_result = UDPclient(message, sizeof(message));
    if (request_result == "ERR")
        return;
    else {             // If logout is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf(SUCCESSFUL_LOGOUT_USER); user_ID = ""; user_password = "";}
        else if (!strcmp(response, "NOK"))    printf(UNSUCCESSFUL_LOGOUT_USER);
        else if (!strcmp(response, "UNR"))    printf(USER_NOT_REGISTERED_ERROR_USER);
        else if (!strcmp(response, "ERR"))    printf(GENERIC_LOGOUT_ERROR_USER);
    }
}

void unregister() {
    char message[BUFFER_SIZE];
    sprintf(message, "UNR %s %s\n", user_ID.c_str(), user_password.c_str());

    string request_result = UDPclient(message, sizeof(message));
    if (request_result == "ERR")
        return;
    else {             // If unregistration is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))          {printf(SUCCESSFUL_UNREGISTRATION_USER); user_ID = ""; user_password = "";}
        else if (!strcmp(response, "NOK"))    printf(UNSUCCESSFUL_UNREGISTRATION_USER);
        else if (!strcmp(response, "UNR"))    printf(USER_NOT_REGISTERED_ERROR_USER);
        else if (!strcmp(response, "ERR"))    printf(GENERIC_UNREGISTRATION_ERROR_USER);
    }
}

void myauctions() {
    string message = "LMA " + user_ID + "\n";
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = UDPclient(message2, sizeof(message2));
    if (request_result == "ERR")
        return;
    else {             // If myauctions is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "NOK"))         printf(NO_ONGOING_AUCTIONS_ERROR_USER);
        else if (!strcmp(response, "NLG"))    printf(USER_NOT_LOGGED_IN_ERROR_USER);
        else if (!strcmp(response, "ERR"))    printf(GENERIC_MY_AUCTIONS_ERROR_USER);
        else {
            char auctions_list[UDP_BUFFER_SIZE];
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
        return;
    else {             // If mybids is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "NOK"))         printf(NO_ONGOING_BIDS_ERROR_USER);
        else if (!strcmp(response, "NLG"))    printf(USER_NOT_LOGGED_IN_ERROR_USER);
        else if (!strcmp(response, "ERR"))    printf(GENERIC_MY_BIDS_ERROR_USER);
        else {
            char auctions_list[UDP_BUFFER_SIZE];
            sscanf(request_result.c_str(), "%*s %*s %[^\n]", auctions_list);
            printf("%s\n", auctions_list);
        }
    }
}

void list_auctions() {
    string message = "LST\n";
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = UDPclient(message2, sizeof(message2));
    if (request_result == "ERR")
        return;
    else {             // If list is successful
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "NOK"))         printf("No auctions are currently open\n");
        else if (!strcmp(response, "ERR"))    printf("ERROR: something went wrong while listing the auctions\n");
        else {
            char auctions_list[UDP_BUFFER_SIZE];
            sscanf(request_result.c_str(), "%*s %*s %[^\n]", auctions_list);
            printf("%s\n", auctions_list);
        }
    }
}

void show_record(char arguments[]) {
    char AID[MAX_DIGITS+1];
    sscanf(arguments, "%*s %s", AID);

    char message[BUFFER_SIZE];
    sprintf(message, "SRC %s\n", AID);

    string request_result = UDPclient(message, sizeof(message));
    if (request_result == "ERR")
        return;
    else {
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "NOK"))
            printf(UNSUCCESSFUL_SHOW_RECORD_USER, AID);
        else if (!strcmp(response, "OK")) {
            printf("received : %s\n", request_result.c_str());
        }
    }
}

/*  +------------ TCP Commands ------------+ */

void open_auction(char arguments[]) {
    string message = "OPA " + user_ID + " " + user_password;
    char asset_name[20], file_name[BUFFER_SIZE];
    long start_value, timeactive;
    int  jpg_fd;

    sscanf(arguments, "%*s %s %s %ld %ld", asset_name, file_name, &start_value, &timeactive);

    if (strlen(asset_name) > ASSET_NAME_SIZE) {
        printf(ARGUMENTS_SIZE_ERROR, "asset_name", ASSET_NAME_SIZE); 
        return;
    }
    if (strlen(file_name) > FILE_NAME_SIZE) {
        printf(ARGUMENTS_SIZE_ERROR, "file_name", FILE_NAME_SIZE); 
        return;
    }

    message += " " + string(asset_name) + " " + to_string(start_value) + " " + to_string(timeactive)
        + " " + file_name;

    if ((jpg_fd = image_processing(file_name, &message)) == -1) {
        printf(IMAGE_PROCESSING_ERROR); 
        return;
    }

    const char* message2 = message.c_str();
    string request_result = TCPclient(message2, strlen(message2), &jpg_fd, NULL);
    if (request_result == "ERR")
        return;
    else {
        char response[BUFFER_SIZE];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK")) {
            char AID[COMMAND_WORD_SIZE+1];
            sscanf(request_result.c_str(), "%*s %*s %s", AID);
            printf(SUCCESSFUL_AUCTION_OPENING_USER, AID); 
        }
        else if (!strcmp(response, "NOK"))    printf(UNSUCCESSFUL_AUCTION_OPENING_USER);
        else if (!strcmp(response, "NLG"))    printf(USER_NOT_LOGGED_IN_ERROR_USER);
        else if (!strcmp(response, "ERR"))    printf(GENERIC_AUCTION_OPENING_ERROR_USER);
    }

    close(jpg_fd);
}

void close_auction(char arguments[]) {
    char AID[MAX_DIGITS+1];
    sscanf(arguments, "%*s %s", AID);

    string message = "CLS " + user_ID + " " + user_password +" " + AID + '\n';
    char message2[BUFFER_SIZE];
    strcpy(message2, message.c_str());

    string request_result = TCPclient(message2, sizeof(message2), NULL, NULL);
    if (request_result == "ERR")
        return;
    else {
        char response[COMMAND_WORD_SIZE+1];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "OK"))         printf(SUCCESSFUL_AUCTION_CLOSING_USER, AID);
        if (!strcmp(response, "NLG"))        printf(USER_NOT_LOGGED_IN_ERROR_USER);
        if (!strcmp(response, "EAU"))        printf(USER_NOT_REGISTERED_ERROR_USER);
        if (!strcmp(response, "EOW"))        printf(UNALLOWED_AUCTION_CLOSING_USER, AID, user_ID.c_str());
        if (!strcmp(response, "END"))        printf(AUCTION_ALREADY_CLOSED_ERROR_USER, AID);
        else if (!strcmp(response, "ERR"))    printf(GENERIC_AUCTION_OPENING_ERROR_USER);
    }
     
}

void show_asset(char arguments[]) {
    char AID[MAX_DIGITS+1];
    sscanf(arguments, "%*s %s", AID);

    char message[BUFFER_SIZE];
    sprintf(message, "SAS %s\n", AID);

    int socket_fd;
    string request_result = TCPclient(message, sizeof(message), NULL, &socket_fd);

    // Complexity of reading the socket is made here
    char response_word[COMMAND_WORD_SIZE+1], status[COMMAND_WORD_SIZE+1];
    if (byte_reading(socket_fd, response_word, COMMAND_WORD_SIZE, false, false))     printf(GENERIC_SHOW_ASSET_ERROR);
    if (byte_reading(socket_fd, status, COMMAND_WORD_SIZE, true, false))             printf(GENERIC_SHOW_ASSET_ERROR);

    if (!strcmp(status, "ERR") || !strcmp(status, "NOK")) 
        printf(GENERIC_SHOW_ASSET_ERROR);
    else if (!strcmp(status, "OK")) {
        char file_name[FILE_NAME_SIZE+1], file_size_str[FILE_SIZE_SIZE+1];
        if (byte_reading(socket_fd, file_name, FILE_NAME_SIZE, true, false))        {printf(GENERIC_SHOW_ASSET_ERROR); close(socket_fd); return;}
        if (byte_reading(socket_fd, file_size_str, FILE_SIZE_SIZE, true, false))        {printf(GENERIC_SHOW_ASSET_ERROR); close(socket_fd); return;}

        long file_size = stol(file_size_str);
    
        // Reading the image
        char image_buffer[IMAGE_BUFFER_SIZE];
        long bytes_read=0; 
        int n; 
        FILE* fd_image = fopen(file_name, "w");
        while (bytes_read < file_size) {
            memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
            n = (file_size-bytes_read < IMAGE_BUFFER_SIZE ? file_size-bytes_read : IMAGE_BUFFER_SIZE);
            n = read(socket_fd, image_buffer, n);
            if (n < 0)         {printf(GENERIC_SHOW_ASSET_ERROR); close(socket_fd); return;}
            n = fwrite(image_buffer, 1, n, fd_image);
            bytes_read += n;
        }

        // Reading the '\n' character at the end
        char read_char;
        n = read(socket_fd, &read_char, 1);
        if (n < 0)                      {printf(GENERIC_SHOW_ASSET_ERROR); close(socket_fd); return;}
        if (read_char != '\n')          {printf(GENERIC_SHOW_ASSET_ERROR); close(socket_fd); return;}

        fclose(fd_image);

        char dir[BUFFER_SIZE];
        if (getcwd(dir, BUFFER_SIZE) == NULL)         {printf(GENERIC_SHOW_ASSET_ERROR); close(socket_fd); return;};
        printf(SUCCESSFUL_SHOW_ASSET_USER, file_name, dir);

    }
    close(socket_fd);
}

void bid(char arguments[]) {
    char AID[MAX_DIGITS+1];
    long value;
    sscanf(arguments, "%*s %s %ld", AID, &value);

    char message[BUFFER_SIZE];
    sprintf(message, "BID %s %s %s %ld\n", user_ID.c_str(), user_password.c_str(), AID, value);

    string request_result = TCPclient(message, sizeof(message), NULL, NULL);
    if (request_result == "ERR")
        return;
    else {
        char response[COMMAND_WORD_SIZE+1];
        sscanf(request_result.c_str(), "%*s %s", response);
        if (!strcmp(response, "NOK")) printf(AUCTION_ALREADY_CLOSED_ERROR_USER, AID);
        else if (!strcmp(response, "NLG")) printf(USER_NOT_LOGGED_IN_ERROR_USER);
        else if (!strcmp(response, "ACC")) printf(SUCCESSFUL_BID_USER, value, AID);
        else if (!strcmp(response, "REF")) printf(LARGER_BID_ERROR_USER);
        else if (!strcmp(response, "ILG")) printf(BID_ON_HOSTED_AUCTION_ERROR_USER);
        else if (!strcmp(response, "ERR")) printf(GENERIC_BID_ERROR_USER);
    }
}



/*  +----------------------------------+
    |                                  |
    |          SOCKET WRITING          |
    |                                  |
    +----------------------------------+  */

string UDPclient(char message[], unsigned int message_size) {      // Returns -1 if error, 0 if successful but denied, 1 if successful
    int fd_UDP;
    struct addrinfo hints_UDP, *res_UDP;
    
    if ((fd_UDP = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        printf(SOCKET_CREATION_ERROR, "UDP");
        return "ERR";
    }
    
    memset(&hints_UDP, 0, sizeof hints_UDP);
    hints_UDP.ai_family = AF_INET;
    hints_UDP.ai_socktype = SOCK_DGRAM;

    if ((getaddrinfo(as_ip_address.c_str(), as_port.c_str(), &hints_UDP, &res_UDP)) != 0) {
        printf(SOCKET_CREATION_ERROR, "UDP");
        return "ERR";
    }

    ssize_t n;
    socklen_t addrlen;
    struct sockaddr_in addr;
    char buffer[UDP_BUFFER_SIZE];
    
    if ((n = sendto(fd_UDP, message, strlen(message), 0, res_UDP->ai_addr, res_UDP->ai_addrlen)) == -1) {
        printf(SOCKET_WRITING_ERROR, "UDP");
        return "ERR";
    }
    
    addrlen = sizeof(addr);
    if ((n = recvfrom(fd_UDP, buffer, UDP_BUFFER_SIZE, 0, (struct sockaddr*) &addr, &addrlen)) == -1) {
        printf(SOCKET_READING_ERROR, "UDP");
        return "ERR";
    }

    freeaddrinfo(res_UDP);
    close(fd_UDP);

    return buffer;
}


string TCPclient(const char message[], unsigned int message_size, int *image_fd, int *socket_fd) {
    int fd;
    ssize_t n;
    struct addrinfo hints, *res;
    char buffer[128];

    fd=socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)   {printf(SOCKET_CREATION_ERROR, "TCP");  return "ERR";}

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    n = getaddrinfo(as_ip_address.c_str(), as_port.c_str(), &hints, &res);
    if (n != 0)     {printf(SOCKET_CREATION_ERROR, "TCP");  return "ERR";}
    n = connect(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1)    {printf(SOCKET_CREATION_ERROR, "TCP");  return "ERR";}
    n = write(fd, message, strlen(message));
    if (n == -1)    {printf(SOCKET_CREATION_ERROR, "TCP");  return "ERR";}

    // Sending the Image
    if (image_fd != NULL) {
        long image_size, bytes_read=0;
        char image_buffer[IMAGE_BUFFER_SIZE];
        sscanf(message, "%*s %*s %*s %*s %*d %*d %*s %ld", &image_size);
        while (bytes_read < image_size) {
            memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
            n = read(*image_fd, image_buffer, IMAGE_BUFFER_SIZE);
            if (n<0)    {printf(IMAGE_FILE_DESCRIPTOR_ERROR);  return "ERR";}
            n = write(fd, image_buffer, n);
            if (n<0)    {printf(SOCKET_WRITING_ERROR, "TCP");  return "ERR";}
            bytes_read += n;
        }
        char new_line_char = '\n';
        n = write(fd, &new_line_char, 1);
    }

    // If was called by show_asset(), transfer conntrol to it
    if (socket_fd != NULL) {
        *socket_fd = fd;
        return "";
    }
    // Await the answer
    n = read(fd, buffer, 128);
    if (n == -1) {printf(SOCKET_READING_ERROR, "TCP");  return "ERR";}

    freeaddrinfo(res);
    close(fd);

    return buffer;
}