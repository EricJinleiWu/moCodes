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
#include "keyAgree.h"

#include "moUtils.h"
#include "moLogger.h"
#include "moCrypt.h"


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

/*
    Connect to server;

    1.create socket;
    2.do connect;
        2.1.If in timeout, donot recv server response, failed;
        2.2.recv server response, succeed.
*/
static int connectToServer(const IP_ADDR_INFO servInfo)
{
    //TODO, do it.
    return 0;
}

/*
    A thread, to recv the response from server, then parse it, deal with it.
*/
stativ void * recvRespThr(void * args)
{
    //TODO, do it
}

//start thread recvRespThr, this thread to recv all response from server;
static int startRecvRespThr()
{
    pthread_t thId;
    int ret = pthread_create(&thId, NULL, );
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

    //2 Connect to server, TODO, this function should set gSockId value;
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
    ret = startRecvRespThr();
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "startRecvRespThr failed! ret = %d\n", 
            ret);
        disConnectToServer();   //send byebye to server
        confParserUnInit();
        pthread_mutex_unlock(&gLock);
        return MONVRCLI_ERR_INTERNAL;
    }
    moLoggerDebug(MO_NVR_CLIENT_LOG_MODULE, "startRecvRespThr succeed.\n");
    
    //4 start a thread to send heartbeat;
    ret = startHeartbeatThr();
    if(ret != 0)
    {
        moLoggerError(MO_NVR_CLIENT_LOG_MODULE, "startHeartbeatThr failed! ret = %d\n", 
            ret);
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

