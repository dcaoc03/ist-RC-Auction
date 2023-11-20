#ifndef AS
#define AS

#define PORT "58000"
#define BUFFER_SIZE 128
#define MAX_AUCTIONS 999
#define MAX_DIGITS 3

std::string login(char* arguments);
std::string logout(char* arguments);
std::string unregister(char* arguments);
std::string open(char* arguments);

#endif