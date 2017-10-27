/*
 * =====================================================================================
 *
 *       Filename:  moNvrClient.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  20171025
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  EricWu 
 *        Company:  
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "moNvrClient.h"
#include "commMsg.h"
#include "confParser.h"

#include "moUtils.h"
#include "moLogger.h"
#include "moCrypt.h"

/*
    To heartbeat thread, we use this inteval, this can help us to stop thread quickly.
*/
#define THREAD_TIME_INTEVAL     1
/*
    In this timeout, if cannot connect to server, we think connect to server failed!
*/
#define CONNECT_TIMEOUT         3

typedef enum
{
    MONVRCLI_STATE_IDLE,
    MONVRCLI_STATE_KEYAGREE,    //has connect to server yet, and key agree being done yet, can do common transport, but cannot play video
    MONVRCLI_STATE_STARTPLAY,   //send request to server to get frame, but donot know, If donot receive frame data in several seconds, should set to keyAgree state
    MONVRCLI_STATE_PLAYING,     //frame data playing now
}MONVRCLI_STATE;

char gIsInited = 0;
int gSockId = -1;
pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;
pMoNvrCliSendFrameCallback gpSendFrameFunc = NULL;
MONVRCLI_STATE gState = MONVRCLI_STATE_IDLE;
char gIsRecvRespThrRunning = 0;
char gIsHeartbeatThrRunning = 0;


/* set socket to non block mode */
static int setSockToNonBlock(const int sockId)
{
    int b_on = 1;
    int ret = ioctl(sockId, FIONBIO, &b_on);
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, 
            "ioctl to set socket to nonblock failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "ioctl to set socket to nonblock succeed.\n");
    
    return 0;
}

/*
    Connect to server in timeout;
*/
static int connect2ServerAsync(const int sockId, const IP_ADDR_INFO servInfo, const int timeout)
{
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "sockId=%d, server ip=[%s], server port=%d, timeout=%d\n",
        sockId, servInfo.ip, servInfo.port, timeout);
    
    struct sockaddr_in servAddr;
    memset(&servAddr, 0x00, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servInfo.ip);
    servAddr.sin_port = htons(servInfo.port);
    int ret = connect(gSockId, (struct sockaddr *)&servAddr, sizeof(struct sockaddr));
    if(0 == ret)
    {
        moLoggerInfo(MO_NVR_CLIENT_LOG_MODULE, "Connect succeed once!\n");
    }
    else
    {
        if(errno == EINPROGRESS)
        {
            moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "Socket being blocked yet, we have no ret.\n");
            //Do select here.
            fd_set wFdSet;
            FD_ZERO(&wFdSet);
            FD_SET(gSockId, &wFdSet);
            fd_set rFdSet;
            FD_ZERO(&rFdSet);
            FD_SET(gSockId, &rFdSet);
            struct timeval tm;
            tm.tv_sec = timeout;
            tm.tv_usec = 0;
            ret = select(gSockId + 1, &rFdSet, &wFdSet, NULL, &timeout);
            if(ret < 0)
            {
                moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "when connect, select failed! ret = %d, errno = %d, desc = [%s]\n", 
                    ret, errno, strerror(errno));
                return -1;
            }
            else if(ret == 0)
            {
                moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "when connect, select timeout!\n");
                return -2;
            }
            else
            {
                //select succeed, but its neccessary to assure its really succeed.
                if(FD_ISSET(gSockId, &wFdSet))
                {
                    if(FD_ISSET(gSockId, &rFdSet))
                    {
                        moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, 
                            "select ok, but wFdSet and rFdSet all being set, we should check!\n");
                        //check this socket is OK or not.
                        int err = 0;
                        socklen_t errLen;
                        ret = getsockopt(gSockId, SOL_SOCKET, SO_ERROR, (void *)&err, &errLen);
                        if(0 != ret)
                        {
                            moLoggerError(MO_NVR_CLIENT_LOG_MODULE, 
                                "getsockopt failed! ret = %d, errno = %d, desc = [%s]\n", 
                                ret, errno, strerror(errno));
                            return -3;
                        }
                        else
                        {
                            if(err != 0)
                            {
                                moLoggerError(MO_NVR_CLIENT_LOG_MODULE, 
                                    "getsockopt ok, but err = %d, donot mean OK, just mean some error!\n",
                                    err);
                                return -4;
                            }
                            else
                            {
                                moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, 
                                    "getsockopt ok, and err = %d, means connect succeed!\n", err);
                            }
                        }
                    }
                    else
                    {
                        moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, 
                            "select ok, and just wFdSet being set, means connect being OK.\n");
                    }
                }
                else
                {
                    //Just this socket being set to wFdSet, if not this socket, error ocurred!
                    moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "select ok, but the fd is not client socket!\n");
                    return -5;
                }
            }
        }
        else
        {
            moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Connect failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            return -5;
        }
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "Connect succeed now!\n");
    
    return 0;
}

/*
    Connect to server;

    1.create socket;
    2.do connect;
*/
static int connectToServer(const IP_ADDR_INFO servInfo)
{
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "Input server : ip = [%s], port = [%d]\n",
        servInfo.ip, servInfo.port);

    //create socket
    gSockId = socket(AF_INET, SOCK_STREAM, 0);
    if(gSockId < 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Create socket failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "Create socket succeed. sockId = %d.\n", gSockId);

    //set socket to reuseable
    int on = 1;
    int ret = setsockopt(gSockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret < 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Set socket to reuseable failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(gSockId);
        gSockId = -1;
        return -2;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "set socket to reuseable succeed.\n");

    //set socket to async
    ret = setSockToNonBlock(gSockId);
    if(ret < 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Set socket to nonblock failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(gSockId);
        gSockId = -1;
        return -3;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "set socket to non block succeed.\n");

    //connect to server in timeout
    ret = connect2ServerAsync(gSockId, servInfo, CONNECT_TIMEOUT);
    if(ret < 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Connect to server failed! ret = %d\n",
            ret);
        close(gSockId);
        gSockId = -1;
        return -4;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "connect to server(ip[%s]port[%d]) succeed.\n",
        servInfo.ip, servInfo.port);
    
    return 0;
}

/*
    disconnect to server;
    just send a request "byebye" to server, donot care its response;
*/
static void disConnectToServer()
{
    commmsgSendStopReq(gSockId);
}

/*
    A thread, to recv the response from server, then parse it, deal with it.
*/
static void * recvRespThr(void * args)
{
    args = args;

    while(1)
    {
        fd_set rFds;
        FD_ZERO(&rFds);
        FD_SET(gSockId, &rFds);
        
        struct timeval tm;
        tm.tv_sec = THREAD_TIME_INTEVAL;
        tm.tv_usec = 0;

        int ret = select(gSockId + 1, &rFds, NULL, NULL, &tm);
        if(ret < 0) //select failed
        {
            moLoggerError(MO_NVR_CLIENT_LOG_MODULE, 
                "select failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            continue;   //just continue, donot exit now.
        }
        else if(ret = 0)    //select timeout
        {
            moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, 
                "select timeout, means donot recv response from server.\n");
            continue;
        }
        else
        {
            moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "recv a response now.\n");
            if(FD_ISSET(gSockId, &rFds))
            {
                
            }
        }
    }

    return NULL;
}

//start thread recvRespThr, this thread to recv all response from server;
static int startRecvRespThr()
{
    pthread_t thId;
    int ret = pthread_create(&thId, NULL, recvRespThr, NULL);
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Create thread recvRespThr failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "Create thread recvRespThr succeed. thId = %ld\n", thId);
    return 0;
}

static void stopRecvRespThr()
{
    //TODO, do it.
}

/*
    This thread should send heartbeat to server looply;
*/
static void * heartbeatThr(void * args)
{
    args = args;
    //TODO, do it.

    return NULL;
}

//start thread recvRespThr, this thread to recv all response from server;
static int startHeartbeatThr()
{
    pthread_t thId;
    int ret = pthread_create(&thId, NULL, heartbeatThr, NULL);
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Create thread heartbeatThr failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "Create thread heartbeatThr succeed. thId = %ld\n", thId);
    return 0;
}

/*
    This to stop heartbeat thread;
*/
static void stopHeartBeatThr()
{
    //TODO, set the heartbeat thread running flag to false

    //sleep 1s is necessary, this can help us to assure heartbeat thread has been stopped
    sleep(1);

    
}

/*
    1.read moNvrServer from config file @pConfFilepath;
    2.Connect to server;
    3.Start thread, receive the response from moNvrServer;
    4.Start thread, send heartbeat to moNvrServer;
    5.Do key agree;
*/
int moNvrCli_init(const char *pConfFilepath, pMoNvrCliSendFrameCallback pFunc)
{
    pthread_mutex_lock(&gLock);
    
    if(gIsInited)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "moNvrClient has been inited! cannot init it again.\n");
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_INIT_AGAIN;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "moNvrClient will do init now.\n");
    
    if(NULL == pConfFilepath || NULL == pFunc)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Input param is NULL.\n");
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_INPUT_NULL;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "config file path is [%s].\n", pConfFilepath);

    //1 Get basic infomation from config file
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "Config file path is [%s]\n", pConfFilepath);
    int ret = confParserInit(pConfFilepath);
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "confParserInit failed! ret = %d, config file path is [%s]\n", 
            ret, pConfFilepath);
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_INVALID_CONFFILE;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "confParserInit succeed.\n");
    
    IP_ADDR_INFO servInfo;
    ret = confParserGetServerInfo(&servInfo);
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "confParserGetServerInfo failed! ret = %d\n", ret);
        confParserUnInit();
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_INVALID_CONFFILE;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "confParserGetServerInfo succeed, server ip = [%s], port = %d.\n",
        servInfo.ip, servInfo.port);

    //2 Connect to server
    ret = connectToServer(servInfo);
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "connectToServer failed! ret = %d, server ip = [%s], port = %d\n", 
            ret, servInfo.ip, servInfo.port);
        confParserUnInit();
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_CONNECT_FAILED;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "connectToServer succeed.\n");

    //3 start a thread to receive all response sent from server
    gIsRecvRespThrRunning = 1£»
    ret = startRecvRespThr();
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "startRecvRespThr failed! ret = %d\n", 
            ret);
        gIsRecvRespThrRunning = 0;
        disConnectToServer();   //send byebye to server
        confParserUnInit();
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_INTERNAL;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "startRecvRespThr succeed.\n");
    
    //4 start a thread to send heartbeat;
    gIsHeartbeatThrRunning = 1;
    ret = startHeartbeatThr();
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "startHeartbeatThr failed! ret = %d\n", 
            ret);
        gIsHeartbeatThrRunning = 0;
        disConnectToServer();   //send byebye to server
        stopRecvRespThr();  //stop thread
        confParserUnInit();
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_INTERNAL;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "startHeartbeatThr succeed.\n");

    //Do keyagree
    ret = commmsgKeyAgree();
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "commmsgKeyAgree failed! ret = %d\n", 
            ret);
        stopHeartBeatThr();
        disConnectToServer();   //send byebye to server
        stopRecvRespThr();  //stop thread
        confParserUnInit();
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_KEYAGREE;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "commmsgKeyAgree succeed.\n");

    gpSendFrameFunc = pFunc;
    gState = MONVRCLI_STATE_KEYAGREE;
    gIsInited = 1;
    pthread_mutex_unlock(&gLock);
    return MONVRCLI_RET_OK;
}


/*
    Do uninit to moNvrClient;
*/
void moNvrCli_unInit()
{
    pthread_mutex_lock(&gLock);
    
    stopHeartBeatThr();
    disConnectToServer();   //send byebye to server
    stopRecvRespThr();  //stop thread
    confParserUnInit();
    
    close(gSockId);
    gSockId = -1;
    gState = MONVRCLI_STATE_IDLE;
    gpSendFrameFunc = NULL;
    gIsInited = 0;

    pthread_mutex_unlock(&gLock);
}

/*
    Send a request to moNvrServer to get frames, and these frames should be send to player to play;
*/
int moNvrCli_startPlay()
{
    pthread_mutex_lock(&gLock);
    
    if(gState != MONVRCLI_STATE_KEYAGREE)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Current state = %d, cannot start play!\n", gState);
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_PLAY_AGAIN;
    }

    gState = MONVRCLI_STATE_STARTPLAY;
    int ret = commmsgSendStartPlayReq();
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "commmsgSendStartPlayReq failed, ret = %d\n", ret);
        gState = MONVRCLI_STATE_KEYAGREE;
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_PLAY_AGAIN;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "commmsgSendStartPlayReq succeed.\n");

    pthread_mutex_unlock(&gLock);
    return MONVRCLI_RET_OK;
}


/*
    Send a request to moNvrServer to stop sending frames;
*/
int moNvrCli_stopPlay()
{
    pthread_mutex_lock(&gLock);

    if(gState != MONVRCLI_STATE_PLAYING || gState != MONVRCLI_STATE_STARTPLAY)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "Current state = %d, cannot stop play!\n", gState);
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_CANNOT_STOPPLAY;
    }

    gState = MONVRCLI_STATE_KEYAGREE;
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "Stop play ok.\n");
    
    pthread_mutex_unlock(&gLock);
    return MONVRCLI_RET_OK;
}

