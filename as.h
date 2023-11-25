#ifndef AS
#define AS

#define MAX_AUCTIONS 999
#define MAX_DIGITS 3

std::string login(char* arguments);
std::string logout(char* arguments);
std::string unregister(char* arguments);
std::string open_auction(int fd);

int byte_reading(int fd, char word[], int word_len, bool can_be_smaller);

#endif