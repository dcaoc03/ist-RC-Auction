#ifndef USER
#define USER

#define PORT "58000"
#define BUFFER_SIZE 128

void login(char* arguments);
void logout();
void unregister();
void exit(int* ending);
void open();
void close();
void myauctions();
void mybids();
void list();
void show_asset();
void bid();
void show_record();

std::string UDPclient(char message[], unsigned int message_size);
int TCPclient();

#endif