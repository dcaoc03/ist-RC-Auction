#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h> 

#include <string>
#include <list>

#include "database_utils.hpp"
#include "constants.h"

#define NUM_AUCTIONS_FILE_NAME "NUM_AUCTIONS.txt"
#define AUCTIONS_SEMAPHORE_NAME "/AUCTION_SEMAPHORE"
#define INDIVIDUAL_AUCTION_SEMAPHORE_NAME "/AUCTION_SEMPAHORE_"

using namespace std;

/* TODO!!!!!!!!!!!!!!!!!: Function whcih removes users, auctions and bids (directories) in case of errors */

/*  +--------------------------------------+
    |                                      |
    |         AS CLOSING FUNCTIONS         |
    |                                      |
    +--------------------------------------+  */

void unlink_semaphores() {
    DIR* auctions_dir = opendir("./AUCTIONS");
    struct dirent* entry;
    sem_unlink(AUCTIONS_SEMAPHORE_NAME);
    while ((entry = readdir(auctions_dir)) != NULL)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && strcmp(entry->d_name, NUM_AUCTIONS_FILE_NAME)) {
            string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + string(entry->d_name);
            sem_unlink(individual_auction_semaphore_name.c_str());
        }

}



/*  +----------------------------------+
    |                                  |
    |         BINARY FUNCTIONS         |
    |                                  |
    +----------------------------------+  */

DIR* does_user_exist(string user_id) {
    string dir_name = "./USERS/" + user_id;
    DIR* dir = opendir(dir_name.c_str());
    return dir;
}

int is_user_logged_in(string user_ID) {
    DIR* dir = does_user_exist(user_ID);
    if (dir == NULL)
        return -1;
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

    // Initialize the semaphore
    string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + AID;
    sem_t *sem = sem_open(individual_auction_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);

    if (access(auction_end_file.c_str(), F_OK) == 0) {
        sem_post(sem); sem_close(sem);
        return 0;
    }
    
    time_t predicted_ending_time = get_auction_start_and_end_fulltime(AID, 'e');
    if (predicted_ending_time == -1) {
        sem_post(sem); sem_close(sem);
        return -1;
    }
    time_t current_time = time(NULL);
    if (predicted_ending_time > current_time) {
        sem_post(sem); sem_close(sem);
        return 1;
    }
    else {
        if (create_auction_end_file(AID) == -1)     return -1;
        sem_post(sem); sem_close(sem);
        return 0;
    }
}



/*  +----------------------------------+
    |                                  |
    |         ACTION FUNCTIONS         |
    |                                  |
    +----------------------------------+  */

/* ----------------- USER MANIPULATION FUNCTIONS ---------------- */

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


/* --------------- AUCTION MANIPULATION FUNCTIONS -------------- */

int setup_auctions_dir() {int count = 0;
    DIR* auctions_dir = opendir("./AUCTIONS");
    struct dirent* entry;

    if (auctions_dir == NULL)
        return -1;

    while ((entry = readdir(auctions_dir)) != NULL)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && strcmp(entry->d_name, NUM_AUCTIONS_FILE_NAME)) {
            count++;

            // Initialize the individual semaphores
            string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + string(entry->d_name);
            sem_t *sem = sem_open(individual_auction_semaphore_name.c_str(), O_CREAT, 0666, 1);
            if (sem == SEM_FAILED)  return -1;
            sem_close(sem);
        }

    string num_auctions_file_name = "./AUCTIONS/" + string(NUM_AUCTIONS_FILE_NAME);
    FILE* fd_start_file = fopen(num_auctions_file_name.c_str(), "w+");
    if (fprintf(fd_start_file, "%d", count) < 1) {
        fclose(fd_start_file);
        return -1;
    }
    fclose(fd_start_file);

    // Initialize the semaphore
    sem_t *sem = sem_open(AUCTIONS_SEMAPHORE_NAME, O_CREAT | O_EXCL, 0666, 1);
    sem_close(sem);
    return 0;
}

int get_number_of_auctions() {
    string num_auctions_file_name = "./AUCTIONS/" + string(NUM_AUCTIONS_FILE_NAME);
    FILE* fd_start_file = fopen(num_auctions_file_name.c_str(), "r+");

    // Initialize the semaphore
    sem_t* sem = sem_open(AUCTIONS_SEMAPHORE_NAME, O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);

    int n_auctions;
    if (fscanf(fd_start_file, "%d", &n_auctions) < 1) {
        fclose(fd_start_file);
        sem_post(sem); sem_close(sem);
        return -1;
    }
    if (n_auctions == MAX_AUCTIONS) {
        fclose(fd_start_file);
        sem_post(sem); sem_close(sem);
        return n_auctions;
    }
    n_auctions++;
    fseek(fd_start_file, 0, SEEK_SET);
    if (fprintf(fd_start_file, "%d", n_auctions) < 1) {
        fclose(fd_start_file);
        sem_post(sem); sem_close(sem);
        return -1;
    }
    sem_post(sem); sem_close(sem);
    return n_auctions;
}

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


/* --------------- AUCTION CREATION FUNCTIONS -------------- */

int copy_image(string AID, string file_name, long file_size, int socket_fd) {
    char image_buffer[IMAGE_BUFFER_SIZE];
    if (mkdir("./AUCTIONS/temp", S_IRWXU) == -1)
        return -1;
    string image_name = "./AUCTIONS/temp/" + file_name;
    FILE* fd_image = fopen(image_name.c_str(), "w");
    long bytes_read = 0, n;
    while (bytes_read < file_size) {
        n = (file_size-bytes_read < IMAGE_BUFFER_SIZE ? file_size-bytes_read : IMAGE_BUFFER_SIZE);
        memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
        n = read(socket_fd, image_buffer, n);
        if (n<0)    return -1;
        n = fwrite(image_buffer, 1, n, fd_image);
        if (n<0)    return -1;
        bytes_read += n;
    }
    fclose(fd_image);
    return 0;
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
    string asset_dir = auctions_dir + "/ASSET";
    if (mkdir(auctions_dir.c_str(), S_IRWXU) == -1) {
        return -1;
    }
    if (mkdir(bids_dir.c_str(), S_IRWXU) == -1) {
        return -1;
    }
    if (mkdir(asset_dir.c_str(), S_IRWXU) == -1) {
        return -1;
    }

    // Initialize the semaphore
    string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + AID;
    sem_t *sem = sem_open(individual_auction_semaphore_name.c_str(), O_CREAT, 0666, 1);
    if (sem == SEM_FAILED)  return -1;
    sem_close(sem);

    return 0;
}

int move_image(string file_name, string AID) {
    string image_initial_name = "./AUCTIONS/temp/" + file_name;
    string image_final_name = "./AUCTIONS/" + AID + "/ASSET/" + file_name;
    if (rename(image_initial_name.c_str(), image_final_name.c_str()) == -1)
        return -1;
    if (rmdir("./AUCTIONS/temp") < 0)
        return -1;
    return 0;
}

void create_auction_start_file(string AID, string UID, string asset_name, string file_name, long start_value, long timeactive) {
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

    // Create file which holds highest bid
    string max_bid_file_name = "./AUCTIONS/" + AID + "/BIDS/MAX_BID.txt";
    FILE* fd_max_bid_file = fopen(max_bid_file_name.c_str(), "w");
    fprintf(fd_max_bid_file, "%ld", start_value);
    fclose(fd_max_bid_file);
}

int create_auction(string AID, string UID, string asset_name, string file_name, long start_value, long timeactive) {
    if (create_auction_dirs(AID,UID) == -1)
        return -1;
    create_auction_start_file(AID, UID, asset_name, file_name, start_value, timeactive);
    if (move_image(file_name, AID) == -1)
        return -1;
    return 0;
}

// Returns 0 if succsess, -1 in case of error
int create_auction_end_file(string AID) {
    string auction_end_file = "./AUCTIONS/" + AID + "/END_" + AID + ".txt";
    FILE* fd_end_file = fopen(auction_end_file.c_str(), "w");

    time_t start_fulltime = get_auction_start_and_end_fulltime(AID, 's');
    time_t predicted_ending_time = get_auction_start_and_end_fulltime(AID, 'e');
    if (predicted_ending_time == -1)
        return -1;
    time_t current_fulltime = time(NULL);
    time_t end_fulltime = min(current_fulltime, predicted_ending_time);
    struct tm *end_date_time = localtime(&end_fulltime);

    fprintf(fd_end_file, "%04d-%02d-%02d %02d:%02d:%02d %ld", end_date_time->tm_year+1900, end_date_time->tm_mon + 1, end_date_time->tm_mday,
        end_date_time->tm_hour, end_date_time->tm_min, end_date_time->tm_sec, (end_fulltime-start_fulltime));

    fclose(fd_end_file);
    return 0;
}


/* --------------- BID CREATION FUNCTIONS -------------- */

// Returns 0 if new bid is higher, 0 if not, -1 in case of error
int get_highest_bid(string AID, long new_bid) {
    long highest_bid;
    string max_bid_file_name = "./AUCTIONS/" + AID + "/BIDS/MAX_BID.txt";

    // Initialize the semaphore
    string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + AID;
    sem_t *sem = sem_open(individual_auction_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);

    FILE* fd_max_bid_file = fopen(max_bid_file_name.c_str(), "r+");
    if (fscanf(fd_max_bid_file, "%ld", &highest_bid) < 1) {
        fclose(fd_max_bid_file);
        sem_post(sem); sem_close(sem);
        return -1;
    }
    fclose(fd_max_bid_file);
    if (new_bid <= highest_bid) {
        sem_post(sem); sem_close(sem);
        return 0;
    }
    else {
        FILE* fd_max_bid_file = fopen(max_bid_file_name.c_str(), "w");
        fprintf(fd_max_bid_file, "%ld", new_bid);
        fclose(fd_max_bid_file);
        sem_post(sem); sem_close(sem);
        return 1;
    }

}

// Returns 0 if succsess, -1 in case of error
int create_bid_files(std::string UID, std::string AID, long value, string value_str) {
    // Create the file in the user's "BIDDED" directory
    string user_bidded_file_name = "./USERS/" + UID + "/BIDDED/" + AID + ".txt";
    FILE* fd_user_bidded_file = fopen(user_bidded_file_name.c_str(), "w");
    fclose(fd_user_bidded_file);

    // Create the file 
    string auction_bid_file_name = "./AUCTIONS/" + AID + "/BIDS/" + value_str + ".txt";
    FILE* fd_auction_bid_file = fopen(auction_bid_file_name.c_str(), "w");

    fprintf(fd_auction_bid_file, "%s %ld ", UID.c_str(), value);

    time_t bid_fulltime = time(NULL);
    struct tm *bid_date_time = localtime(&bid_fulltime);
    time_t start_fulltime = get_auction_start_and_end_fulltime(AID, 's');
    if (start_fulltime == -1)
        return -1;

    fprintf(fd_auction_bid_file, "%04d-%02d-%02d %02d:%02d:%02d %ld", bid_date_time->tm_year+1900, bid_date_time->tm_mon + 1, bid_date_time->tm_mday,
        bid_date_time->tm_hour, bid_date_time->tm_min, bid_date_time->tm_sec, (bid_fulltime - start_fulltime));
    fclose(fd_auction_bid_file);
    return 0;
}



/*  +----------------------------------+
    |                                  |
    |              GETTERS             |
    |                                  |
    +----------------------------------+  */

// mode == 'a' -> returns the user's hosted auctions
// mode == 'b' -> returns the auctions where the user has put a bid
// mode == 'l' -> returns all auctions
list <string> get_hosted_auctions_or_bids(string UID, char mode) {
    string desired_dir;
    if (mode == 'a')    desired_dir = "./USERS/" + string(UID) + "/HOSTED";
    else if (mode == 'b')    desired_dir = "./USERS/" + string(UID) + "/BIDDED";
    else if (mode == 'l')    desired_dir = "./AUCTIONS/";
    DIR* dir = opendir(desired_dir.c_str());
    list <string> user_list;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && strcmp(entry->d_name, NUM_AUCTIONS_FILE_NAME))
            user_list.push_back(string(entry->d_name).substr(0, strlen(entry->d_name) - 4));
    }
    closedir(dir);
    return user_list;
}

string get_auction_file_name(string AID) {
    string file_name;
    if (!does_auction_exist(AID))
        return "";

    string auction_asset_dir_name = "./AUCTIONS/" + AID + "/ASSET";
    DIR* auction_asset_dir = opendir(auction_asset_dir_name.c_str());
    struct dirent* entry;
    while ((entry = readdir(auction_asset_dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")){
            file_name = entry->d_name;
            break;
        }
    }
    closedir(auction_asset_dir);
    return file_name;
}

string get_auction_info(string AID) {
    string start_file_name = "./AUCTIONS/" + AID + "/START_"+ AID +".txt";
    FILE* fd_start_file = fopen(start_file_name.c_str(), "r");

    char UID[UID_SIZE+1], asset_name[ASSET_NAME_SIZE], file_name[FILE_NAME_SIZE], start_value[START_VALUE_SIZE],
    start_date[20], start_hours[20];
    long start_fulltime;
    if (fscanf(fd_start_file, "%s %s %s %s %*s %s %s %ld", UID, asset_name, file_name, start_value, start_date, start_hours, &start_fulltime) < 0)
        return "";
    
    time_t current_time = time(NULL);
    string s_active_time = to_string(current_time-start_fulltime);
    string active_time = string(TIMEACTIVE_SIZE - s_active_time.length(), '0') + s_active_time;
    
    return string(UID) + " " + asset_name + " " + file_name + " " + start_value + " " + start_date + " " + start_hours + " " + active_time;
}