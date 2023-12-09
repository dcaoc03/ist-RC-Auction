#ifndef AS
#define AS

// UDP Commands

std::string login(char* arguments);
std::string logout(char* arguments);
std::string unregister(char* arguments);
std::string myauctions_or_mybids(char* arguments, char mode);
std::string list_auctions(char* arguments);
std::string show_record(char* arguments);


// TCP Commands

std::string open_auction(int fd);
std::string close_auction(int fd);
std::string show_asset(int fd, int *asset_fd);
std::string bid(int fd);

#endif