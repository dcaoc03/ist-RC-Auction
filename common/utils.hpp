#ifndef UTILS
#define UTILS

int byte_reading(char buffer[], int fd, char word[], int word_len, bool can_be_smaller, bool is_last_word);
int image_processing(char image_name[], std::string* message);
std::string get_file_from_path(std::string path);
void display_list(char buffer[], char mode, std::string UID);
void display_auction(char buffer[], std::string AID);
int check_formatation(char buffer[], const char* expected_code);

#endif