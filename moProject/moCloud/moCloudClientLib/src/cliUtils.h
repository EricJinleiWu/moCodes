#ifndef __CLI_UTILS_H__
#define __CLI_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define CONNECT_TIMEOUT 3   //in seconds, when connect to server, this is the timeout value


/*
    create socket;
    set socket id to @gDataSockId;
    set this socket to non block;
    bind it to the ctrl port;
*/
int createSocket(const char * ip, const int port, int * pSockId);

/*
    connect to server with TCP mode;
    @sockId is the id of a socket, because we have data socket id and ctrl socket id, 
        so we should has this param to define them;
*/
int connectToServer(const int sockId, const char *servIp, const int servPort);

/*
    If socket has valid socket id, should free it;
*/
void destroySocket(int * pSockId);


#ifdef __cplusplus
}
#endif

#endif
