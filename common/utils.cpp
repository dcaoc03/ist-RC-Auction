#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utils.h"

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
