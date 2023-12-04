#ifndef USER
#define USER

void login(char* arguments);
void logout();
void unregister();
void exit(int* ending);
void open_auction(char* arguments);
void close_auction(char* arguments);
void myauctions();
void mybids();
void list_auctions();
void show_asset();
void show_record();
void bid(char* arguments);

std::string UDPclient(char message[], unsigned int message_size);
std::string TCPclient(const char message[], unsigned int message_size, int *image_fd);

#endif