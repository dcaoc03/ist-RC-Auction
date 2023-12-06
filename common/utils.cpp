#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <string>

#include "utils.h"
#include "constants.h"

using namespace std;

int byte_reading(int fd, char word[], int word_len, bool can_be_smaller, bool is_last_word) {
    char read_char;
    int i;
    for (i=0; i < word_len+1; i++) {
        if (read(fd, &read_char, sizeof(read_char)) < 0) {
            printf("ERROR: failed to read from socket\n");
            return -1;
        }
        if (is_last_word && (i >= word_len) && (read_char != '\n')) {
            printf("ERROR: badly formatted message\n");
            return -1;
        }
        if ((read_char == ' ') || (read_char == '\n')) {
            if (i == 0)  {
                printf("ERROR: badly formatted message\n");
                return -1;
            }
            else if (!can_be_smaller && (i < word_len)) {
                printf("ERROR: argument %s too small\n", word);
                return -1;
            }
            else {
                break;
            }
        }
        else {
            if (i >= word_len) {
                printf("ERROR: argument %s too big\n", word);
                return -1;
            }
            else
                word[i] = read_char;
        }
    }
    word[i] = '\0';
    return 0;
}

int image_processing(char image_name[], string* message) {
    // Get image size
    FILE* jpg_pointer = fopen(image_name, "rb");
    int fd_jpg;
    if (jpg_pointer == NULL) {
        printf("ERROR: image file %s not found\n", image_name); 
        return -1;
    }
    fseek(jpg_pointer, 0, SEEK_END);
    long jpg_size = ftell(jpg_pointer);
    fseek(jpg_pointer, 0, SEEK_SET);
    if (jpg_size > MAX_JPG_SIZE) {
        printf("ERROR: file size exceeds 10 MB\n"); 
        return -1;
    }
    fclose(jpg_pointer);
    *message += " " + to_string(jpg_size) + " ";

    if((fd_jpg = open(image_name, S_IRUSR)) < 0) {
        printf("ERROR: failed to open jpg file\n");
        return -1;
    }
    return fd_jpg;
}
