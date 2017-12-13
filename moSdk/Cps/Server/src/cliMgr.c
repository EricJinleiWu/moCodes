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

#include "cliMgr.h"
#include "moLogger.h"
#include "moCpsUtils.h"

typedef enum
{
    THR_RUNSTATE_RUNNING,
    THR_RUNSTATE_STOPED
}THR_RUNSTATE;

static CLIMGR_CLIINFO_NODE * gpCliInfoListHead = NULL;
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static int gCheckHeartbeatThrRunning = 0;
static THR_RUNSTATE gCheckHeartbeatThrState = THR_RUNSTATE_STOPED;

static void * checkHeartbeatThr(void *args)
{
    args = args;
    while(gCheckHeartbeatThrRunning)
    {   
        struct timeval tm;
        tm.tv_sec = MOCPS_HEARTBEAT_INTEVAL;
        tm.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tm);

        pthread_mutex_lock(&gMutex);

        CLIMGR_CLIINFO_NODE * pPreNode = gpCliInfoListHead;
        CLIMGR_CLIINFO_NODE * pCurNode = gpCliInfoListHead->next;
        while(pCurNode != NULL)
        {
            if(pCurNode->info.state == CLIMGR_CLI_STATE_IDLE || 
                pCurNode->info.state == CLIMGR_CLI_STATE_CTRLSOCKIN|| 
                pCurNode->info.state == CLIMGR_CLI_STATE_KEYAGREED)
            {
                pPreNode = pCurNode;
                pCurNode = pCurNode->next;
            }
            else if(pCurNode->info.state == CLIMGR_CLI_STATE_HEARTBEAT)
            {
                clock_t curTime = clock();
                if((curTime - pCurNode->info.lastHeartbeatTime) / CLOCKS_PER_SEC > 
                    MOCPS_HEARTBEAT_INTEVAL * 2 + 1)    //We think this means heartbeat timeout!
                {
                    moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "This client heartbeat timeout! " \
                        "ip=[%s], lastHeartbeatTime=%ld, curTime=%ld, timeInterval=%ld\n",
                        pCurNode->info.addr, pCurNode->info.lastHeartbeatTime, curTime, 
                        (curTime - pCurNode->info.lastHeartbeatTime) / CLOCKS_PER_SEC);
                    pCurNode->info.state = CLIMGR_CLI_STATE_STOPPED;
                }

                pPreNode = pCurNode;
                pCurNode = pCurNode->next;
            }
            else
            {
                //should delete this client
                pPreNode->next = pCurNode->next;
                
                free(pCurNode);
                pCurNode = NULL;

                pCurNode = pPreNode->next;
                continue;
            }
        }
        
        gCheckHeartbeatThrState = THR_RUNSTATE_STOPED;
        pthread_mutex_unlock(&gMutex);
    }
    return NULL;
}

static int startCheckHeartbeatThr()
{
    gCheckHeartbeatThrRunning = 1;
    
    pthread_t thId;
    int ret = pthread_create(&thId, NULL, checkHeartbeatThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "create heartbeat thread failed! " \
            "ret=%d, errno=%d, desc=[%s]\n", ret, errno, strerror(errno));
        gCheckHeartbeatThrRunning = 0;
        return -1;
    }
    gCheckHeartbeatThrState = THR_RUNSTATE_RUNNING;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "create heartbeat thread succeed.\n");
    
    return 0;
}

static void stopCheckHeartbeatThr()
{
    gCheckHeartbeatThrRunning = 0;
    while(gCheckHeartbeatThrState == THR_RUNSTATE_RUNNING)
    {
        struct timeval tm;
        tm.tv_sec = MOCPS_HEARTBEAT_INTEVAL;
        tm.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tm);
    }
}

/*
    Do init;
    Generate a list to save all client info;
    create a thread to check all clients' heartbeat;
*/
int cliMgrInit()
{
    if(gpCliInfoListHead != NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "gpCliInfoListHead not NULL, CLIMGR being inited! cannot init again!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "start init now.\n");

    gpCliInfoListHead = (CLIMGR_CLIINFO_NODE *)malloc(1 * sizeof(CLIMGR_CLIINFO_NODE));
    if(gpCliInfoListHead == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc for header succeed.\n");
    memset(&gpCliInfoListHead->info, 0x00, sizeof(CLIMGR_CLIINFO));
    gpCliInfoListHead->next = NULL;

    int ret = startCheckHeartbeatThr();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "startCheckHeartbeatThr failed! ret=%d\n", ret);
        free(gpCliInfoListHead);
        gpCliInfoListHead = NULL;
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "startCheckHeartbeatThr succeed!\n");
    
    return 0;
}

/*
    Do uninit;
*/
void cliMgrUnInit()
{
    stopCheckHeartbeatThr();
    
    CLIMGR_CLIINFO_NODE * pPreNode = gpCliInfoListHead;
    CLIMGR_CLIINFO_NODE * pCurNode = gpCliInfoListHead->next;
    while(pCurNode != NULL)
    {
        pPreNode->next = pCurNode->next;
        free(pCurNode);
        pCurNode = NULL;
        pCurNode = pPreNode->next;
    }
    free(gpCliInfoListHead);
    gpCliInfoListHead = NULL;
}

int cliMgrRefreshHeartbeat(const char * pAddr)
{
    if(NULL == pAddr)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "address = [%s]\n", pAddr);

    in_addr_t value = inet_addr(pAddr);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "address [%s] has id=%u\n", pAddr, value);

    pthread_mutex_lock(&gMutex);
    CLIMGR_CLIINFO_NODE * pCurNode = gpCliInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.id == value)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find this client!\n");
            pCurNode->info.lastHeartbeatTime = clock();
            break;
        }
        pCurNode = pCurNode->next;
    }

    int ret = 0;
    if(NULL == pCurNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot find this node!\n");
        ret = -3;
    }
    
    pthread_mutex_lock(&gMutex);
    return ret;
}

/*
    A new client in, should malloc a node for it, and save in local memory;
*/
int cliMgrInsertNewCli(const char * pAddr, const int ctrlPort, const int ctrlSockId)
{
    if(NULL == pAddr)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "address = [%s]\n", pAddr);

    in_addr_t value = inet_addr(pAddr);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "address [%s] has id=%u\n", pAddr, value);

    pthread_mutex_lock(&gMutex);
    CLIMGR_CLIINFO_NODE * pPreNode = gpCliInfoListHead;
    CLIMGR_CLIINFO_NODE * pCurNode = gpCliInfoListHead->next;
    while(NULL != pCurNode)
    {
        if(pCurNode->info.id == value)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "The client being added, has been exist yet!\n");
            pthread_mutex_unlock(&gMutex);
            return -3;
        }
        pPreNode = pCurNode;
        pCurNode = pCurNode->next;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "This client will be add to list now.\n");

    CLIMGR_CLIINFO_NODE * pNewNode = NULL;
    pNewNode = (CLIMGR_CLIINFO_NODE *)malloc(1 * sizeof(CLIMGR_CLIINFO_NODE));
    if(NULL == pNewNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        pthread_mutex_unlock(&gMutex);
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc for new node OK.\n");
    memset(&pNewNode->info, 0x00, sizeof(CLIMGR_CLIINFO));
    pNewNode->info.id = value;
    strcpy(pNewNode->info.addr, pAddr);
    pNewNode->info.ctrlPort = ctrlPort;
    pNewNode->info.ctrlSockId= ctrlSockId;
    pNewNode->next = NULL;

    pPreNode->next = pNewNode;
    
    pthread_mutex_unlock(&gMutex);
    
    return 0;
}

/*
    Set state, if state set to Heartbeat, should check its timeout;
*/
int cliMgrSetState(const char *pAddr, CLIMGR_CLI_STATE state)
{
    if(NULL == pAddr || state >= CLIMGR_CLI_STATE_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is invalid!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "address=[%s], state=%d\n",
        pAddr, state);

    in_addr_t value = inet_addr(pAddr);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "address [%s] has id=%u\n", pAddr, value);

    pthread_mutex_lock(&gMutex);
    CLIMGR_CLIINFO_NODE * pCurNode = gpCliInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.id == value)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find this client!\n");
            pCurNode->info.state = state;
            break;
        }
        pCurNode = pCurNode->next;
    }

    int ret = 0;
    if(NULL == pCurNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot find this node!\n");
        ret = -3;
    }
    
    pthread_mutex_lock(&gMutex);
    return ret;
}

int cliMgrSetDataPortValue(const char * pAddr, const int dataPort, const int dataSockId)
{
    if(NULL == pAddr)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is invalid!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "address=[%s]\n", pAddr);

    in_addr_t value = inet_addr(pAddr);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "address [%s] has id=%u\n", pAddr, value);

    pthread_mutex_lock(&gMutex);
    CLIMGR_CLIINFO_NODE * pCurNode = gpCliInfoListHead->next;
    while(pCurNode != NULL)
    {
        if(pCurNode->info.id == value)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find this client!\n");
            pCurNode->info.dataPort = dataPort;
            pCurNode->info.dataSockId = dataSockId;
            break;
        }
        pCurNode = pCurNode->next;
    }

    int ret = 0;
    if(NULL == pCurNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot find this node!\n");
        ret = -3;
    }
    
    pthread_mutex_lock(&gMutex);
    return ret;
    
    return 0;
}








