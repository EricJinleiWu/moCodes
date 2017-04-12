#ifndef __UTILS_H__
#define __UTILS_H__

#include "moCrypt.h"
#include "moUtils.h"
#include "moLogger.h"

#define MOCFT_LOGGER_MODULE_NAME    "mocft"

#define MOCFT_VERSION               "1.0.0"

typedef enum
{
    MOCFT_ALGO_RC4,
    MOCFT_ALGO_BASE64,
    
    MOCFT_ALGO_MAX
}MOCFT_ALGO;

typedef struct
{
    MOCRYPT_METHOD method;
    MOCFT_ALGO algo;
    char srcfilepath[MOCRYPT_FILEPATH_LEN];
    char dstfilepath[MOCRYPT_FILEPATH_LEN];
    char key[MOCRYPT_KEY_MAXLEN];
    unsigned int keyLen;
}MOCFT_TASKINFO;

#define MOCFT_INVALID_SOCKID    -1
#define MOCFT_CP_SOCKET_FILE    "/tmp/mocftSockFile"
#define MOCFT_CP_SOCKET_SERV_MAXLISTENNUM   4

#define MOCFT_MARK    "mocft"
#define MOCFT_MARKLEN   6   //strlen(MOCFT_MARK) + 1

#define MOCFT_REQUESTMSG_PREFIX "request"
#define MOCFT_REQUESTMSG_PREFIX_LEN   8   //strlen(MOCFT_REQUESTMSG_PREFIX) + 1

#define MOCFT_RESPONSEMSG_PREFIX "response"
#define MOCFT_NOTIFYMSG_PREFIX "progress"
#define MOCFT_RESPONSEMSG_PREFIX_LEN   9   //strlen(MOCFT_RESPONSEMSG_PREFIX) + 1


typedef struct
{
    char mark[MOCFT_MARKLEN];
    char prefix[MOCFT_REQUESTMSG_PREFIX_LEN];
    MOCFT_TASKINFO task;
}MOCFT_REQMSG;

typedef struct
{
    char mark[MOCFT_MARKLEN];
    /*
        result and progress all using this struct, difference is the prefix, one is "response", the other is "progress";
    */
    char prefix[MOCFT_RESPONSEMSG_PREFIX_LEN];
    /*
        if prefix==response, this value is the ret of crypt;
        if prefix==progress, this value is the progress of crypting;
        else, meaningless;
    */
    int value;
}MOCFT_RESPMSG;


#endif
