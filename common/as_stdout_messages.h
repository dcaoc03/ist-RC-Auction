#ifndef STDOUT_MESSAGES
#define STDOUT_MESSAGES

// AS ERROR MESSAGES

// Main Function
#define SOCKET_CREATION_ERROR "SOCKET ERROR: failed to initialize %s socket\n"
#define SOCKET_READING_ERROR "SOCKET ERROR: failed to read from %s socket\n"
#define SOCKET_WRITING_ERROR "SOCKET ERROR: failed to write to %s socket\n"
#define AUCTIONS_NOT_FOUND_ERROR "DATABASE ERROR: directory \"AUCTIONS\" not found\n"
#define BADLY_FORMATTED_MESSAGE "PROTOCOL ERROR: received message is badly formatted\n"

// Login Function
#define SUCCESSFUL_LOGIN "Login %s: user succesfully logged in\n"
#define UNSUCCESSFUL_LOGIN "Login %s: failed to login; %s\n"
#define SUCCESSFUL_REGISTRATION "Login %s: user registrated succesfully\n"
#define UNSUCCESSFUL_REGISTRATION "Login %s: unable to register user\n"

#define ARGUMENTS_WRONG_SIZE_ERROR "UID must have length 6 and password length 8"
#define UID_DIGITS_ERROR "UID must be composed of six digits"
#define PASSWORD_DIGITS_ERROR "password must be composed of eight alphanumeric digits"
#define WRONG_PASSWORD_ERROR "wrong password"
#define GENERIC_PASSWORD_ERROR "something wrong happened while checking password"

// Logout Function
#define SUCCESSFUL_LOGOUT "Logout %s: user successfully logged out\n"
#define UNSUCCESSFUL_LOGOUT "Logout %s: failed to log out; %s\n"

#define USER_NOT_LOGGED_IN_ERROR "user is not logged in"
#define USER_NOT_REGISTERED_ERROR "failed to locate user in the database"
#define GENERIC_LOGOUT_ERROR "something wrong happened while logging out"

// Unregister Function
#define SUCCESSFUL_UNREGISTRATION "Unregister %s: user successfully unregistered\n"
#define UNSUCCESSFUL_UNREGISTRATION "Unregister %s: failed unregister user; %s\n"
#define GENERIC_UNREGISTRATION_ERROR "something wrong happened while unregistrating user"

// Open Auction Function
#define SUCCESSFUL_AUCTION_OPENING "Open Auction %s: auction %s launched successfully\n"
#define UNSUCCESSFUL_AUCTION_OPENING "Open Auction %s: auction failed to launch; %s\n"

#define NUMERIC_VALUES_ERROR "numeric values must be positive or 0"
#define MAX_NUM_AUCTIONS_ERROR "maximum number of auctions has been reached"
#define AUCTION_DIRS_ERROR "something went wrong while creating the auction's directories"

// Close Auction Function
#define SUCCESSFUL_AUCTION_CLOSING "Close Auction %s: auction ended successfully\n"
#define UNSUCCESSFUL_AUCTION_CLOSING "Close Auction %s: auction failed to close; %s\n"
#define AUCTION_ALREADY_CLOSED "Close Auction %s: auction has already ended\n"

#define AUCTION_NOT_FOUND_ERROR "auction does not exist"
#define AUCTION_OWNER_ERROR "user is not the owner of the auction"
#define GENERIC_CLOSE_AUCTION_ERROR "something wrong happened while closing the auction"

// My Auctions Function
#define UNSUCCESSFUL_MY_AUCTIONS "My Auctions %s: failed to execute command; %s\n"
#define USER_HAS_NO_ONGOING_AUCTIONS "My Auctions %s: user has no ongoing auctions\n"
#define GENERIC_MY_AUCTIONS_ERROR "something wrong happened while reading an auction file"

// My Bids Function
#define UNSUCCESSFUL_MY_BIDS "My Bids %s: failed to execute command; %s\n"
#define USER_HAS_NO_ONGOING_BIDS "My Bids %s: user has placed no bids on ongoing auctions\n"
#define GENERIC_MY_BIDS_ERROR "something wrong happened while reading an auction file"

#endif