int handleLogin(char * second, char * third, int connSock);
int handleMetric(char * second, char * third, int connSock);
int handleConfig(char * second, char * third, int connSock);
int handleHelp();
int handleExit(char * second, char * third, int connSock);
void printDatagram(void * datagram, int size);