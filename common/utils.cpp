#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <string>

#include "utils.hpp"
#include "constants.h"

using namespace std;

/* byte_reading: reads a buffer or a file descriptor one byte at a time, detecting formatation errors as well
   - buffer: the buffer to read from (== NULL if file_descriptor is to be used)
   - fd: the file descriptor to read from (== -1 if buffer is to be used)
   - word: the char pointer to read the word to
   - word_len: the maximum size the read word can have
   - can_be_smaller: indicates if the word to be read doesn't need to have its maximum size
   - is_last_word: indicates if it's the last word in a message (if it needs to read '\n')
   Returns the number of bytes read
*/
int byte_reading(char buffer[], int fd, char word[], int word_len, bool can_be_smaller, bool is_last_word) {
    char read_char;
    int i;
    for (i=0; i < word_len+1; i++) {
        if (fd != -1) {
            if (read(fd, &read_char, sizeof(read_char)) < 0) {
                printf("ERROR: failed to read from socket\n");
                return -1;
            }
        } else
            read_char = buffer[i];

        // Checks if it's the last word and if it has exceeded supposed size
        if (is_last_word && (i >= word_len) && (read_char != '\n')) {
            printf("ERROR: badly formatted message\n");
            return -1;
        }

        // In case it reads an empty space or '\n' -> word reading is done
        if ((read_char == ' ') || (read_char == '\n')) {
            if (i == 0)  {                                          // checks if it read two spaces in a row
                printf("ERROR: badly formatted message\n");
                return -1;
            }
            else if (!can_be_smaller && (i < word_len)) {           // checks if the word can be smaller than its supposed size
                printf("ERROR: argument %s too small\n", word);
                return -1;
            }
            else                                                    // if meets all criteria, word is fully read
                break;
        }
        // Word continues on
        else {
            if (i >= word_len) {                                    // checks if the word has exceeded its supposed size
                printf("ERROR: argument %s too big\n", word);
                return -1;
            }
            else                                                    // if it doesn't, adds up to the word
                word[i] = read_char;
        }
    }
    word[i] = '\0';                                                 // close the string
    return i+1;
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
