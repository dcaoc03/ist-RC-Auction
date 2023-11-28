#ifndef DATABASE_UTILS
#define DATABASE_UTILS

/* ---------------------- BINARY FUNCTIONS ---------------------- */
int is_user_logged_in(std::string user_ID);
int does_auction_exist(std::string AID);
int does_user_host_auction(std::string AID, std::string UID);
int is_auction_ongoing(std::string AID);

/* ---------------------- ACTION FUNCTIONS ---------------------- */
time_t get_auction_start_and_end_fulltime(std::string AID, char mode);
int create_auction_end_file(std::string AID);

#endif