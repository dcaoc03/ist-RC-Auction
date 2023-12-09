#ifndef DATABASE_UTILS
#define DATABASE_UTILS

/*  +--------------------------------------+
    |                                      |
    |         AS CLOSING FUNCTIONS         |
    |                                      |
    +--------------------------------------+  */

void unlink_semaphores();


/*  +----------------------------------+
    |                                  |
    |         BINARY FUNCTIONS         |
    |                                  |
    +----------------------------------+  */

DIR* does_user_exist(std::string user_id);
int is_user_logged_in(std::string user_ID);
int is_user_registered(std::string user_ID);
int user_password_check(char user_ID[], char password[]);
int does_auction_exist(std::string AID);
int does_user_host_auction(std::string AID, std::string UID);
int is_auction_ongoing(std::string AID);


/*  +----------------------------------+
    |                                  |
    |         ACTION FUNCTIONS         |
    |                                  |
    +----------------------------------+  */

/* ----------------- USER MANIPULATION FUNCTIONS ---------------- */

int create_user(std::string user_id, char password[],bool create_directories);
int logout_user(std::string user_id);
int unregister_user(std::string user_id);

/* --------------- AUCTION MANIPULATION FUNCTIONS -------------- */

int setup_auctions_dir();
int get_number_of_auctions();
time_t get_auction_start_and_end_fulltime(std::string AID, char mode);

/* --------------- AUCTION CREATION FUNCTIONS -------------- */

int copy_image(std::string AID, std::string file_name, long file_size, int socket_fd);
int create_auction(std::string AID, std::string UID, std::string asset_name, std::string file_name, long start_value, long timeactive);

int create_auction_end_file(std::string AID);

/* --------------- BID CREATION FUNCTIONS -------------- */

int get_highest_bid(std::string AID, long new_bid);
int create_bid_files(std::string UID, std::string AID, long value, std::string value_str);

/*  +----------------------------------+
    |                                  |
    |              GETTERS             |
    |                                  |
    +----------------------------------+  */

std::list <std::string> get_hosted_auctions_or_bids(std::string UID, char mode);
std::string get_auction_file_name(std::string AID);
std::string get_auction_info(std::string AID);


#endif