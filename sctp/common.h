#define MAX_BUFFER			1024 * 1024
#define MAX_USER			10
#define MAX_PASS			10
#define MAX_DATAGRAM_SIZE 	8 * 120

#define MY_PORT_NUM			19100

#define STREAM				0
#define SIGN_IN   			1

#define LOGIN_RESPONSE_LEN	4
#define COM_RESPONSE_LEN	10

#define	START_USER			4
#define START_BYTES     	4
#define START_CURR      	8
#define START_HIS       	9
#define START_SUC       	10

#define	HEADERS				4
#define MAX_MSG 			15
#define ONE_BYTE            8

//J2M2 PROTOCOL
#define TYPE				0
#define COMMAND 			1
#define ARGSQ				2
#define CODE				3

//METRICS
#define CURRCON				0
#define HISTACC 			1
#define TRABYTES			2
#define CONNSUCC			3

//CONFIGS
#define TRANSFORM			0
#define MEDIATYPES 			1
#define COMM				2


#define REQUEST				0
#define OK		 			1
#define BAD_CREDENTIALS		2
#define BAD_REQUEST			3
#define INVALID_ARG			4
#define UNAUTHORIZED		5



// int log(uint8_t * buf, char * u, char * p);