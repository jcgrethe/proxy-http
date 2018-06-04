#define MAX_BUFFER	2048
#define MY_PORT_NUM	19100
#define STREAM		0
#define SIGN_IN   	1
#define MAX_USER	10
#define MAX_PASS	10
#define	START_USER	4
#define MAX_DATAGRAM_SIZE 104

#define TYPE		0
#define COMMAND 	1
#define ARGSQ		2
#define CODE		3

#define LOGIN_RESPONSE_LEN	4
#define COM_RESPONSE_LEN	10

#define START_BYTES     4
#define START_CURR      11
#define START_HIS       12
#define START_SUC       13

// int log(uint8_t * buf, char * u, char * p);