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

/* get_file_from_path: recursive function that, given a path, returns the file at the end of it
   - path: the path to the file
   Returns the name of the file at the end of the path
*/
string get_file_from_path(string path) {
    size_t pos = path.find('/');

    if (pos != string::npos)
        // Found '/, it's still a directory
        return get_file_from_path(path.substr(pos + 1));

    // '/' not found, it's the file name
    return path;
}

/* display_list: receives a C string and, knowing the structure of the (AID, status) structure, 
   displays all of them in a readable way */
void display_list(char buffer[], char mode, string UID) {
    int offset = 0;

    char AID[MAX_DIGITS+1];
    int status;

    printf("\n");
    if (mode == 'l')        printf("List of all auctions:\n");
    else if (mode == 'a')   printf("All auctions hosted by user %s:\n", UID.c_str());
    else if (mode == 'b')   printf("All auctions where user %s placed bids:\n", UID.c_str());

    while (sscanf(buffer+offset, "%s %d", AID, &status) >= 0) {
        printf("- %s: ", AID);
        if (status)     printf("ACTIVE\n");
        else            printf("INACTIVE\n");
        offset += 6;
    }
    printf("\n");
}

void display_auction(char buffer[], string AID) {
    char UID[UID_SIZE+1], auction_name[ASSET_NAME_SIZE+1], file_name[FILE_NAME_SIZE+1], start_value[START_VALUE_SIZE+1],
       start_date[20], start_time[20], timeactive[TIMEACTIVE_SIZE+1];
    
    if (sscanf(buffer, "%s %s %s %s %s %s %s", UID, auction_name, file_name, start_value, start_date, start_time, timeactive) < 0) {
        printf("ERROR: couldn't display the auction\n");
        return;
    }
    else {
        printf("\n");
        printf("AUCTION %s:\n", AID.c_str());
        printf("- Name: %s\n", auction_name);
        printf("- Host: %s\n", UID);
        printf("- Asset file name: %s\n", file_name);
        printf("- Started: %s %s\n", start_date, start_time);
        printf("- Initial value: %s\n", start_value);
        printf("- Predicted duration: %s seconds\n", timeactive);
    }

    char* new_buffer = buffer + strlen(UID) +  strlen(auction_name) +  strlen(file_name) + strlen(start_value) +
       strlen(start_date) +  strlen(start_time) + strlen(timeactive) + 7;

    char next_info;
    if (sscanf(new_buffer, "%c", &next_info) < 0)
        return;

    if (next_info == 'B') {
        printf("\n");
        printf("BIDS MADE ON AUCTION %s:\n", AID.c_str());
    }
    while (next_info == 'B') {
        char bidder_UID[UID_SIZE+1], bid_value[START_VALUE_SIZE+1], bid_date[20], bid_time[20], sec_active[TIMEACTIVE_SIZE+1];
        sscanf(new_buffer, "%*c %s %s %s %s %s", bidder_UID, bid_value, bid_date, bid_time, sec_active);
        printf("- %s\n", bid_value);
        printf("   - made by user %s on %s %s (%s seconds after start)\n", bidder_UID, bid_date, bid_time, sec_active);

        new_buffer += strlen(bidder_UID) + strlen(bid_value) + strlen(bid_date) + strlen(bid_time) + strlen(sec_active) + 7;
        if (sscanf(new_buffer, "%c", &next_info) < 0)
            break;
    }
    if (next_info == 'E') {
        printf("\n");
        char end_date[20], end_time[20], end_sec[TIMEACTIVE_SIZE+1];
        if (sscanf(new_buffer, "%*c %s %s %s", end_date, end_time, end_sec) < 0)
            return;
        printf("AUCTION %s ENDED ON %s %s (%s seconds after start)\n", AID.c_str(), end_date, end_time, end_sec);
    }
    printf("\n");
}

int check_formatation(char buffer[], const char* expected_code) {
    char code[COMMAND_WORD_SIZE+1];
    if (byte_reading(buffer, -1, code, COMMAND_WORD_SIZE, false, false) < 0) {
        printf("ERROR: got a response with unexpected formatation\n");
        return -1;
    }
    else if (strcmp(code, expected_code)) {
        printf("ERROR: expected %s message, but got %s instead\n", code, expected_code);
        return -1;
    }
    return 0;
}