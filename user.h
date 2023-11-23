#ifndef USER
#define USER

#define PORT "58104"
#define BUFFER_SIZE 128

#define MAX_JPG_SIZE 10000000
#define IMAGE_TRANSMISSION_SPEED 1

void login(char* arguments);
void logout();
void unregister();
void exit(int* ending);
void open(char* arguments);
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