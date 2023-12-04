#ifndef USER_STDOUT_MESSAGES
#define USER_STDOUT_MESSAGES

// Main Function

#define LOGIN_FIRST "Please login first\n"
#define ALREADY_LOGGED_IN "User already logged in\n"
#define SOCKET_CREATION_ERROR "SOCKET ERROR: failed to initialize %s socket\n"
#define SOCKET_READING_ERROR "SOCKET ERROR: failed to read from %s socket\n"
#define SOCKET_WRITING_ERROR "SOCKET ERROR: failed to write to %s socket\n"

// Login Function
#define SUCCESSFUL_LOGIN_USER "User successfully logged in\n"
#define UNSUCCESSFUL_LOGIN_USER "User failed to log in\n"
#define SUCCESSFUL_RESGISTRATION_USER "New user successfully registered\n"
#define GENERIC_LOGIN_ERROR_USER "ERROR: something wrong happened while logging in\n"

#define SUCCESSFUL_LOGOUT_USER "User successfully logged out\n"
#define UNSUCCESSFUL_LOGOUT_USER "User failed to log in\n"
#define USER_NOT_REGISTERED_ERROR_USER "User not registered in the database\n"
#define GENERIC_LOGOUT_ERROR_USER "ERROR: something wrong happened while logging out\n"

// Unregister Function
#define SUCCESSFUL_UNREGISTRATION_USER "User successfully unregistered\n"
#define UNSUCCESSFUL_UNREGISTRATION_USER "User failed to unregister\n"
#define GENERIC_UNREGISTRATION_ERROR_USER "ERROR: something wrong happened while unregistering the user\n"

// Exit Function
#define EXIT_BEFORE_LOGOUT_ERROR "Please, execute the logout command before exiting\n"

// Open Auction Function
#define ARGUMENTS_SIZE_ERROR "ERROR: attribute %s exceeds %d alphanumerical characters\n"
#define IMAGE_PROCESSING_ERROR "ERROR: failed to open jpg\n"
#define IMAGE_FILE_DESCRIPTOR_ERROR "ERROR: failed to read from the designated file descriptor\n"

#define SUCCESSFUL_AUCTION_OPENING_USER "Auction %s successfully started\n"
#define UNSUCCESSFUL_AUCTION_OPENING_USER "Auction failed to start\n"
#define USER_NOT_LOGGED_IN_ERROR_USER "User is not logged in\n"
#define GENERIC_AUCTION_OPENING_ERROR_USER "ERROR: something wrong happened while creating an auction\n"

// Close Auction Function
#define SUCCESSFUL_AUCTION_CLOSING_USER "Auction %s successfully closed\n"
#define UNALLOWED_AUCTION_CLOSING_USER "Auction %s is not owned by user %s\n"
#define AUCTION_ALREADY_CLOSED_ERROR_USER "Auction %s has already ended\n"
#define GENERIC_AUCTION_CLOSING_ERROR_USER "ERROR: something wrong happened while ending the auction\n"

// My Auctions Function
#define NO_ONGOING_AUCTIONS_ERROR_USER "User has no ongoing auctions\n"
#define GENERIC_MY_AUCTIONS_ERROR_USER "ERROR: something wrong happened while listing the auctions\n"

// My Bids FUnction
#define NO_ONGOING_BIDS_ERROR_USER "User has no ongoing bids\n"
#define GENERIC_MY_BIDS_ERROR_USER "ERROR: something wrong happened while listing the bids\n"

// Bid Function
#define SUCCESSFUL_BID_USER "Successfully made a bid of value %ld in auction %s\n"
#define LARGER_BID_ERROR_USER "A larger bid has already been made\n"
#define BID_ON_HOSTED_AUCTION_ERROR_USER "Users can't bid in their own auctions\n"
#define GENERIC_BID_ERROR_USER "ERROR: something wrong happened while making a bid\n"


#endif