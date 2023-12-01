#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>

#include <string>
#include <list>

#include "database_utils.h"
#include "constants.h"

using namespace std;

/* ---------------------- BINARY FUNCTIONS ---------------------- */

DIR* does_user_exist(string user_id) {
    string dir_name = "./USERS/" + user_id;
    DIR* dir = opendir(dir_name.c_str());
    return dir;
}

int is_user_logged_in(string user_ID) {
    string login_file_name = "./USERS/" + user_ID + "/" + user_ID + "_login.txt";
    return access(login_file_name.c_str(), F_OK);
}

int is_user_registered(string user_ID) {
    string login_file_name = "./USERS/" + user_ID + "/" + user_ID + "_password.txt";
    return access(login_file_name.c_str(), F_OK);
}

// Returns 1 if accepted, 0 if not, -1 if error
int user_password_check(char user_id[], char password[]) {
    string user_id_str = string(user_id);
    string password_file_name = "./USERS/" + user_id_str + "/" + user_id_str + "_password.txt";   
    FILE* fd_pass = fopen(password_file_name.c_str(), "r");

    char password_in_file[PASSWORD_SIZE+1];
    memset(password_in_file, 0, sizeof(password_in_file));

    if (fread(password_in_file, sizeof(char), PASSWORD_SIZE+1, fd_pass) < 0) {
        return -1;
    }
    fclose(fd_pass);

    if (!strcmp(password_in_file, password)) {
        string user_file_name = "./USERS/" + user_id_str + "/" + user_id_str + "_login.txt";
        FILE* fd_user = fopen(user_file_name.c_str(), "w");
        fclose(fd_user);
        return 1;
    }
    else 
        return 0;
}

int does_auction_exist(string AID) {
    string auction_dir_name = "./AUCTIONS/" + AID;
    DIR* dir = opendir(auction_dir_name.c_str());

    if (dir) {closedir(dir);   return 1;}
    else    return 0;

}

int does_user_host_auction(string AID, string UID) {
    string user_auction_file = "./USERS/" + UID + "/HOSTED/" + AID + ".txt";
    if (access(user_auction_file.c_str(), F_OK) == 0)
        return 1;
    else
        return 0;
}

// Returns 1 if is ongoing, 0 otherwise, -1 if error
int is_auction_ongoing(string AID) {
    string auction_end_file = "./AUCTIONS/" + AID + "/END_" + AID + ".txt";
    if (access(auction_end_file.c_str(), F_OK) == 0)
        return 0;
    
    time_t predicted_ending_time = get_auction_start_and_end_fulltime(AID, 'e');
    if (predicted_ending_time == -1)
        return -1;
    time_t current_time = time(NULL);
    if (predicted_ending_time > current_time)
        return 1;
    else 
        return 0;
}

/* ---------------------- ACTION FUNCTIONS ---------------------- */

// USER MANIPULATION FUNCTIONS
int create_user_dirs(string user_id) {
    string dir_name = "./USERS/" + user_id;
    string hosted = dir_name + "/HOSTED";
    string bidded = dir_name + "/BIDDED";

    if(mkdir(dir_name.c_str(), S_IRWXU) == -1) 
        return -1;

    if(mkdir(hosted.c_str(), S_IRWXU) == -1)
        return -1;
        
    if(mkdir(bidded.c_str(), S_IRWXU) == -1)
        return -1;
    return 0;
}

int create_user(string user_id, char password[], bool create_directories) {
    if (create_directories) 
        if (create_user_dirs(user_id) == -1) 
            return -1;
    
    string user_file_name = "./USERS/" + user_id + "/" + user_id + "_login.txt";
    string password_file_name = "./USERS/" + user_id + "/" + user_id + "_password.txt";        

    FILE* fd_user = fopen(user_file_name.c_str(), "w");
    FILE* fd_pass = fopen(password_file_name.c_str(), "w");

    if ((fd_user == NULL) || (fd_pass == NULL))
        return -1;

    fprintf(fd_pass, "%s", password);

    fclose(fd_user);
    fclose(fd_pass);

    return 0;
}

// Returns 0 if successful, 1 if unsuccessful, -1 if error
int logout_user(string user_id) {    
    string login_file_name = "./USERS/" + user_id + "/" + user_id + "_login.txt";
    if (access(login_file_name.c_str(), F_OK) == -1) 
        return 1;
    else {
        if (remove(login_file_name.c_str()) != 0)
            return -1;
        else
            return 0;
    }
}

int unregister_user(string user_id) {
    string login_file_name = "./USERS/" + user_id + "/" + user_id + "_login.txt";
    string password_file_name = "./USERS/" + user_id + "/" + user_id + "_password.txt";
    
    if ((remove(login_file_name.c_str()) != 0) || remove(password_file_name.c_str()) != 0) 
        return -1;
    
    return 0;
}

// AUCTION MANIPULATION FUNCTIONS

// Mode 's' to get start_fulltime and 'e' to get the predicted_ending_time, returns -1 in case of fscanf error
time_t get_auction_start_and_end_fulltime(string AID, char mode) {
    string auction_start_file = "./AUCTIONS/" + AID + "/START_" + AID + ".txt";
    FILE* fd_start_file = fopen(auction_start_file.c_str(), "r");
    time_t start_fulltime;
    int duration;

    if (fscanf(fd_start_file, "%*s %*s %*s %*d %d %*s %*s %ld", &duration, &start_fulltime) < 2)
        return -1;
    fclose(fd_start_file);

    if (mode == 's')
        return start_fulltime;
    else
        return start_fulltime+duration;
}

int create_auction_dirs(string AID, string UID) {
    // Create .txt on User directory
    string owner_dir_name = "./USERS/" + string(UID);

    string hosted_file = owner_dir_name + "/HOSTED/" + AID + ".txt";
    FILE* fd_hosted = fopen(hosted_file.c_str(), "w");
    fclose(fd_hosted);

    // Create directory in AUCTIONS
    string auctions_dir = "./AUCTIONS/" + AID;
    string bids_dir = auctions_dir + "/BIDS";
    if (mkdir(auctions_dir.c_str(), S_IRWXU) == -1) {
        return -1;
    }
    if (mkdir(bids_dir.c_str(), S_IRWXU) == -1) {
        return -1;
    }
    return 0;
}

void copy_image(string AID, string file_name, int file_size, char* image_buffer, char* image) {
    string image_name = "./AUCTIONS/" + AID + "/" + file_name;
    FILE* fd_image = fopen(image_name.c_str(), "w");
    int bytes_read = 0, n;
    while (bytes_read < file_size) {
        n = (file_size-bytes_read < IMAGE_BUFFER_SIZE ? file_size-bytes_read : IMAGE_BUFFER_SIZE);
        memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
        memcpy(image_buffer, image+bytes_read, n);
        n = fwrite(image_buffer, 1, n, fd_image);
        bytes_read += n;
    }
    fclose(fd_image);
}

void create_auction_start_file(string AID, string UID, string asset_name, string file_name, int start_value, int timeactive) {
    string start_file_name = "./AUCTIONS/" + AID + "/START_"+ AID +".txt";
    FILE* fd_start_file = fopen(start_file_name.c_str(), "w");
    string start_file_content = string(UID) + " " + asset_name + " " + file_name + " " + to_string(start_value) + " " +
        to_string(timeactive) + " ";
    fprintf(fd_start_file, "%s", start_file_content.c_str());
    time_t start_fulltime = time(NULL);
    struct tm *start_date_time = localtime(&start_fulltime);
    fprintf(fd_start_file, "%04d-%02d-%02d %02d:%02d:%02d %ld", start_date_time->tm_year+1900, start_date_time->tm_mon + 1, start_date_time->tm_mday,
        start_date_time->tm_hour, start_date_time->tm_min, start_date_time->tm_sec, start_fulltime);

    fclose(fd_start_file);
}

// Returns 0 if succsess, -1 in case of error
int create_auction_end_file(string AID) {
    string auction_end_file = "./AUCTIONS/" + AID + "/END_" + AID + ".txt";
    FILE* fd_end_file = fopen(auction_end_file.c_str(), "w");

    time_t start_fulltime = get_auction_start_and_end_fulltime(AID, 's');
    if (start_fulltime == -1)
        return -1;
    time_t end_fulltime = time(NULL);
    struct tm *end_date_time = localtime(&end_fulltime);

    fprintf(fd_end_file, "%04d-%02d-%02d %02d:%02d:%02d %ld", end_date_time->tm_year+1900, end_date_time->tm_mon + 1, end_date_time->tm_mday,
        end_date_time->tm_hour, end_date_time->tm_min, end_date_time->tm_sec, (end_fulltime - start_fulltime));

    fclose(fd_end_file);
    return 0;
}

list <string> get_hosted_auctions(string UID) {
    string hosted_dir = "./USERS/" + string(UID) + "/HOSTED";
    DIR* hosted = opendir(hosted_dir.c_str());
    list <string> hosted_list;
    struct dirent* entry;
    while ((entry = readdir(hosted)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
            hosted_list.push_back(string(entry->d_name).substr(0, strlen(entry->d_name) - 4));
    }
    closedir(hosted);
    return hosted_list;
}