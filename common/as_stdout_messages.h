#ifndef AS_STDOUT_MESSAGES
#define AS_STDOUT_MESSAGES

// Main Function
#define SOCKET_CREATION_ERROR "SOCKET ERROR: failed to initialize %s socket\n"
#define SOCKET_READING_ERROR "SOCKET ERROR: failed to read from %s socket\n"
#define SOCKET_WRITING_ERROR "SOCKET ERROR: failed to write to %s socket\n"
#define AUCTIONS_NOT_FOUND_ERROR "DATABASE ERROR: directory \"AUCTIONS\" not found\n"
#define BADLY_FORMATTED_MESSAGE "PROTOCOL ERROR: received message is badly formatted\n"
#define SIGNAL_HANDLING_ERROR "SIGNAL ERROR: failed to set signal handler\n"
#define IMAGE_FILE_DESCRIPTOR_ERROR "ERROR: failed to read from the designated file descriptor\n"
#define CHILD_PROCESS_ERROR "ERROR: failed to create child process: %s\n"
#define AS_CLOSING_MESSAGE " AS closed successfully\n"

#define REQUEST_RECEIVED "NEW %s REQUEST: sent by [%s:%s]\n"
#define ISSUED_REQUEST "REQUEST COMMAND: %s\n"
#define UNKNOWN_REQUEST "ERROR: unknown request %s received\n"

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
#define ASSET_CREATION_ERROR "something went wrong while creating the asset"
#define AUCTION_DIRS_ERROR "something went wrong while creating the auction's directories"

// Close Auction Function
#define SUCCESSFUL_AUCTION_CLOSING "Close Auction %s: auction ended successfully\n"
#define UNSUCCESSFUL_AUCTION_CLOSING "Close Auction %s: auction failed to close; %s\n"
#define AUCTION_ALREADY_CLOSED "Close Auction %s: auction has already ended\n"

#define AUCTION_NOT_FOUND_ERROR "auction does not exist"
#define AUCTION_OWNER_ERROR "user is not the owner of the auction"
#define GENERIC_CLOSE_AUCTION_ERROR "something wrong happened while closing the auction"

// My Auctions and My Bids Function
#define UNSUCCESSFUL_MY_AUCTIONS "My Auctions %s: failed to execute command; %s\n"
#define USER_HAS_NO_ONGOING_AUCTIONS "My Auctions %s: user has no ongoing auctions\n"
#define GENERIC_MY_AUCTIONS_ERROR "something wrong happened while reading an auction file"

#define UNSUCCESSFUL_MY_BIDS "My Bids %s: failed to execute command; %s\n"
#define USER_HAS_NO_ONGOING_BIDS "My Bids %s: user has placed no bids on ongoing auctions\n"
#define GENERIC_MY_BIDS_ERROR "something wrong happened while reading an auction file"

#define NON_EXISTING_MODE_ERROR "ERROR: tried to execute a function using a non existing mode\n"

// List Function
#define NO_AUCTIONS "List: no auctions have been made yet\n"
#define UNSUCCESSFUL_LIST "List: command failed, %s\n"
#define GENERIC_LIST_ERROR "something wrong happened while listing the auctions"

// Bid Function
#define SUCCESSFUL_BID "Bid %ld: bid made successfully\n"
#define UNSUCCESSFUL_BID "Bid %ld: failed to make a bid, %s\n"
#define AUCTION_NOT_ACTIVE_ERROR "auction is not active"
#define BID_IN_OWN_AUCTION_ERROR "user tried to bid in their own auction"
#define BID_IS_NOT_VALID_ERROR "a bigger bid has already been made"
#define GENERIC_BID_ERROR "something wrong happened while placing a bid"

// Show Asset Function
#define UNSUCCESSFUL_SHOW_ASSET "Show Auction %s: failed to show asset; %s\n"
#define ASSET_NOT_FOUND_ERROR "couldn't find requested asset"
#define GENERIC_SHOW_ASSET_ERROR "something wrong happened while getting the asset"

// Show Record Function
#define UNSUCCESSFUL_SHOW_RECORD "Show Record %s: failed to show record; %s\n"

#endif