#ifndef UTILS
#define UTILS

int byte_reading(int fd, char word[], int word_len, bool can_be_smaller, bool is_last_word);
int image_processing(char image_name[], std::string* message);

#endif