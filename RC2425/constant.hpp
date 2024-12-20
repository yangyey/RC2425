#ifndef __H_CONSTANT
#define __H_CONSTANT

#define CLIENT "client"
#define SERVER "server"

#define DSIP_DEFAULT "localhost"
#define DSPORT_DEFAULT 58030

#define TIMEOUT_TIME 5

#define FAIL -1
#define SUCCESS 0
#define VALID 1
#define MAX_ATTEMPTS 8

#define REQUEST_START "SNG"
#define REQUEST_TRY "TRY"
#define REQUEST_SHOW_TRIALS "STR"
#define REQUEST_SCOREBOARD "SSB"
#define REQUEST_QUIT "QUT"
#define REQUEST_DEBUG "DBG"


#define RESPONSE_START "RSG"
#define RESPONSE_TRY "RTR"
#define RESPONSE_SHOW_TRIALS "RST"
#define RESPONSE_SCOREBOARD "RSS"
#define RESPONSE_QUIT "RQT"
#define RESPONSE_DEBUG "RDB"


#define STATUS_OK "OK"
#define STATUS_NOK "NOK"
#define STATUS_ERR "ERR"
#define INVALID_TRY "INV"
#define DUPLICATE_TRY "DUP"
#define NO_TRIAL "ENT"
#define MAX_TIME "ETM"
#define ACCEPT "ACT"
#define FINISH "FIN"


//SIZES//
#define MAX_INPUT_SIZE 512
#define BUFFER_SIZE 4096

#endif