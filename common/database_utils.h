#ifndef DATABASE_UTILS
#define DATABASE_UTILS

/* ---------------------- BINARY FUNCTIONS ---------------------- */
DIR* does_user_exist(std::string user_id);
int is_user_logged_in(std::string user_ID);
int is_user_registered(std::string user_ID);
int user_password_check(char user_ID[], char password[]);
int does_auction_exist(std::string AID);
int does_user_host_auction(std::string AID, std::string UID);
int is_auction_ongoing(std::string AID);

/* ---------------------- ACTION FUNCTIONS ---------------------- */
int create_user(std::string user_id, char password[],bool create_directories);
int logout_user(std::string user_id);
int unregister_user(std::string user_id);
time_t get_auction_start_and_end_fulltime(std::string AID, char mode);
int create_auction_dirs(std::string AID, std::string UID);
void copy_image(std::string AID, std::string file_name, int file_size, char* image_buffer, char* image);
void create_auction_start_file(std::string AID, std::string UID, std::string asset_name, std::string file_name, int start_value, int timeactive);
int create_auction_end_file(std::string AID);
std::list <std::string> get_hosted_auctions(std::string UID);
int get_highest_bid(std::string AID, int new_bid);
int create_bid_files(std::string UID, std::string AID, int value, std::string value_str);

#endif