#ifndef USER
#define USER

// Flux Control Functions

void exit(int* ending);


// UDP Commands

void login(char* arguments);
void logout();
void unregister();
void myauctions();
void mybids();
void list_auctions();
void show_record(char* arguments);


// TCP Commands

void open_auction(char* arguments);
void close_auction(char* arguments);
void show_asset(char* arguments);
void bid(char* arguments);


// Socket Writing Functions

std::string UDPclient(char message[], unsigned int message_size);
std::string TCPclient(const char message[], unsigned int message_size, int *image_fd, int *socket_fd);

#endif