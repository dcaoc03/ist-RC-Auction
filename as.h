#ifndef AS
#define AS

#define MAX_AUCTIONS 999

std::string login(char* arguments);
std::string logout(char* arguments);
std::string unregister(char* arguments);
std::string open_auction(int fd);
std::string close_auction(int fd);

#endif