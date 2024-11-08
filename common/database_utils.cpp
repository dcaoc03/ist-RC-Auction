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
#include <vector>

#include "database_utils.hpp"
#include "constants.h"

#define NUM_AUCTIONS_FILE_NAME "NUM_AUCTIONS.txt"
#define AUCTIONS_SEMAPHORE_NAME "/AUCTION_SEMAPHORE"
#define INDIVIDUAL_AUCTION_SEMAPHORE_NAME "/AUCTION_SEMPAHORE_"
#define INDIVIDUAL_USER_SEMAPHORE_NAME "/USER_SEMPAHORE_"

using namespace std;


/*  +------------------------------------------+
    |                                          |
    |         ERROR HANDLING FUNCTIONS         |
    |                                          |
    +------------------------------------------+  */

// remove_directory: Recursively deletes a directory and all it's contents
// (Used in case of errors while creating User or Auction directories)
// - dir_name: name of the directory to delete
void remove_directory(string dir_name) {
    DIR* dir = opendir(dir_name.c_str());
    struct dirent* entry;

    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                string file_path = dir_name + "/" + entry->d_name;
                if (entry->d_type == DT_DIR)
                    remove_directory(file_path);
                else
                    remove(file_path.c_str());
            }
        }
        closedir(dir);
        rmdir(dir_name.c_str());
    }
}

/*  +--------------------------------------+
    |                                      |
    |         AS CLOSING FUNCTIONS         |
    |                                      |
    +--------------------------------------+  */

/* unlink_semaphores: deletes all semaphores created during the execution of the AS
   (one for the AUCTIONS directory, one for each individual auction and one for each individual user)
*/
void unlink_semaphores() {
    DIR* auctions_dir = opendir("./AUCTIONS");
    struct dirent* entry;
    sem_unlink(AUCTIONS_SEMAPHORE_NAME);
    while ((entry = readdir(auctions_dir)) != NULL)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && strcmp(entry->d_name, NUM_AUCTIONS_FILE_NAME)) {
            string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + string(entry->d_name);
            sem_unlink(individual_auction_semaphore_name.c_str());
        }
    closedir(auctions_dir);
    
    DIR* users_dir = opendir("./USERS");
    while ((entry = readdir(users_dir)) != NULL)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + string(entry->d_name);
            sem_unlink(individual_user_semaphore_name.c_str());
        }
    closedir(users_dir);

}



/*  +----------------------------------+
    |                                  |
    |         BINARY FUNCTIONS         |
    |                                  |
    +----------------------------------+  */

/* does_user_exist: checks out if a user exists in the database
   - user_id: the ID of the given user
   Returns a pointer to the user's directory or NULL if it doesn't exist
*/
DIR* does_user_exist(string user_id) {
    string dir_name = "./USERS/" + user_id;
    DIR* dir = opendir(dir_name.c_str());
    return dir;
}

/* is_user_logged_in: checks out if a user is logged in in the system
   - user_id: the ID of the given user
   Returns 0 if user is logged in, -1 if not
*/
int is_user_logged_in(string user_ID) {
    DIR* dir = does_user_exist(user_ID);
    if (dir == NULL)
        return -1;
    closedir(dir);
    string login_file_name = "./USERS/" + user_ID + "/" + user_ID + "_login.txt";
    return access(login_file_name.c_str(), F_OK);
}

/* is_user_registered: checks out if a user is registered in the system
   - user_id: the ID of the given user
   Returns 0 if user is registered, -1 if not
*/
int is_user_registered(string user_ID) {
    string login_file_name = "./USERS/" + user_ID + "/" + user_ID + "_password.txt";
    return access(login_file_name.c_str(), F_OK);
}

/* user_password_check: given a user and a password, checks out if the password is the one the
   user is associated with in the database
   - user_id: the ID of the given user
   - password: the given password
   Returns 1 if the passwords match, 0 if not, -1 if an error occurs
*/
int user_password_check(char user_id[], char password[]) {
    string user_id_str = string(user_id);
    string password_file_name = "./USERS/" + user_id_str + "/" + user_id_str + "_password.txt";

    // Initialize the semaphore
    string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + string(user_id);
    sem_t *sem = sem_open(individual_user_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);

    FILE* fd_pass = fopen(password_file_name.c_str(), "r");

    char password_in_file[PASSWORD_SIZE+1];
    memset(password_in_file, 0, sizeof(password_in_file));

    if (fread(password_in_file, sizeof(char), PASSWORD_SIZE+1, fd_pass) < 0) {sem_post(sem); sem_close(sem); return -1;}
    fclose(fd_pass);

    if (!strcmp(password_in_file, password)) {
        string user_file_name = "./USERS/" + user_id_str + "/" + user_id_str + "_login.txt";
        FILE* fd_user = fopen(user_file_name.c_str(), "w");
        fclose(fd_user);
        sem_post(sem); sem_close(sem);
        return 1;
    }
    else 
        {sem_post(sem); sem_close(sem); return 0;}
}

/* does_auction_exist: checks out if an auction exists in the database
   - AID: the ID of the given auction
   Returns 1 if it exists, 0 if not;
*/
int does_auction_exist(string AID) {
    string auction_dir_name = "./AUCTIONS/" + AID;
    DIR* dir = opendir(auction_dir_name.c_str());

    if (dir) {closedir(dir);   return 1;}
    else    return 0;

}

/* does_user_host_auction: given a user and a password, checks if the user is the host of tha auction
   - AID: the ID of the given auction
   - UID: the ID of the given user
   Returns 1 if the user hosts the auction, 0 if not
*/
int does_user_host_auction(string AID, string UID) {
    string user_auction_file = "./USERS/" + UID + "/HOSTED/" + AID + ".txt";
    if (access(user_auction_file.c_str(), F_OK) == 0)
        return 1;
    else
        return 0;
}

/* is_auction_ongoing: checks out if a given auction is ongoing or not, creating an END file if necessary
   - AID: the ID of the given auction
   Returns 1 if is the auction ongoing, 0 otherwise, -1 if an error occured
*/
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
        if (create_auction_end_file(AID) == -1)     {sem_post(sem); sem_close(sem); return -1;}
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

/* create_user_dirs: creates the directories associated with the user, including the HOSTED and BIDDED directories
   - user_id: the ID of the given user
   Returns 0 if all are created successfully, -1 if not
*/
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

    // Initialize the individual semaphore
    string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + user_id;
    sem_t *sem = sem_open(individual_user_semaphore_name.c_str(), O_CREAT, 0666, 1);
    if (sem == SEM_FAILED)  return -1;
    sem_close(sem);

    return 0;
}

/* create_user: creates a user entry in the database, including the directories (if necessary),
   the login file and the password file
   - user_id: the ID of the user to log in
   - password: the user's password
   - create_directories: indicates if it s necessary to create the user's directories (=true in case
   the user is registrating)
   Returns 0 if done successfully, -1 if not
*/
int create_user(string user_id, char password[], bool create_directories) {
    if (create_directories) 
        if (create_user_dirs(user_id) == -1) {
            string user_dir_name = "USERS/" + user_id;
            remove_directory(user_dir_name.c_str());
            return -1;
        }
    
    string user_file_name = "./USERS/" + user_id + "/" + user_id + "_login.txt";
    string password_file_name = "./USERS/" + user_id + "/" + user_id + "_password.txt";
    // Initialize the semaphore
    string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + user_id;
    sem_t *sem = sem_open(individual_user_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);

    FILE* fd_user = fopen(user_file_name.c_str(), "w");
    FILE* fd_pass = fopen(password_file_name.c_str(), "w");

    if ((fd_user == NULL) || (fd_pass == NULL)) {sem_post(sem); sem_close(sem); return -1;}

    fprintf(fd_pass, "%s", password);

    fclose(fd_user);
    fclose(fd_pass);

    sem_post(sem); sem_close(sem);
    return 0;
}

/* logout_user: logs out a user, deleting the respective login file
   - user_id: the ID of the user to log out
   Returns 0 if log out is successful, 1 if user is not logged in, -1 if an error occured
*/
int logout_user(string user_id) {    
    string login_file_name = "./USERS/" + user_id + "/" + user_id + "_login.txt";
    // Initialize the semaphore
    string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + user_id;
    sem_t *sem = sem_open(individual_user_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);

    if (access(login_file_name.c_str(), F_OK) == -1) {sem_post(sem); sem_close(sem); return 1;}
    else {
        if (remove(login_file_name.c_str()) != 0) {sem_post(sem); sem_close(sem); return -1;}
        else {sem_post(sem); sem_close(sem); return 0;}
    }
}

/* unregister_user: unregisters a user, deleting the respective login and password files
   - user_id: the ID of the user to unregister
   Returns 0 if log out is successful, -1 if an error occured
*/
int unregister_user(string user_id) {
    string login_file_name = "./USERS/" + user_id + "/" + user_id + "_login.txt";
    string password_file_name = "./USERS/" + user_id + "/" + user_id + "_password.txt";
    // Initialize the semaphore
    string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + user_id;
    sem_t *sem = sem_open(individual_user_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);
    
    if ((remove(login_file_name.c_str()) != 0) || remove(password_file_name.c_str()) != 0) {sem_post(sem); sem_close(sem); return -1;}
    
    sem_post(sem); sem_close(sem); 
    return 0;
}


/* --------------- AUCTION MANIPULATION FUNCTIONS -------------- */

/* setup_auctions_dir: called upon when the AS starts up;
   Returns 0 if is successful -1 if not
*/
int setup_auctions_dir() {
    int count = 0;

    mkdir("AUCTIONS", S_IRWXU);
    mkdir("USERS", S_IRWXU);

    DIR* auctions_dir = opendir("./AUCTIONS");
    struct dirent* entry;

    if (auctions_dir == NULL)
        return -1;

    // Create semaphores for every auction in the database and counts the number of auctions in the database
    while ((entry = readdir(auctions_dir)) != NULL)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && strcmp(entry->d_name, NUM_AUCTIONS_FILE_NAME)) {
            count++;

            // Initialize the individual auction semaphores
            string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + string(entry->d_name);
            sem_t *sem = sem_open(individual_auction_semaphore_name.c_str(), O_CREAT, 0666, 1);
            if (sem == SEM_FAILED)  return -1;
            sem_close(sem);
        }
    closedir(auctions_dir);

    DIR* users_dir = opendir("./USERS");

    if (auctions_dir == NULL)
        return -1;

    // Create semaphores for every user in the database
    while ((entry = readdir(users_dir)) != NULL)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            // Initialize the individual user semaphores
            string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + string(entry->d_name);
            sem_t *sem = sem_open(individual_user_semaphore_name.c_str(), O_CREAT, 0666, 1);
            if (sem == SEM_FAILED)  return -1;
            sem_close(sem);
        }
    closedir(users_dir);

    // Writes the number of auctions in the database to AUCTIONS/NUM_AUCTIONS.txt
    string num_auctions_file_name = "./AUCTIONS/" + string(NUM_AUCTIONS_FILE_NAME);
    FILE* fd_start_file = fopen(num_auctions_file_name.c_str(), "w+");
    if (fprintf(fd_start_file, "%d", count) < 1) {
        fclose(fd_start_file);
        return -1;
    }
    fclose(fd_start_file);

    // Initialize the AUCTIONS directory semaphore
    sem_t *sem = sem_open(AUCTIONS_SEMAPHORE_NAME, O_CREAT | O_EXCL, 0666, 1);
    sem_close(sem);
    return 0;
}

/* get_number_of_auctions: reads the NUM_AUCTIONS.txt file to know how many auctions exist in the
   database; also increases it by one since the function is only called to know the ID of a new auction
   Returns the number of auctions in the database or -1 in case an error occurs
*/
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
    fclose(fd_start_file);
    sem_post(sem); sem_close(sem);
    return n_auctions;
}

/* get_auction_start_and_end_fulltime: reads the START file of an auction and gives either the auction's
   start time or the auction's predicted ending time (depends on the mode)
   - AID: the ID of the given auction
   - mode: determines the return value of the funcion -> can be 's' or 'e'
   Returns the auction's start time if mode=='s', or the auction's predicted ending time if mode=='e', or
   -1 in case an error occurs
*/
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

/* copy_image: reads a file from its file descriptor into a temporary location in the database
   - AID: the ID of the auction associated with the file
   - file_name: the name of the file
   - file_size: the size of the file (in bytes)
   - socket_fd: the socket file descriptor to read from
   Returns 0 if everything is successful, -1 in case an error occurs
*/
int copy_image(string AID, string file_name, long file_size, int socket_fd) {
    char image_buffer[IMAGE_BUFFER_SIZE];
    // Check if the temp directory already exists
    struct stat st;
    if (stat("./AUCTIONS/temp/", &st) != 0) 
        if (mkdir("./AUCTIONS/temp", S_IRWXU) == -1)
            return -1;
    
    // Establish the file's temporary location
    string image_name = "./AUCTIONS/temp/" + AID + "_" + file_name;
    FILE* fd_image = fopen(image_name.c_str(), "w");

    // Read the file from the socket
    long bytes_read = 0, n;
    while (bytes_read < file_size) {
        n = (file_size-bytes_read < IMAGE_BUFFER_SIZE ? file_size-bytes_read : IMAGE_BUFFER_SIZE);
        memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
        n = read(socket_fd, image_buffer, n);
        if (n<=0)    return -1;
        n = fwrite(image_buffer, 1, n, fd_image);
        if (n<=0)    return -1;
        bytes_read += n;
    }
    fclose(fd_image);
    return 0;
}

/* create_auction_dirs: creates the directories associated with an auction, including the BIDS and ASSET directories
  - AID: the ID of the give auction
  - UID: the ID of the usr who hosts the auction
  Returns 0 if everything is successful, -1 if not
*/
int create_auction_dirs(string AID, string UID) {
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

    // Initialize the individual semaphore
    string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + AID;
    sem_t *sem = sem_open(individual_auction_semaphore_name.c_str(), O_CREAT, 0666, 1);
    if (sem == SEM_FAILED)  return -1;
    sem_close(sem);

    return 0;
}

/* move_image: moves the file from its temporary location to the ASSET directory of the file's
  auction
  - file_name: the name of the file
  - AID: the ID of the auction associated with the file
  Returns 0 if is successful, -1 if not
*/
int move_image(string file_name, string AID) {
    string image_initial_name = "./AUCTIONS/temp/" + AID + "_" + file_name;
    string image_final_name = "./AUCTIONS/" + AID + "/ASSET/" + file_name;
    if (rename(image_initial_name.c_str(), image_final_name.c_str()) == -1)
        return -1;
    rmdir("./AUCTIONS/temp");
    return 0;
}

/* create_auction_start_file: creates a START file for a given auction
   - AID: the ID of the given auction
   - UID: the ID of the user who hosts the auction
   - asset_name: the title of the auction
   - file_name: the name of the file associated with the auction
   - start_value: the start value of the auction
   - timeacive: the time an auction is set to last
*/
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

    // Initialize the user semaphore
    string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + UID;
    sem_t *sem = sem_open(individual_user_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return;
    sem_wait(sem);
    // Create .txt on User directory
    string owner_dir_name = "./USERS/" + string(UID);

    string hosted_file = owner_dir_name + "/HOSTED/" + AID + ".txt";
    FILE* fd_hosted = fopen(hosted_file.c_str(), "w");
    fclose(fd_hosted);
    sem_post(sem); sem_close(sem);
    
}

/* create_auction_start_file: creates an entry in the database for the given auction
   - AID: the ID of the given auction
   - UID: the ID of the user who hosts the auction
   - asset_name: the title of the auction
   - file_name: the name of the file associated with the auction
   - start_value: the start value of the auction
   - timeacive: the time an auction is set to last
*/
int create_auction(string AID, string UID, string asset_name, string file_name, long start_value, long timeactive) {
    if (create_auction_dirs(AID,UID) == -1) {
        string auction_dir_name = "AUCTIONS/" + AID;
        remove_directory(auction_dir_name.c_str());
        return -1;
    }
    create_auction_start_file(AID, UID, asset_name, file_name, start_value, timeactive);
    if (move_image(file_name, AID) == -1) {
        string auction_dir_name = "AUCTIONS/" + AID;
        remove_directory(auction_dir_name.c_str());
        return -1;
    }
    return 0;
}

/* create_auction_end_file: crates the END file for a give auction
   - AID: the ID of the given auction
   Returns 0 if is succsessful, -1 if an error occurs
*/
int create_auction_end_file(string AID) {
    string auction_end_file = "./AUCTIONS/" + AID + "/END_" + AID + ".txt";
    FILE* fd_end_file = fopen(auction_end_file.c_str(), "w");

    time_t start_fulltime = get_auction_start_and_end_fulltime(AID, 's');
    time_t predicted_ending_time = get_auction_start_and_end_fulltime(AID, 'e');
    if (predicted_ending_time == -1) {
        remove(auction_end_file.c_str());
        return -1;
    }
    time_t current_fulltime = time(NULL);
    time_t end_fulltime = min(current_fulltime, predicted_ending_time);
    struct tm *end_date_time = localtime(&end_fulltime);

    fprintf(fd_end_file, "%04d-%02d-%02d %02d:%02d:%02d %ld", end_date_time->tm_year+1900, end_date_time->tm_mon + 1, end_date_time->tm_mday,
        end_date_time->tm_hour, end_date_time->tm_min, end_date_time->tm_sec, (end_fulltime-start_fulltime));

    fclose(fd_end_file);
    return 0;
}


/* --------------- BID CREATION FUNCTIONS -------------- */

/* get_highest_bid: given a new bid, tells if the bid is higher than the current biggest bid in that auction;
   in case it is, it writes the new highest bid in the MAX_BID file of the auction
   - AID: the auction that's being bidded
   - new_bid: the value of the new bid
   Returns 1 if the new bid is higher, 0 if not, -1 in case an error occurs
*/
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

/* create_bid_files: creates all files associated with a new bid
   - AID: the ID of the bidded auction
   - UID: the ID of the auction's host user
   - value: the value of the new bid
   - value_str: the value of the new bid converted to a string (for easier file naming)
   Returns 0 if everything is successful ,-1 in case an error occurs
*/
int create_bid_files(string UID, string AID, long value, string value_str) {
    // Initialize the user semaphore
    string individual_user_semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + UID;
    sem_t *sem = sem_open(individual_user_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);
    // Create the file in the user's "BIDDED" directory
    string user_bidded_file_name = "./USERS/" + UID + "/BIDDED/" + AID + ".txt";
    FILE* fd_user_bidded_file = fopen(user_bidded_file_name.c_str(), "w");
    fclose(fd_user_bidded_file);
    sem_post(sem); sem_close(sem);

    // Initialize the individual auction semaphore
    string individual_auction_semaphore_name = INDIVIDUAL_AUCTION_SEMAPHORE_NAME + AID;
    sem = sem_open(individual_auction_semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return -1;
    sem_wait(sem);
    // Create the file 
    string auction_bid_file_name = "./AUCTIONS/" + AID + "/BIDS/" + value_str + ".txt";
    FILE* fd_auction_bid_file = fopen(auction_bid_file_name.c_str(), "w");

    fprintf(fd_auction_bid_file, "%s %ld ", UID.c_str(), value);

    time_t bid_fulltime = time(NULL);
    struct tm *bid_date_time = localtime(&bid_fulltime);
    time_t start_fulltime = get_auction_start_and_end_fulltime(AID, 's');
    if (start_fulltime == -1)   {sem_post(sem); sem_close(sem); return -1;}
    fprintf(fd_auction_bid_file, "%04d-%02d-%02d %02d:%02d:%02d %ld", bid_date_time->tm_year+1900, bid_date_time->tm_mon + 1, bid_date_time->tm_mday,
        bid_date_time->tm_hour, bid_date_time->tm_min, bid_date_time->tm_sec, (bid_fulltime - start_fulltime));

    fclose(fd_auction_bid_file);
    sem_post(sem); sem_close(sem);

    return 0;
}



/*  +----------------------------------+
    |                                  |
    |              GETTERS             |
    |                                  |
    +----------------------------------+  */

/* get_hosted_auctions_or_bids: returns a list of auctions according to the given mode
   - UID: the ID of the given user (if mode==='a' or mode=='b')
   - mode: changes the return type of the function
   Returns according to the following modes:
      - mode == 'a' -> returns the user's hosted auctions
      - mode == 'b' -> returns the auctions where the user has placed a bid
      - mode == 'l' -> returns all auctions
*/
list <string> get_hosted_auctions_or_bids(string UID, char mode) {
    string desired_dir, semaphore_name;
    sem_t* sem;
    list <string> user_list;
    struct dirent* entry;

    if (mode == 'a')    {desired_dir = "./USERS/" + string(UID) + "/HOSTED"; semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + UID;}
    else if (mode == 'b')    {desired_dir = "./USERS/" + string(UID) + "/BIDDED"; semaphore_name = INDIVIDUAL_USER_SEMAPHORE_NAME + UID;}
    else if (mode == 'l')    {desired_dir = "./AUCTIONS/"; semaphore_name = AUCTIONS_SEMAPHORE_NAME;}
    else                      return user_list;
    
    // Initialize the semaphore
    sem = sem_open(semaphore_name.c_str(), O_RDWR);
    if (sem == SEM_FAILED)  return user_list;
    sem_wait(sem);

    DIR* dir = opendir(desired_dir.c_str());
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && strcmp(entry->d_name, NUM_AUCTIONS_FILE_NAME))
            user_list.push_back(string(entry->d_name).substr(0, strlen(entry->d_name) - 4));
    }

    closedir(dir);
    sem_post(sem); sem_close(sem);

    return user_list;
}

/* get_auction_file_name: gives the name of a file associated with a given auction
   - AID: the ID of the give auction
   Returns the name of the file
*/
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

/* get_auction_info: gets the information about an auction stored in its START file
   - AID: the ID of the given auction
   Returns a string containing all the requested information according to the show_record protocol
*/
string get_auction_info(string AID) {
    string start_file_name = "./AUCTIONS/" + AID + "/START_"+ AID +".txt";
    FILE* fd_start_file = fopen(start_file_name.c_str(), "r");

    char UID[UID_SIZE+1], asset_name[ASSET_NAME_SIZE+1], file_name[FILE_NAME_SIZE+1], start_value[START_VALUE_SIZE+1],
    duration[TIMEACTIVE_SIZE+1], start_date[20], start_hours[20];
    if (fscanf(fd_start_file, "%s %s %s %s %s %s %s", UID, asset_name, file_name, start_value, duration, start_date, start_hours) < 0)
        return "";
    
    return string(UID) + " " + asset_name + " " + file_name + " " + start_value + " " + start_date + " " + start_hours + " " + duration;
}

/* sort_by_most_recent_bid: criterion used for ordering the bids in descending value order
   - bid1, bid2: vectors containing information regarding two different bids
   Returns a boolean indicating if the value of bid1 is bigger than the value of bid2
*/
bool sort_by_most_recent_bid(vector <string> bid1, vector <string> bid2) {
    return stoi(bid1[4]) > stoi(bid2[4]);
}

/* get_bids: gets the 50 biggest bids made in an auction
   - AID: the ID of the given auction
   Returns a string containg all the request information regarding the bids according to the
   show_record protocol
*/
string get_bids(string AID) {
    string bids_dir_name = "./AUCTIONS/" + AID + "/BIDS";
    DIR* bids_dir = opendir(bids_dir_name.c_str());
    struct dirent* entry;
    list <vector <string>> bids_list;
    string bids = "";

    // Get all bids made in an auction
    while ((entry = readdir(bids_dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..") && strcmp(entry->d_name, "MAX_BID.txt")) {
            string bid_file_name = "./AUCTIONS/" + AID + "/BIDS/" + string(entry->d_name);
            FILE* fd_bid_file = fopen(bid_file_name.c_str(), "r");
            char UID[UID_SIZE+1], value[BID_VALUE_SIZE+1], bid_date[20], bid_hours[20];
            long bid_fulltime;
            if (fscanf(fd_bid_file, "%s %s %s %s %ld", UID, value, bid_date, bid_hours, &bid_fulltime) < 0)
                return "";  
            string s_active_time = to_string(bid_fulltime);
            string active_time = s_active_time;
            bids_list.push_back({UID, value, bid_date, bid_hours, active_time});
            fclose(fd_bid_file);
        }
    }

    // Sort the bids by descending order of value
    bids_list.sort(sort_by_most_recent_bid);
    closedir(bids_dir);

    // Select the 50 highest bids
    int count = 0;
    for (auto bid : bids_list) {
        bids += " B " + bid[0] + " " + bid[1] + " " + bid[2] + " " + bid[3] + " " + bid[4];
        count++;
        if (count == MAX_BIDS_TO_SEND)
            break;
    }
    return bids;
}

/* get_auction_end_info: gets the information about an auction stored in its END file
   - AID: the ID of the given auction
   Returns a string containing all the requested information according to the show_record protocol
*/
string get_auction_end_info(string AID) {
    if (is_auction_ongoing(AID) == 1)
        return "";
    string end_file_name = "./AUCTIONS/" + AID + "/END_"+ AID +".txt";
    FILE* fd_end_file = fopen(end_file_name.c_str(), "r");

    char end_date[20], end_hours[20], duration[TIMEACTIVE_SIZE+1];
    if (fscanf(fd_end_file, "%s %s %s", end_date, end_hours, duration) < 0)
        return "";
    
    return " E " + string(end_date) + " " + string(end_hours) + " " + string(duration);
}