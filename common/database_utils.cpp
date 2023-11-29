#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>

#include <string>

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
    time_t predicted_ending_time = get_auction_start_and_end_fulltime(AID, 'e');
    if (predicted_ending_time == -1)
        return -1;
    time_t current_time = time(NULL);
    if (predicted_ending_time > current_time)
        return 1;
    else {
        string auction_end_file = "./AUCTIONS/" + AID + "/END_" + AID + ".txt";
        if (access(auction_end_file.c_str(), F_OK) == 0)
            return 0;
        else {
            create_auction_end_file(AID);
            return 0;
        }

    }
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