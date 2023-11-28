#ifndef AS
#define AS

#define MAX_AUCTIONS 999
#define MAX_DIGITS 3

std::string login(char* arguments);
std::string logout(char* arguments);
std::string unregister(char* arguments);
std::string open_auction(int fd);
std::string close_auction(int fd);
std::string mybids(char* arguments);

#endif