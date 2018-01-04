#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "moCpsServer.h"
#include "moLogger.h"
#include "moCpsUtils.h"
#include "fileMgr.h"
#include "cliMgr.h"

#define CONF_SECNAME    "servInfo"
#define CONF_IPNAME     "servIp"
#define CONF_PORTNAME   "servPort"
#define CONF_DIRNAME    "dirPath"

#define LISTEN_BACKLOG  128 //listen(sockid, backlog), backlog value

#define THREAD_TIME_INTEVAL 1

typedef struct
{
    char ip[MOCPS_IP_ADDR_MAXLEN];
    int port;
    char dirPath[DIRPATH_MAXLEN];
}CONF_INFO;

typedef enum
{
    //thread donot being created, send data thread will not create at the beginning, just 
    //when we recv the request from client which want to get file, create this thread
    THREAD_STATE_DONOT_CREATE,
    //Thread running
    THREAD_STATE_RUNNING,
    //Thread should be deleted
    THREAD_STATE_DELETING,
    //Thread being stopped
    THREAD_STATE_DELETED
}THREAD_STATE;


/********************************************************************************************/
/************************ A module to manage all client running infomation *******************/
/********************************************************************************************/
typedef struct 
{
    char ip[MOCPS_IP_ADDR_MAXLEN];
    in_addr_t ipValue;
    
    int ctrlSockId;
    int ctrlPort;
    pthread_t ctrlThrId;
    THREAD_STATE ctrlThrState;

    int dataSockId;
    int dataPort;
    pthread_t dataThrId;
    THREAD_STATE dataThrState;

    //When do keyagree, set this value
    MOCPS_CRYPT_INFO cryptInfo;
}CLI_RUNNING_INFO;

typedef struct _CLI_RUNNING_INFO_NODE
{
    CLI_RUNNING_INFO info;
    struct _CLI_RUNNING_INFO_NODE * next;
}CLI_RUNNING_INFO_NODE;

static CLI_RUNNING_INFO_NODE * gpCriListHead = NULL;
static pthread_mutex_t gCriMutex = PTHREAD_MUTEX_INITIALIZER;

static int gSockId = MOCPS_INVALID_SOCKID;

static void criDump();


/*
    Thread, to check all clients thread state, if state in DELETING, 
    means we should stop its thread, and delete it from list;
*/
static void * criCheckThr(void * args)
{
    args = args;

    while(1)
    {
        struct timeval tm;
        tm.tv_sec = 1;
        tm.tv_usec = 0;
        int ret = select(0, NULL, NULL, NULL, &tm);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "select failed! ret=%d, errno=%d, desc=[%s]\n",
                ret, errno, strerror(errno));
            break;
        }

        pthread_mutex_lock(&gCriMutex);

        CLI_RUNNING_INFO_NODE * pPreNode = gpCriListHead;
        if(NULL == pPreNode)
            break;
        
        CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
        while(pCurNode != NULL)
        {
            if(pCurNode->info.ctrlThrState == THREAD_STATE_DELETING)
            {
                killThread(pCurNode->info.ctrlThrId);
                pCurNode->info.ctrlThrState = THREAD_STATE_DELETED;
                
                if(pCurNode->info.dataThrState == THREAD_STATE_DELETING)
                {
                    killThread(pCurNode->info.dataThrId);
                    pCurNode->info.dataThrState = THREAD_STATE_DELETED;
                }
                
                int ret = cliMgrDeleteCli(pCurNode->info.ctrlSockId);
                if(ret < 0)
                {
                    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cliMgrDeleteCli failed! ret = %d\n", ret);
                }
                else
                {
                    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "cliMgrDeleteCli succeed.\n");
                }
                
                pPreNode->next = pCurNode->next;
                free(pCurNode);
                pCurNode = NULL;
                pCurNode = pPreNode->next;
            }
            else
            {
                pCurNode = pCurNode->next;
            }
        }
        
        pthread_mutex_unlock(&gCriMutex);
    }

    return NULL;
}

static int criInit()
{
    if(NULL != gpCriListHead)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "module CRI has been inited! cannot init again.\n");
        return -1;
    }

    gpCriListHead = (CLI_RUNNING_INFO_NODE *)malloc(sizeof(CLI_RUNNING_INFO_NODE) * 1);
    if(NULL == gpCriListHead)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc succeed.\n");
    memset(gpCriListHead->info.ip, 0x00, MOCPS_IP_ADDR_MAXLEN);
    gpCriListHead->info.ipValue = 0;
    gpCriListHead->info.ctrlSockId = MOCPS_INVALID_SOCKID;
    gpCriListHead->info.ctrlPort = MOCPS_INVALID_PORT;
    gpCriListHead->info.ctrlThrId = MOCPS_INVALID_THR_ID;
    gpCriListHead->info.ctrlThrState = THREAD_STATE_DONOT_CREATE;
    gpCriListHead->info.dataSockId = MOCPS_INVALID_SOCKID;
    gpCriListHead->info.dataPort = MOCPS_INVALID_PORT;
    gpCriListHead->info.dataThrId = MOCPS_INVALID_THR_ID;
    gpCriListHead->info.dataThrState = THREAD_STATE_DONOT_CREATE;
    gpCriListHead->next = NULL;

    //Start thread to check list
    pthread_t thId;
    int ret = pthread_create(&thId, NULL, criCheckThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "create thread failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        free(gpCriListHead);
        gpCriListHead = NULL;
        return -3;
    }
    
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "criInit succeed.\n");
    return 0;
}

static void criUnInit()
{
    if(NULL != gpCriListHead)
    {
        CLI_RUNNING_INFO_NODE * pPreNode = gpCriListHead;
        CLI_RUNNING_INFO_NODE * pCurNode = pPreNode->next;
        while(pCurNode != NULL)
        {
            pPreNode->next = pCurNode->next;
            free(pCurNode);
            pCurNode = NULL;
            pCurNode = pPreNode->next;
        }
        free(gpCriListHead);
        gpCriListHead = NULL;
    }
}

/*
    ctrl socket id is the key of a client;
    when we need fresh its info, like thread id, data port, and other info,
    should loop this list, and find a node with socketId

    If @sockId has been exist in this list, return 0, too.
*/
static int criInsertNewCliInfo(const char * ip, const int sockId, const int ctrlPort)
{
    if(NULL == ip)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    
    pthread_mutex_lock(&gCriMutex);
    if(gpCriListHead == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCreListHead is NULL! cannot insert anything!\n");
        pthread_mutex_unlock(&gCriMutex);
        return -2;
    }

    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ctrlSockId == sockId)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client has been exist!\n");
            pthread_mutex_unlock(&gCriMutex);
            //We think this means client being inserted yet, return 0, too.
            return 0;
        }
        pCurNode = pCurNode->next;
    }
    //malloc memory for this client
    CLI_RUNNING_INFO_NODE * pNewNode = NULL;
    pNewNode = (CLI_RUNNING_INFO_NODE *)malloc(sizeof(CLI_RUNNING_INFO_NODE) * 1);
    if(NULL == pNewNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        pthread_mutex_unlock(&gCriMutex);
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc for new node succeed.\n");

    //Set values
    strncpy(pNewNode->info.ip, ip, MOCPS_IP_ADDR_MAXLEN);
    pNewNode->info.ip[MOCPS_IP_ADDR_MAXLEN - 1] = 0x00;
    pNewNode->info.ipValue = inet_addr(pNewNode->info.ip);
    pNewNode->info.ctrlSockId = sockId;
    pNewNode->info.ctrlPort = ctrlPort;
    pNewNode->info.ctrlThrId = MOCPS_INVALID_THR_ID;
    pNewNode->info.ctrlThrState = THREAD_STATE_DONOT_CREATE;
    pNewNode->info.dataSockId = MOCPS_INVALID_SOCKID;
    pNewNode->info.dataPort = MOCPS_INVALID_PORT;
    pNewNode->info.dataThrId = MOCPS_INVALID_THR_ID;
    pNewNode->info.dataThrState = THREAD_STATE_DONOT_CREATE;

    //insert to list
    pNewNode->next = gpCriListHead->next;
    gpCriListHead->next = pNewNode;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "New node being inserted after head node!\n");

    pthread_mutex_unlock(&gCriMutex);
    return 0;
}

/*
    If @ctrlSockId donot exist, return 0, too.
    we just set this node state to deleting;
    delete the node, will be done in thread criCheckThr;
*/
static int criDeleteCliInfo(const int ctrlSockId)
{
    pthread_mutex_lock(&gCriMutex);
    if(gpCriListHead == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCreListHead is NULL! cannot insert anything!\n");
        pthread_mutex_unlock(&gCriMutex);
        return -1;
    }

    CLI_RUNNING_INFO_NODE * pPreNode = gpCriListHead;
    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ctrlSockId == ctrlSockId)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find this client!\n");
            pCurNode->info.ctrlThrState = THREAD_STATE_DELETING;
            pCurNode->info.dataThrState = THREAD_STATE_DELETING;
            break;
        }
        pPreNode = pCurNode;
        pCurNode = pCurNode->next;
    }
    if(pCurNode == NULL)
    {
        moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "Client donot exist yet!\n");
        pthread_mutex_unlock(&gCriMutex);
        return 0;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "find client! and set its state to deleting, wait for delete later!\n");

    pthread_mutex_unlock(&gCriMutex);
    return 0;
}

/*
    Check @ip exist or not;
    exist, return 1; else, return 0;
*/
static int criIsCliExist(const in_addr_t ipValue)
{
    if(NULL == gpCriListHead)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCirListHead is NULL!\n");
        return 0;
    }

    pthread_mutex_lock(&gCriMutex);

    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ipValue == ipValue)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client exist yet!\n");
            pthread_mutex_unlock(&gCriMutex);
            return 1;
        }
        pCurNode = pCurNode->next;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client donot exist!\n");
    pthread_mutex_unlock(&gCriMutex);
    return 0;
}

/*
    ctrl socket will send a data port info to server, should set it;
    when recv data port accept request, check its port;
*/
static int criSetDataSockPort(const int ctrlSockId, const int dataPort)
{
    if(NULL == gpCriListHead)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCirListHead is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&gCriMutex);

    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ctrlSockId == ctrlSockId)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client being found!\n");

            pCurNode->info.dataSockId = MOCPS_INVALID_SOCKID;
            pCurNode->info.dataPort = dataPort;
            pCurNode->info.dataThrId = MOCPS_INVALID_THR_ID;
            pCurNode->info.dataThrState = THREAD_STATE_DONOT_CREATE;
            
            pthread_mutex_unlock(&gCriMutex);
            return 0;
        }
        pCurNode = pCurNode->next;
    }
    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot find this client!\n");
    pthread_mutex_unlock(&gCriMutex);
    return -2;
}

static int criRefreshCtrlThrInfo(const int ctrlSockId, const pthread_t thId)
{
    if(NULL == gpCriListHead)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCirListHead is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&gCriMutex);

    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ctrlSockId == ctrlSockId)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client being found!\n");

            pCurNode->info.ctrlThrId = thId;
            pCurNode->info.ctrlThrState = THREAD_STATE_RUNNING;
            
            pthread_mutex_unlock(&gCriMutex);
            return 0;
        }
        pCurNode = pCurNode->next;
    }
    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot find this client!\n");
    pthread_mutex_unlock(&gCriMutex);
    return -2;
}

/*
    A client use its data port to connect to server;
    We should check its data port is right or not firstly;
*/
static int criSetDataSocketInfo(const in_addr_t ipValue, const int sockId, const int port)
{
    if(NULL == gpCriListHead)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCirListHead is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&gCriMutex);

    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ipValue == ipValue)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client being found!\n");

            if(port != pCurNode->info.dataPort)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "input port=%d, saved port=%d, donot equal!\n",
                    port, pCurNode->info.dataPort);
                pthread_mutex_unlock(&gCriMutex);
                return -2;
            }
            pCurNode->info.dataSockId = sockId;
            
            pthread_mutex_unlock(&gCriMutex);
            return 0;
        }
        pCurNode = pCurNode->next;
    }
    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot find this client!\n");
    pthread_mutex_unlock(&gCriMutex);
    return -3;
}

static int criRefreshCryptInfo(const int ctrlSockId, MOCPS_CRYPT_INFO info)
{
    if(NULL == gpCriListHead)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCirListHead is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&gCriMutex);

    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ctrlSockId == ctrlSockId)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client being found!\n");
            memcpy(&pCurNode->info.cryptInfo, &info, sizeof(MOCPS_CRYPT_INFO));
            pthread_mutex_unlock(&gCriMutex);
            return 0;
        }
        pCurNode = pCurNode->next;
    }
    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot find this client!\n");
    pthread_mutex_unlock(&gCriMutex);
    return -2;
}

static int criGetCryptInfo(const int ctrlSockId, MOCPS_CRYPT_INFO *pInfo)
{
    if(NULL == pInfo)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
 
    if(NULL == gpCriListHead)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCirListHead is NULL!\n");
        return -2;
    }

    pthread_mutex_lock(&gCriMutex);

    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ctrlSockId == ctrlSockId)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client being found!\n");
            memcpy(pInfo, &pCurNode->info.cryptInfo, sizeof(MOCPS_CRYPT_INFO));
            pthread_mutex_unlock(&gCriMutex);
            return 0;
        }
        pCurNode = pCurNode->next;
    }
    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot find this client!\n");
    pthread_mutex_unlock(&gCriMutex);
    return -3;   
}

static void criDump()
{
    printf("==================== Dump CRI module resources start now ====================\n");
    
    do
    {
        if(NULL == gpCriListHead)
        {
            printf("Head node is NULL!\n");
            break;
        }
    
        pthread_mutex_lock(&gCriMutex);

        int cnt = 0;
        CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
        
        while(pCurNode != NULL)
        {
            printf("\t IP=[%s], ctrlPort=%d, dataPort=%d, ctrlThreadId=%u, dataThreadId=%u, " \
                "ctrlThreadState=%d, dataThreadState=%d, cryptAlgoNo=%d\n",
                pCurNode->info.ip, pCurNode->info.ctrlPort, pCurNode->info.dataPort, 
                pCurNode->info.ctrlThrId, pCurNode->info.dataThrId, 
                pCurNode->info.ctrlThrState, pCurNode->info.dataThrState,
                pCurNode->info.cryptInfo.cryptAlgoNo);
            cnt++;
            pCurNode = pCurNode->next;
        }
        printf("\t In summary, %d nodes here.\n", cnt);
        
        pthread_mutex_unlock(&gCriMutex);    
    }while(0);
    
    printf("==================== Dump CRI module resources stop now ====================\n");
}

/*
    When a client being invalid, like heartbeat timeout£¬
    we should stop its threads, and free its resources, this function just recv the invalid client 
    infomation, then do it.
    
    return 0 if succeed.
*/
int recvInvalidCliInfo(const int ctrlSockId)
{
#if 0
    
    pthread_mutex_lock(&gCriMutex);
    if(gpCriListHead == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gpCreListHead is NULL!\n");
        pthread_mutex_unlock(&gCriMutex);
        return -1;
    }

    CLI_RUNNING_INFO_NODE * pCurNode = gpCriListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.ctrlSockId == ctrlSockId)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find this client!\n");

            if(pCurNode->info.ctrlThrId > 0)
            {
                pCurNode->info.ctrlThrState = THREAD_STATE_DELETING;
            }
            if(pCurNode->info.dataThrId > 0)
            {
                pCurNode->info.dataThrState = THREAD_STATE_DELETING;
            }
            break;
        }
        pCurNode = pCurNode->next;
    }
    
    pthread_mutex_unlock(&gCriMutex);

#endif

    return criDeleteCliInfo(ctrlSockId);
}

/*
    Config file format:
    [servInfo]
    servIp = 127.0.0.1
    servPort = 4096
    dirPath = ./MoCpsDir
*/
static int parseConfFile(const char * pConfFilepath, CONF_INFO * pInfo)
{
    if(NULL == pConfFilepath || NULL == pInfo)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "config file path is [%s]\n",
        pConfFilepath);

    MOUTILS_INI_SECTION_INFO_NODE * pHeadNode = moUtils_Ini_Init(pConfFilepath);
    if(NULL == pHeadNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_Init failed! config file path is [%s]\n",
            pConfFilepath);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_Init succeed.\n");

    char tmp[MOUTILS_INI_ATTR_VALUE_MAX_LEN] = {0x00};
    memset(tmp, 0x00, MOUTILS_INI_ATTR_VALUE_MAX_LEN);
    int ret = moUtils_Ini_GetAttrValue(CONF_SECNAME, CONF_IPNAME, tmp, pHeadNode);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue for [%s] failed! ret = %d\n",
            CONF_IPNAME, ret);
        moUtils_Ini_UnInit(pHeadNode);
        pHeadNode = NULL;
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get server ip ok, value=[%s]\n", tmp);
    strncpy(pInfo->ip, tmp, MOCPS_IP_ADDR_MAXLEN);
    pInfo->ip[MOCPS_IP_ADDR_MAXLEN - 1] = 0x00;

    memset(tmp, 0x00, MOUTILS_INI_ATTR_VALUE_MAX_LEN);
    ret = moUtils_Ini_GetAttrValue(CONF_SECNAME, CONF_PORTNAME, tmp, pHeadNode);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue for [%s] failed! ret = %d\n",
            CONF_PORTNAME, ret);
        moUtils_Ini_UnInit(pHeadNode);
        pHeadNode = NULL;
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get server port ok, value=[%s]\n", tmp);
    pInfo->port = atoi(tmp);
    
    memset(tmp, 0x00, MOUTILS_INI_ATTR_VALUE_MAX_LEN);
    ret = moUtils_Ini_GetAttrValue(CONF_SECNAME, CONF_DIRNAME, tmp, pHeadNode);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moUtils_Ini_GetAttrValue for [%s] failed! ret = %d\n",
            CONF_DIRNAME, ret);
        moUtils_Ini_UnInit(pHeadNode);
        pHeadNode = NULL;
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get server dir path ok, value=[%s]\n", tmp);
    strncpy(pInfo->dirPath, tmp, DIRPATH_MAXLEN);
    pInfo->dirPath[DIRPATH_MAXLEN - 1] = 0x00;

    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Server config info : ip=[%s], port=%d, dirpath=[%s]\n",
        pInfo->ip, pInfo->port, pInfo->dirPath);
    moUtils_Ini_UnInit(pHeadNode);
    pHeadNode = NULL;
    return 0;
}

/*
    Set socket attribute, currently, support:
        1.reused;
        2.non-block;
*/
static int setSockAttr(const int sockId)
{
    //1.set this socket to reusable
    int on = 1;
    int ret = setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "setsockopt failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "setsockopt ok.\n");

    //2.set socket to non-block
    ret = setSock2NonBlock(sockId);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "setSock2NonBlock failed! ret = %d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "setSock2NonBlock ok.\n");

    return 0;
}

/*
    create socket;
    bind to a port;
    set attributes;
    set listen;
*/
static int initSocket(const char *ip, const int port)
{
    if(NULL == ip)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "ip=[%s], port=%d\n", ip, port);

    //1.create socket
    if(gSockId != MOCPS_INVALID_SOCKID)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gSockId=%d, not MOCPS_INVALID_SOCKID(%d), cannot initSocket!\n",
            gSockId, MOCPS_INVALID_SOCKID);
        return -2;
    }
    gSockId = socket(AF_INET, SOCK_STREAM, 0);
    if(gSockId < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "create socket failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Create socket succeed! gSockId=%d\n", gSockId);

    //2.set attributes
    int ret = setSockAttr(gSockId);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "setSockAttr failed! ret = %d\n", ret);
        close(gSockId);
        gSockId = MOCPS_INVALID_SOCKID;
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Set socket attributes succeed.\n");

    //3.bind to port
    struct sockaddr_in servAddr;
    memset(&servAddr, 0x00, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(ip);
    servAddr.sin_port = htons(port);
    ret = bind(gSockId, (struct sockaddr *)&servAddr, sizeof(struct sockaddr));
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "bind failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        close(gSockId);
        gSockId = MOCPS_INVALID_SOCKID;
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "bind succeed.\n");

    //4.set listen backlog
    ret = listen(gSockId, LISTEN_BACKLOG);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "listen failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        close(gSockId);
        gSockId = MOCPS_INVALID_SOCKID;
        return -6;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "listen succeed!\n");

    return 0;
}

static void uninitSocket()
{
    if(gSockId > 0)
    {
        close(gSockId);
        gSockId = MOCPS_INVALID_SOCKID;
    }
}

/*
    Do init to MoCpsServer;
    @pConfFilepath is the config file path; We should read its port and ip from this file;
    return 0 if succeed, else failed;

    1.parse config file, get server ip and port;
    2.init socket;
    3.init criModule;
    4.init fileMgrModule and cliMgrModule;
*/
int moCpsServ_init(const char *pConfFilepath)
{
    //1.parse config file
    if(NULL == pConfFilepath)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "config file path is [%s]\n",
        pConfFilepath);
    CONF_INFO confInfo;
    int ret = parseConfFile(pConfFilepath, &confInfo);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "parseConfFile failed! " \
            "ret = %d, configFilepath = [%s]\n", ret, pConfFilepath);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "parseConfFile succeed. " \
        "server ip = [%s], port = [%d]\n", confInfo.ip, confInfo.port);

    //2.init invalidCtrlSockidModule and invalidDataSockidModule
    ret = criInit();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "criInit failed! ret = %d\n", ret);
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "criInit succeed.\n");

    //3.init fileMgr and cliMgr module
    ret = cliMgrInit(recvInvalidCliInfo);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cliMgrInit failed! ret = %d\n", ret);
        criUnInit();
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "cliMgrInit succeed.\n");
    ret = fmInit(confInfo.dirPath);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "fmInit failed! ret = %d\n", ret);
        cliMgrUnInit();
        criUnInit();
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "fmInit succeed.\n");

    //4.init server socket
    ret = initSocket(confInfo.ip, confInfo.port);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "initSocket failed! ret = %d\n", ret);
        return -6;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "initSocket succeed.\n");

    //for rand()
    srand((unsigned int)time(NULL));
    
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCpsServer init ok!\n");
    return 0;
}

/*
    Do uninit to MoCpsServer;
*/
void moCpsServ_unInit()
{
    uninitSocket();
    fmUnInit();
    cliMgrUnInit();
    criUnInit();
}

/*
    Check @header in right format or not;
*/
static int isRightFormatRequest(const MOCPS_CTRL_REQUEST request)
{
    if(strcmp(request.basicInfo.mark, MOCPS_MARK_CLIENT) != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Mark=[%s], donot [%s], not a right format header!\n",
            request.basicInfo.mark, MOCPS_MARK_CLIENT);
        return 0;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Mark ok.\n");

    if(request.basicInfo.cmdId >= MOCPS_CMDID_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "CmdId=%d, larger than %d, not a right format header!\n",
            request.basicInfo.cmdId, MOCPS_CMDID_MAX);
        return 0;        
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "CmdId ok.\n");

    int ret = moUtils_Check_checkCrc((unsigned char *)&request, 
        sizeof(MOCPS_CTRL_REQUEST) - sizeof(MOUTILS_CHECK_CRCVALUE), 
        MOUTILS_CHECK_CRCMETHOD_32, request.crc32);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "moUtils_Check_checkSum failed! ret = %d\n", ret);
        return 0;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "CRC check ok.\n");
    return 1;
}

/*
    Generate a response for keyagree request which has wrong format
*/
static int genErrKeyAgreeResp(MOCPS_KEYAGREE_RESPONSE *pResp)
{
    if(NULL == pResp)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    pResp->cmdId = MOCPS_CMDID_KEYAGREE;
    strncpy(pResp->mark, MOCPS_MARK_SERVER, MOCPS_MARK_MAXLEN);
    pResp->mark[MOCPS_MARK_MAXLEN - 1] = 0x00;
    memset(&pResp->cryptInfo, 0x00, sizeof(MOCPS_CRYPT_INFO));
    pResp->cryptInfo.cryptAlgoNo = MOCPS_CRYPT_ALGO_IDLE;
    moUtils_Check_getCrc((char *)pResp, sizeof(MOCPS_KEYAGREE_RESPONSE) - sizeof(MOUTILS_CHECK_CRCVALUE),
        MOUTILS_CHECK_CRCMETHOD_32, &pResp->crc32);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "generate keyAgree response ok.\n");
    return 0;
}

/*
    generate an array in length @len;
*/
static void genRandArray(char * pArray, const int len)
{
    int i = 0;
    for(; i < len; i++)
    {
        pArray[i] = rand() % 127;
    }
}

static void dumpArrayValue(char * pArray, const int len)
{
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Dump Array now:\n");
    int i = 0;
    for(; i < len; i++)
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "pArray[%d] = %d(0x%x)\n",
            i, pArray[i], pArray[i]);
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Dump over.\n");
}

/*
    Generate a response for keyagree request which has right format
*/
static int genRightKeyAgreeResp(MOCPS_KEYAGREE_RESPONSE *pResp)
{
    if(NULL == pResp)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    pResp->cmdId = MOCPS_CMDID_KEYAGREE;
    strncpy(pResp->mark, MOCPS_MARK_SERVER, MOCPS_MARK_MAXLEN);
    pResp->mark[MOCPS_MARK_MAXLEN - 1] = 0x00;

    memset(&pResp->cryptInfo, 0x00, sizeof(MOCPS_CRYPT_INFO));
    pResp->cryptInfo.cryptAlgoNo = MOCPS_CRYPT_ALGO_DES/*rand() % (MOCPS_CRYPT_ALGO_MAX - 1) + 1*/;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "To this client, algoNo=%d, will be set.\n", 
        pResp->cryptInfo.cryptAlgoNo);
    switch(pResp->cryptInfo.cryptAlgoNo)
    {
        case MOCPS_CRYPT_ALGO_DES:
            genRandArray(pResp->cryptInfo.cryptKey.desKey, MOCRYPT_DES_KEYLEN);
            pResp->cryptInfo.keyLen = MOCRYPT_DES_KEYLEN;
            dumpArrayValue(pResp->cryptInfo.cryptKey.desKey, pResp->cryptInfo.keyLen); 
            break;
        case MOCPS_CRYPT_ALGO_DES3:
        case MOCPS_CRYPT_ALGO_AES:  //AES, being des3 currently
            genRandArray(pResp->cryptInfo.cryptKey.des3Key, MOCRYPT_DES_KEYLEN * 3);
            pResp->cryptInfo.keyLen = MOCRYPT_DES_KEYLEN * 3;
            dumpArrayValue(pResp->cryptInfo.cryptKey.des3Key, pResp->cryptInfo.keyLen); 
            break;
        case MOCPS_CRYPT_ALGO_RC4:
            genRandArray(pResp->cryptInfo.cryptKey.rc4Key, MOCRYPT_RC4_KEY_MAX_LEN);
            pResp->cryptInfo.keyLen = MOCRYPT_RC4_KEY_MAX_LEN;
            dumpArrayValue(pResp->cryptInfo.cryptKey.rc4Key, pResp->cryptInfo.keyLen); 
            break;
        default:
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cryptAlgoNo error! value=%d, not in (%d, %d)\n", 
                pResp->cryptInfo.cryptAlgoNo, MOCPS_CRYPT_ALGO_IDLE, MOCPS_CRYPT_ALGO_MAX);
            return -2;
    }
    
    moUtils_Check_getCrc((char *)pResp, sizeof(MOCPS_KEYAGREE_RESPONSE) - sizeof(MOUTILS_CHECK_CRCVALUE),
        MOUTILS_CHECK_CRCMETHOD_32, &pResp->crc32);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "generate keyAgree response ok.\n");
    return 0;
}

/*
    To ctrlSockId, do key agree;
*/
static int doKeyAgree(const int ctrlSockId, char *recvBuf, int recvLen)
{
    if(NULL == recvBuf)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Start read request now.\n");
    
    int readLen = readn(ctrlSockId, recvBuf, recvLen);
    if(readLen != recvLen)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "readn failed! readLen = %d, recvLen = %d\n",
            readLen, recvLen);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "readn succeed.\n");

    //Find the mark
    int ret = moUtils_Search_BF((unsigned char *)recvBuf, recvLen, 
        (unsigned char *)MOCPS_MARK_CLIENT, strlen(MOCPS_MARK_CLIENT));
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Find MARK failed! ret=%d\n", ret);
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find MARK ok, pos=%d.\n", ret);

    //MARK donot at the beginning of text, should recv left text now
    if(ret != 0)
    {
        memmove(recvBuf, recvBuf + ret, recvLen - ret);
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "we should recv left text now.\n");
        readLen = readn(ctrlSockId, recvBuf + recvLen - ret, ret);
        if(readLen != ret)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "read left text failed!readLen=%d, leftLen=%d\n",
                readLen, ret);
            return -4;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "read left text succeed.\n");
    }

    //check this request format, generate response
    MOCPS_KEYAGREE_RESPONSE resp;
    memset(&resp, 0x00, sizeof(MOCPS_KEYAGREE_RESPONSE));
    MOCPS_CTRL_REQUEST request;
    memcpy(&request, recvBuf, sizeof(MOCPS_CTRL_REQUEST));
    if(!isRightFormatRequest(request))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "The request donot in right format!\n");
        genErrKeyAgreeResp(&resp);
    }
    else
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "The request is in right format, start do it.\n");
        genRightKeyAgreeResp(&resp);
        criRefreshCryptInfo(ctrlSockId, resp.cryptInfo);
        cliMgrSetState(ctrlSockId, CLIMGR_CLI_STATE_KEYAGREED);
    }

    //send keyAgree response to client
    int writeLen = writen(ctrlSockId, (char *)&resp, sizeof(MOCPS_KEYAGREE_RESPONSE));
    if(writeLen != sizeof(MOCPS_KEYAGREE_RESPONSE))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "writen failed! ret=%d, length=%d\n",
            ret, sizeof(MOCPS_KEYAGREE_RESPONSE));
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "writen succeed!\n");
    return 0;
}

static int doDecryptWithDes(char * pCipher, const int len, char * pPlain, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(NULL == pCipher || NULL == pPlain || 0 == len)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
#if 0
    unsigned int plainLen = 0;
    int ret = moCrypt_DES_ECB(MOCRYPT_METHOD_DECRYPT, (unsigned char *)pCipher,
        (unsigned int)len, (unsigned char *)cryptInfo.cryptKey.desKey, MOCRYPT_DES_KEYLEN,
        (unsigned char *)pPlain, &plainLen);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES_ECB failed! ret = %d\n", ret);
        return -2;
    }
#else
    memcpy(pPlain, pCipher, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES_ECB succeed.\n");
    return 0;
}

static int doDecryptWithDes3(char * pCipher, const int len, char * pPlain, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(NULL == pCipher || NULL == pPlain || 0 == len)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
#if 0
    unsigned int plainLen = 0;
    int ret = moCrypt_DES3_ECB(MOCRYPT_METHOD_DECRYPT, (unsigned char *)pCipher,
        (unsigned int)len, (unsigned char *)cryptInfo.cryptKey.desKey, MOCRYPT_DES_KEYLEN,
        (unsigned char *)pPlain, &plainLen);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES3_ECB failed! ret = %d\n", ret);
        return -2;
    }
#else
    memcpy(pPlain, pCipher, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES3_ECB succeed.\n");
    return 0;
}

static int doDecryptWithAes(char * pCipher, const int len, char * pPlain, const MOCPS_CRYPT_INFO cryptInfo)
{
    memcpy(pPlain, pCipher, len);
    return 0;
}

static int doDecryptWithRc4(char * pCipher, const int len, char * pPlain, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(NULL == pCipher || NULL == pPlain || 0 == len)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
#if 0
    strncpy(pPlain, pCipher, len);
    int ret = moCrypt_RC4_cryptString((unsigned char *)cryptInfo.cryptKey.rc4Key,
        (unsigned int)len, (unsigned char *)pPlain, len);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_RC4_cryptString failed! ret = %d\n", ret);
        return -2;
    }
#else
    memcpy(pPlain, pCipher, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_RC4_cryptString succeed.\n");
    return 0;
}

static int doEncryptWithDes(char * pPlain, const int len, char * pCipher, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(NULL == pCipher || NULL == pPlain || 0 == len)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }

#if 0
    unsigned int cipherLen = 0;
    int ret = moCrypt_DES_ECB(MOCRYPT_METHOD_ENCRYPT, (unsigned char *)pPlain,
        (unsigned int)len, (unsigned char *)cryptInfo.cryptKey.desKey, MOCRYPT_DES_KEYLEN,
        (unsigned char *)pCipher, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES_ECB failed! ret = %d\n", ret);
        return -2;
    }
#else
    memcpy(pCipher, pPlain, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES_ECB succeed.\n");
    return 0;
}

static int doEncryptWithDes3(char * pPlain, const int len, char * pCipher, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(NULL == pCipher || NULL == pPlain || 0 == len)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
#if 0
    unsigned int cipherLen = 0;
    int ret = moCrypt_DES3_ECB(MOCRYPT_METHOD_ENCRYPT, (unsigned char *)pPlain,
        (unsigned int)len, (unsigned char *)cryptInfo.cryptKey.desKey, MOCRYPT_DES_KEYLEN,
        (unsigned char *)pCipher, &cipherLen);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES3_ECB failed! ret = %d\n", ret);
        return -2;
    }
#else
    memcpy(pCipher, pPlain, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_DES3_ECB succeed.\n");
    return 0;
}

static int doEncryptWithAes(char * pPlain, const int len, char * pCipher, const MOCPS_CRYPT_INFO cryptInfo)
{
    memcpy(pCipher, pPlain, len);
    return 0;
}

static int doEncryptWithRc4(char * pPlain, const int len, char * pCipher, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(NULL == pCipher || NULL == pPlain || 0 == len)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
#if 0
    strncpy(pCipher, pPlain, len);
    int ret = moCrypt_RC4_cryptString((unsigned char *)cryptInfo.cryptKey.rc4Key,
        (unsigned int)len, (unsigned char *)pCipher, len);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "moCrypt_RC4_cryptString failed! ret = %d\n", ret);
        return -2;
    }
#else
    memcpy(pCipher, pPlain, len);
#endif
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "moCrypt_RC4_cryptString succeed.\n");
    return 0;
}


/*
    Do decrypt with defined crypt algo.
*/
static int doDecrypt(char * pCipher, const int len, char * pPlain, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(NULL == pCipher || NULL == pPlain || 0 == len)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "crypt algo. id = %d\n", cryptInfo.cryptAlgoNo);

    int ret = 0;
    switch(cryptInfo.cryptAlgoNo)
    {
        case MOCPS_CRYPT_ALGO_DES:
            ret = doDecryptWithDes(pCipher, len, pPlain, cryptInfo);
            break;
        case MOCPS_CRYPT_ALGO_DES3:
            ret = doDecryptWithDes3(pCipher, len, pPlain, cryptInfo);
            break;
        case MOCPS_CRYPT_ALGO_AES:
            ret = doDecryptWithAes(pCipher, len, pPlain, cryptInfo);
            break;
        case MOCPS_CRYPT_ALGO_RC4:
            ret = doDecryptWithRc4(pCipher, len, pPlain, cryptInfo);
            break;
        default:
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "crypt algo no = %d, invalid!\n", cryptInfo.cryptAlgoNo);
            ret = -2;
            break;
    }
    
    return ret;
}

static int doEncrypt(char * pPlain, const int len, char * pCipher, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(NULL == pCipher || NULL == pPlain || 0 == len)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL.\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "crypt algo. id = %d\n", cryptInfo.cryptAlgoNo);

    int ret = 0;
    switch(cryptInfo.cryptAlgoNo)
    {
        case MOCPS_CRYPT_ALGO_DES:
            ret = doEncryptWithDes(pPlain, len, pCipher, cryptInfo);
            break;
        case MOCPS_CRYPT_ALGO_DES3:
            ret = doEncryptWithDes3(pPlain, len, pCipher, cryptInfo);
            break;
        case MOCPS_CRYPT_ALGO_AES:
            ret = doEncryptWithAes(pPlain, len, pCipher, cryptInfo);
            break;
        case MOCPS_CRYPT_ALGO_RC4:
            ret = doEncryptWithRc4(pPlain, len, pCipher, cryptInfo);
            break;
        default:
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "crypt algo no = %d, invalid!\n", cryptInfo.cryptAlgoNo);
            ret = -2;
            break;
    }
    
    return ret;
}

static int doRequestHeartbeat(const int ctrlSockId)
{
    int ret = cliMgrRefreshHeartbeat(ctrlSockId);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cliMgrRefreshHeartbeat failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "cliMgrRefreshHeartbeat succeed.\n");
    return 0;
}

/*
    In cliMgrModule, delete this client;
    In CriModule, delete this client;
*/
static int doRequestByebye(const int ctrlSockId)
{
    int ret = criDeleteCliInfo(ctrlSockId);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "criDeleteCliInfo failed! ret = %d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "criDeleteCliInfo succeed.\n");

    return 0;
}

static int doRequestSendDataport(const int ctrlSockId, const MOCPS_CTRL_REQUEST req, 
    const MOCPS_CRYPT_INFO cryptInfo)
{
    int dataport = 0;
    int ret = mergeChar2Int(req.basicInfo.res, &dataport);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "mergeChar2Int failed! ret = %d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "mergeChar2Int succeed.\n");

    ret = criSetDataSockPort(ctrlSockId, dataport);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "criSetDataSockPort failed! ret=%d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "criSetDataSockPort succeed.\n");

    //send a response to client
    MOCPS_CTRL_RESPONSE_BASIC respBasic;
    memset(&respBasic, 0x00, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
    respBasic.bodyLen = 0;
    respBasic.cmdId = MOCPS_CMDID_SEND_DATAPORT;
    strcpy(respBasic.mark, MOCPS_MARK_SERVER);
    moUtils_Check_getCrc((char *)&respBasic, sizeof(MOCPS_CTRL_RESPONSE_BASIC) - sizeof(MOUTILS_CHECK_CRCVALUE),
        MOUTILS_CHECK_CRCMETHOD_32, &respBasic.crc32);
       
    MOCPS_CTRL_RESPONSE_BASIC cipher;
    memset(&cipher, 0x00, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
    doEncrypt((char *)&respBasic, sizeof(MOCPS_CTRL_RESPONSE_BASIC), (char *)&cipher, cryptInfo);
    
    int writeLen = writen(ctrlSockId, (char *)&cipher, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
    if(writeLen != sizeof(MOCPS_CTRL_RESPONSE_BASIC))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "writen failed! writeLen=%d, len=%d\n",
            writeLen, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "send response basic info succeed.\n");
    
    return 0;
}

static int doRequestGetFileinfo(const int ctrlSockId, const MOCPS_CRYPT_INFO cryptInfo)
{
    int len = 0;
    int ret = fmGetFileinfoLength(&len);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "fmGetFileinfoLength failed! ret = %d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "fmGetFileinfoLength succeed. len = %d\n", len);
    
    char * pBody = NULL;
    if(len > 0)
    {
        while(1)
        {    
            pBody = (char *)malloc(sizeof(char) * len);
            if(NULL == pBody)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc failed! errno=%d, desc=[%s]\n",
                    errno, strerror(errno));
                return -2;
            }
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Malloc succeed.\n");
        
            ret = fmGetFileinfo(pBody, len);
            if(ret < 0) //failed, cannot recovery, return error
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "fmGetFileinfo failed! ret = %d\n", ret);
                free(pBody);
                pBody = NULL;
                return -3;
            }
            else if(ret == 0)
            {
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "fmGetFileinfo succeed!\n");
                break;
            }
            else    //body length changed, get body again
            {
                moLoggerWarn(MOCPS_MODULE_LOGGER_NAME, "fmGetFileinfo find length changed! get it again!\n");
                free(pBody);
                pBody = NULL;
                len = ret;
                continue;
            }
        }
    }
    
    MOCPS_CTRL_RESPONSE_BASIC respBasic;
    memset(&respBasic, 0x00, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
    respBasic.cmdId = MOCPS_CMDID_GETFILEINFO;
    strcpy(respBasic.mark, MOCPS_MARK_SERVER);
    respBasic.bodyLen = len;
    moUtils_Check_getCrc((char *)&respBasic, 
        sizeof(MOCPS_CTRL_RESPONSE_BASIC) - sizeof(MOUTILS_CHECK_CRCVALUE),
        MOUTILS_CHECK_CRCMETHOD_32, &respBasic.crc32);
    
    MOCPS_CTRL_RESPONSE_BASIC cipher;
    memset(&cipher, 0x00, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
    doEncrypt((char *)&respBasic, sizeof(MOCPS_CTRL_RESPONSE_BASIC), (char *)&cipher, cryptInfo);
    
    int writeLen = writen(ctrlSockId, (char *)&cipher, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
    if(writeLen != sizeof(MOCPS_CTRL_RESPONSE_BASIC))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "writen failed! writeLen=%d, len=%d\n",
            writeLen, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "send response basic info succeed.\n");

    //encrypt to body
    if(pBody != NULL)
    {
        char * pCipherBody = NULL;
        pCipherBody = (char *)malloc(sizeof(char) * len);
        if(NULL == pCipherBody)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc failed! errno=%d, desc=[%s]\n",
                errno, strerror(errno));
            free(pBody);
            pBody = NULL;
            return -5;
        }
        doEncrypt(pBody, len, pCipherBody, cryptInfo);
        
        writeLen = writen(ctrlSockId, pCipherBody, len);
        if(writeLen != len)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "writen failed! writeLen=%d, len=%d\n",
                writeLen, sizeof(MOCPS_CTRL_RESPONSE_BASIC));
            return -5;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "send fileinfo body info succeed.\n");
        
        free(pCipherBody);
        pCipherBody = NULL;
        free(pBody);
        pBody = NULL;
    }

    return 0;
}

/*
    1.parse request;
    2.do request;
        2.1.do encrypt;
        2.2.send response if needed;
*/
static int doRequest(const int ctrlSockId, const MOCPS_CTRL_REQUEST req, const MOCPS_CRYPT_INFO cryptInfo)
{
    if(req.basicInfo.cmdId >= MOCPS_CMDID_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cmdid=%d, larger than MOCPS_CMDID_MAX(%d), invalid!\n",
            req.basicInfo.cmdId, MOCPS_CMDID_MAX);
        return -1;
    }

    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "cmdId = %d\n", req.basicInfo.cmdId);
    int ret = 0;
    switch(req.basicInfo.cmdId)
    {
        case MOCPS_CMDID_HEARTBEAT:
            ret = doRequestHeartbeat(ctrlSockId);
            break;
        case MOCPS_CMDID_BYEBYE:
            ret = doRequestByebye(ctrlSockId);
            break;
        case MOCPS_CMDID_SEND_DATAPORT:
            ret = doRequestSendDataport(ctrlSockId, req, cryptInfo);
            break;
        case MOCPS_CMDID_GETFILEINFO:
            ret = doRequestGetFileinfo(ctrlSockId, cryptInfo);
            break;
        case MOCPS_CMDID_GETDATA:
            ret = 0;    //TODO
            break;
        default:
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cmdId=%d, invalid value!\n",
                req.basicInfo.cmdId);
            ret = -2;
            break;
    }
    return ret;
}

/*
    1.recv cipher text; 
    2.decrypt; 
    3.check crc; 
    4.do this request; 
    5.if needed, send response;
*/
static int doSimpleRequest(const int ctrlSockId, char *recvBuf, int recvLen)
{
    if(NULL == recvBuf)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    //get its crypt info
    MOCPS_CRYPT_INFO cryptInfo;
    memset(&cryptInfo, 0x00, sizeof(MOCPS_CRYPT_INFO));
    int ret = criGetCryptInfo(ctrlSockId, &cryptInfo);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "getCryptInfo failed! ret = %d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "getCryptInfo succeed.\n");

    //read buffer
    MOCPS_CTRL_REQUEST req;
    memset(&req, 0x00, sizeof(MOCPS_CTRL_REQUEST));
    int readLen = readn(ctrlSockId, (char *)&req, sizeof(MOCPS_CTRL_REQUEST));
    if(readLen != recvLen)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "readn failed! readLen = %d, recvLen = %d\n",
            readLen, recvLen);
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "readn succeed.\n");

    //decrypt
    ret = doDecrypt((char *)&req, sizeof(MOCPS_CTRL_REQUEST), recvBuf, cryptInfo);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doDecrypt failed! ret=%d\n", ret);
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doDecrypt succeed.\n");

    //Find MARK
    ret = moUtils_Search_BF((unsigned char *)recvBuf, recvLen, 
        (unsigned char *)MOCPS_MARK_CLIENT, strlen(MOCPS_MARK_CLIENT));
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Find MARK failed! ret=%d\n", ret);
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find MARK ok, pos=%d.\n", ret);

    //MARK donot at the beginning of text, should recv left text now
    if(ret != 0)
    {
        memmove(recvBuf, recvBuf + ret, recvLen - ret);
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "we should recv left text now.\n");
        readLen = readn(ctrlSockId, (char *)&req, ret);
        if(readLen != ret)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "read left text failed!readLen=%d, leftLen=%d\n",
                readLen, ret);
            return -6;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "read left text succeed.\n");

        //decrypt these left context
        ret = doDecrypt((char *)&req, readLen, recvBuf + recvLen - ret, cryptInfo);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doDecrypt to left context failed! ret = %d\n", 
                ret);
            return -7;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doDecrypt to left context succeed.\n");
    }

    //Set recvBuf to request structure
    memcpy(&req, recvBuf, recvLen);

    //check its format
    if(!isRightFormatRequest(req))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "The request donot in right format!\n");
        return -8;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "format check succeed.\n");

    //do this request : get its response, then send it back
    ret = doRequest(ctrlSockId, req, cryptInfo);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doRequest failed! ret=%d\n", ret);
        return -9;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doRequest succeed.\n");

    return 0;
}

/*
    Wait for request;
    decrypt request;
    do it;
*/
static void * doCtrlRequestThr(void * args)
{
    int * tmp = (int *)args;
    int ctrlSockId = *tmp;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "ctrlSockId = %d\n", ctrlSockId);

    sigset_t set;
    int ret = threadRegisterSignal(&set);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "threadRegisterSignal failed! ret=%d\n", ret);
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "threadRegisterSignal succeed.\n");

    MOCPS_CTRL_REQUEST recvReq;
    memset(&recvReq, 0x00, sizeof(MOCPS_CTRL_REQUEST));
    char * recvBuf = (char *)&recvReq;
    int recvLen = sizeof(MOCPS_CTRL_REQUEST);

    struct timespec tmInterval;
    tmInterval.tv_sec = THREAD_TIME_INTEVAL;
    tmInterval.tv_nsec = 0;  
    struct timespec tmInterval1;
    tmInterval1.tv_sec = 0;
    tmInterval1.tv_nsec = 0;  
    
    while(1)
    {
//        int ret = sigtimedwait(&set, NULL, &tmInterval);
//        if(ret < 0)
//        {
//            if(errno == EAGAIN)
//            {
//                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "In timeInterval, donot recv any signal. " \
//                    "And errno=%d, means timeout, donot an error.\n", errno);
//            }
//            else
//            {
//                moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
//                    "sigtimedwait failed, and errno=%d(%s), thread will exit now.\n",
//                    errno, strerror(errno));
//                break;
//            }
//        }
//        else
//        {
//            if(ret == MOCPS_STOP_THR_SIG)
//            {
//                moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "recv stop signal! thread should exit now!\n");
//                break;
//            }
//            else
//            {
//                //cannot running to this branch
//                moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "recv a signal = %d, will not deal with it!\n",
//                    ret);
//            }
//        }
    
        fd_set rFds;
        FD_ZERO(&rFds);
        FD_SET(ctrlSockId, &rFds);
//        struct timeval tm;
//        tm.tv_sec = THREAD_TIME_INTEVAL;
//        tm.tv_usec = 0;
        int ret = pselect(ctrlSockId + 1, &rFds, NULL, NULL, &tmInterval, &set);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "pselect failed! ret=%d, errno=%d, desc=[%s]\n",
                ret, errno, strerror(errno));
            if(errno == EINTR)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "errno==EINTR, we should exit this ctrl thread now.\n");
                break;
//                moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "errno==EINTR, should check the signal now!\n");
//                ret = sigtimedwait(&set, NULL, &tmInterval1);
//                printf("wjl_test : sigtimedwait ret = %d, MOCPS_STOP_THR_SIG=%d, SIGALRM=%d, errno=%d, desc=[%s]\n",
//                    ret, MOCPS_STOP_THR_SIG, SIGALRM, errno, strerror(errno));
//                if(ret == MOCPS_STOP_THR_SIG)
//                {
//                    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Recv the exit signal, Thread will exit now!\n");
//                    break;
//                }
            }
            continue;
        }
        else if(ret == 0)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "select timeout.\n");            
            continue;
        }
        else
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Recv a request!\n");
            //get state, if state less than KEYAGREED, means this request must be KEYAGEED, and 
            //in plain text, cannot in cipher text
            CLIMGR_CLI_STATE state = 0;
            ret = cliMgrGetState(ctrlSockId, &state);
            if(ret < 0)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cliMgrGetState failed! ret=%d\n", ret);
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "This client donot in our control, this thread should exit!\n");
//                break;
                continue;
            }
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "ctrlSockId=%d, state=%d\n", 
                ctrlSockId, state);

            if(state < CLIMGR_CLI_STATE_KEYAGREED)
            {
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Will do keyAgree now.\n");
                ret = doKeyAgree(ctrlSockId, recvBuf, recvLen);
                if(ret < 0)
                {
                    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doKeyAgree failed! ret = %d\n", ret);
                    moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                        "This client cannot transport with Server succeed, should delete it.\n");
                    continue;
//                    break;
                }
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doKeyAgree succeed.\n");
                cliMgrSetState(ctrlSockId, CLIMGR_CLI_STATE_KEYAGREED);
                continue;
            }
            else
            {
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Will do simple request now.\n");
                ret = doSimpleRequest(ctrlSockId, recvBuf, recvLen);
                if(ret < 0)
                {
                    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "doSimpleRequest failed! ret = %d\n", ret);
                    continue;
                }
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "doSimpleRequest succeed.\n");
                continue;
            }
        }
    }
    
    pthread_exit(NULL);
}

static int startCtrlThread(const int sockId, pthread_t *thId)
{
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "sockId = %d\n", sockId);

    int ret = pthread_create(thId, NULL, doCtrlRequestThr, (void *)&sockId);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "create thread failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "create thread succeed.\n");
    return 0;
}

/*
    accept new client;
    set to local memory;
    start threads for them;
*/
static void * doAcceptThr(void * args)
{
    args = args;
    int ret = 0;
    while(1)
    {
        fd_set rFds;
        FD_ZERO(&rFds);
        FD_SET(gSockId, &rFds);

        struct timeval tm;
        tm.tv_sec = THREAD_TIME_INTEVAL;
        tm.tv_usec = 0;

        ret = select(gSockId + 1, &rFds, NULL, NULL, &tm);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "select failed! ret=%d, errno=%d, desc=[%s]\n",
                ret, errno, strerror(errno));
            break;
        }
        else if(ret == 0)
        {
            continue;
        }
        else
        {
            //Receive a new client which want to connect
            struct sockaddr_in clientAddr;
            memset(&clientAddr, 0x00, sizeof(struct sockaddr_in));
            socklen_t len = sizeof(struct sockaddr_in);
            int cliSockId = accept(gSockId, (struct sockaddr *)&clientAddr, &len);
            if(cliSockId < 0)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "accept failed! errno = %d, desc = [%s]\n", 
                    errno, strerror(errno));
                continue;
            }
            else
            {
                char ip[MOCPS_IP_ADDR_MAXLEN] = {0x00};
                memset(ip, 0x00, MOCPS_IP_ADDR_MAXLEN);
                strcpy(ip, inet_ntoa(clientAddr.sin_addr));
                int port = ntohs(clientAddr.sin_port);
                moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                    "accept a new client connect request. client ip = [%s], port = [%d], socketid = %d.\n", 
                    ip, port, cliSockId);

                if(criIsCliExist(clientAddr.sin_addr.s_addr))
                {
                    //If this IP address has been exist, means this is a data socket
                    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                        "This is data port connect request from [%s]\n",
                        ip);
                    ret = criSetDataSocketInfo(clientAddr.sin_addr.s_addr, cliSockId, port);
                    if(ret < 0)
                        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "criSetDataportInfo failed! " \
                            "ret = %d, ip = [%s]\n", ret, ip);
                    else
                        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "criSetDataportInfo succeed!\n");
                    
                    continue;
                }
                else
                {
                    //This is the ctrl port for a new client
                    ret = criInsertNewCliInfo(ip, cliSockId, port);
                    if(ret < 0)
                        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "criInsertNewCliInfo failed! " \
                            "ret = %d, ip = [%s]\n", ret, ip);
                    else
                        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "criInsertNewCliInfo succeed!\n");

                    criDump();

                    ret = cliMgrInsertNewCli(ip, port, cliSockId);
                    if(ret < 0)
                    {
                        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "cliMgrInsertNewCli failed! " \
                            "ret = %d, ip = [%s]\n", ret, ip);
                        criDeleteCliInfo(cliSockId);
                    }
                    else
                        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "cliMgrInsertNewCli succeed!\n");

                    pthread_t thId;
                    ret = startCtrlThread(cliSockId, &thId);
                    if(ret < 0)
                    {
                        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "startThread failed! ret = %d\n", ret);
                        cliMgrDeleteCli(cliSockId);
                        criDeleteCliInfo(cliSockId);
                    }
                    else
                    {
                        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "startThread succeed.\n");                
                        criRefreshCtrlThrInfo(cliSockId, thId);
                    }

                    continue;
                }
            }
        }
    }
    return NULL;
}

/*
    Start running;
    start recv request from clients, and deal with them;
    return 0 if succeed.

    accept;
    start ctrl thread to recv requests;
*/
int moCpsServ_startRunning()
{
    if(gSockId < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gSockId=%d, donot init it! cannot start service!\n",
            gSockId);
        return -1;
    }

    pthread_t thId = 0;
    int ret = pthread_create(&thId, NULL, doAcceptThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "create thread failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "create accept thread succeed!\n");
    return 0;
}

/*
    Stop running;
    after this function, all requests from clients, will not be done;
    return 0 if succeed.
*/
int moCpsServ_stopRunning()
{
    //TODO
    return 0;
}

/*
    Dump all resources in moCpsServer;
*/
void moCpsServ_dump()
{
    criDump();
    cliMgrDump();
    fmDump();
}


