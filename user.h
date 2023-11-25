#ifndef USER
#define USER

void login(char* arguments);
void logout();
void unregister();
void exit(int* ending);
void open_auction(char* arguments);
void close();
void myauctions();
void mybids();
void list();
void show_asset();
void bid();
void show_record();

std::string UDPclient(char message[], unsigned int message_size);
std::string TCPclient(const char message[], unsigned int message_size, int image_fd);

#endif