#ifndef __MOCLOUD_UTILS_H__
#define __MOCLOUD_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>

#define MOCLOUD_THR_STACK_SIZE   (4 * 1024 * 1024)

/* 
    Check input @pIp is a valid ip address or not;
    A valid ip address in format like aaa.bbb.ccc.ddd;
    return 0 if invalid, 1 valid;
*/
int isValidIpAddr(const char * pIp);

/*
    The @port should in [1024, 65535] if the process is popular process;
*/
int isValidUserPort(const int port);

/*
    Set the socket(with id @sockId) to nonBlock mode;
    return 0 if succeed, 0- if failed.
*/
int setSock2NonBlock(const int sockId);

/*
    Set a socket to reusable;
*/
int setSockReusable(const int sockId);

/*
    Read function with socket;
    The same as recv();
    return the length being read, if donot equal with @len, error ocurred;
*/
int readn(const int sockId, char* buf, const int len);

/*
    Write function with socket;
    The same as send();
    return the length being write, if donot equal with @len, error ocurred;
*/
int writen(const int sockId, const char* buf, const int len);

/*
    Split a int value to char[4];
*/
int splitInt2Char(const int src, unsigned char dst[4]);

/*
    Merge char[4] to a int value; 
    it is reverse process to splitInt2Char;
*/
int mergeChar2Int(const unsigned char src[4], int *dst);

/*
    Kill the thread with Id @thId;
*/
int killThread(const pthread_t thId);

/*
    A function, when a thread being recv a stop signal, call it.
*/
void threadExitSigCallback(int sigNo);

/*
    Register the signal to a thread;
    When we killThread, we need this register;
*/
int threadRegisterSignal(sigset_t * pSet);

#ifdef __cplusplus
}
#endif

#endif
