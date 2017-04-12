#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "cpSocket.h"
#include "taskMgr.h"

using namespace std;


void * recvEventTh(void * pObj)
{
    CpClientSocket * pCpCli = (CpClientSocket *)pObj;
    pCpCli->recvEvent();
    return NULL;
}

CpClientSocket::CpClientSocket() : cpClient()
{
    mpUI = NULL;
    //init socket
    mSockId = socket(AF_UNIX, SOCK_STREAM, 0);
    if(mSockId < 0) //-1
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "Create socket for cpClient failed! mSockId = %d, errno = %d, desc = [%s]\n",
            mSockId, errno, strerror(errno));
        mSockId = MOCFT_INVALID_SOCKID;
    }
    else
    {
        //Connect to server now
        struct sockaddr_un addr;
        memset(&addr, 0x00, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, MOCFT_CP_SOCKET_FILE);
        int ret = connect(mSockId, (struct sockaddr *)&addr, sizeof(addr));
        if(ret < 0)
        {
            moLoggerError(MOCFT_LOGGER_MODULE_NAME, "Connect to cpServer failed!" \
                "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
            close(mSockId);
            mSockId = MOCFT_INVALID_SOCKID;
        }
        else
        {
            moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "Connect to cpServer succeed.\n");

            //Set this socket options
            int on = 1;
            setsockopt(mSockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
            
            int flags = fcntl(mSockId, F_GETFL, 0);
            fcntl(mSockId, F_SETFL, flags | O_NONBLOCK);
        }
    } 
}

CpClientSocket::CpClientSocket(const CpClientSocket & other)
{
    mpUI = other.mpUI;
    mSockId = other.mSockId;
}

CpClientSocket::~CpClientSocket()
{
    mpUI = NULL;

    if(mSockId != MOCFT_INVALID_SOCKID)
    {
        close(mSockId);
        mSockId = MOCFT_INVALID_SOCKID;
    }
}
 
int CpClientSocket::sendReq(const MOCFT_TASKINFO & task)
{
    if(mSockId == MOCFT_INVALID_SOCKID)
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "cpClient socket init failed! cannot send request!\n");
        return -1;
    }

    MOCFT_REQMSG req;
    memset(&req, 0x00, sizeof(MOCFT_REQMSG));
    strcpy(req.mark, MOCFT_MARK);
    strcpy(req.prefix, MOCFT_REQUESTMSG_PREFIX);
    memcpy(&req.task, &task, sizeof(MOCFT_TASKINFO));
    int ret = send(mSockId, (char *)&req, sizeof(MOCFT_REQMSG), 0);
    if(ret < 0)
    {
        if(errno == EWOULDBLOCK)
        {
            moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "send failed! errno = EWOULDBLOCK, this just because noblock socket.\n");
            ret = 0;
        }
        else
        {
            moLoggerError(MOCFT_LOGGER_MODULE_NAME, "send failed! ret = %d, errno = %d, desc = [%s].\n",
                ret, errno, strerror(errno));
            ret = -2;
        }
    }
    else
    {
        moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "send to cpServer a request succeed!\n");
        ret = 0;
    }

    return ret;
}

int CpClientSocket::recvEvent()
{
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    fd_set rFd;

    //recv
    while(1)
    {
        FD_ZERO(&rFd);
        FD_SET(mSockId, &rFd);
        int ret = select(mSockId + 1, &rFd, NULL, NULL, &tv);
        if(ret < 0)
        {
            moLoggerError(MOCFT_LOGGER_MODULE_NAME, "select failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            break;
        }
        else if(ret == 0)
        {
            //timeout, no event from cpServer, do nothing;
            continue;
        }
        else
        {
            //get a message from cpServer, check it
            MOCFT_RESPMSG respMsg;
            memset(&respMsg, 0x00, sizeof(MOCFT_RESPMSG));
            ret = recv(mSockId, (char *)&respMsg, sizeof(MOCFT_RESPMSG), 0);
            if(ret < 0)
            {
                moLoggerError(MOCFT_LOGGER_MODULE_NAME, "recv failed! ret = %d, errno = %d, desc = [%s]\n",
                    ret, errno, strerror(errno));
                break;
            }
            else if(ret != sizeof(MOCFT_RESPMSG))
            {
                moLoggerError(MOCFT_LOGGER_MODULE_NAME, "recv failed! ret = %d, donot equal with %d.\n",
                    ret, sizeof(MOCFT_RESPMSG));
                continue;
            }
            else
            {
                RESP_TYPE type = getRespType(respMsg);
                switch(type)
                {
                case RESP_TYPE_RESULT:
                    if(respMsg.value < 0)
                    {
                        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "respMsg type is RESP_TYPE_RESULT, " \
                            "its value is %d, means crypt failed!\n", respMsg.value);
                        if(mpUI)
                            mpUI->setProgress(respMsg.value);
                    }
                    else
                    {
                        moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "respMsg type is RESP_TYPE_RESULT, " \
                            "its value is %d, means crypt is running!\n", respMsg.value);
                        //donot send to ui, just send progress in this case
                        ;
                    }
                    break;
                case RESP_TYPE_PROGRESS:
                    moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "respMsg type is RESP_TYPE_RESULT, " \
                        "its value is %d\n", respMsg.value);
                    if(mpUI)
                        mpUI->setProgress(respMsg.value);
                    break;
                default:
                    moLoggerError(MOCFT_LOGGER_MODULE_NAME, "respMsg.type = ERROR!\n");
                    break;
                }
            }
        }
    }
    
    return 0;
}

RESP_TYPE CpClientSocket::getRespType(const MOCFT_RESPMSG & respMsg)
{
    RESP_TYPE ret = RESP_TYPE_ERROR;
    if(0 == strcmp(respMsg.mark, MOCFT_MARK))
    {
        if(0 == strcmp(respMsg.prefix, MOCFT_RESPONSEMSG_PREFIX))
        {
            moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "respMsg is a result of doCrypt.\n");
            ret = RESP_TYPE_RESULT;
        }
        else if(0 == strcmp(respMsg.prefix, MOCFT_NOTIFYMSG_PREFIX))
        {
            moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "respMsg is a progress of doCrypt.\n");
            ret = RESP_TYPE_PROGRESS;
        }
        else
        {
            moLoggerError(MOCFT_LOGGER_MODULE_NAME, "respMsg.prefix = [%s], donot valid\n", respMsg.prefix);
        }
    }
    else
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "respMsg.mark = [%s], donot valid\n", respMsg.mark);
    }
    
    return ret;
}

int CpClientSocket::addListener(UI * ui)
{
    if(NULL == mpUI)
    {
        mpUI = ui;
        return 0;
    }
    else
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "Some listener function has been added! cannot add another one!\n");
        return -1;
    }
}

int CpClientSocket::removeListener(UI * ui)
{
    if(mpUI == ui && mpUI != NULL)
    {
        mpUI = NULL;
        return 0;
    }
    else if(mpUI != ui && mpUI != NULL)
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "The listener function donot being registered by this ui!" \
            "Cannot remove it except itself!\n");
        return -1;
    }
    else
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "Donot have listener function being registered! cannot remove it.\n");
        return -2;
    }
}

int CpClientSocket::run()
{
    pthread_t thId = 0;
    int ret = pthread_create(&thId, NULL, recvEventTh, this);
    if(ret < 0)
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "Create thread recvEventTh failed! " \
            "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        return -1;
    }
    else
    {
        moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "Create thread recvEventTh succeeded.\n");
        return 0;
    }
}

static int cpServerSockId = MOCFT_INVALID_SOCKID;
static int cpServerCliSockId = MOCFT_INVALID_SOCKID;
static CPSERV_SOCK_STATE cpServerState = CPSERV_SOCK_STATE_IDLE;
static int cpServerProgress = 0;
static sem_t cpServerSem;

int cpServerInit()
{
    cpServerProgress = 0;
    cpServerSockId = MOCFT_INVALID_SOCKID;
    cpServerCliSockId= MOCFT_INVALID_SOCKID;
    cpServerState = CPSERV_SOCK_STATE_IDLE;

    //init socket
    cpServerSockId = socket(AF_UNIX, SOCK_STREAM, 0);
    if(cpServerSockId < 0)
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "Create socket failed! mSockId = %d, " \
            "errno = %d, desc = [%s]\n", cpServerSockId, errno, strerror(errno));
        cpServerSockId = MOCFT_INVALID_SOCKID;
    }
    else
    {
        //Should delete the file if exist
        unlink(MOCFT_CP_SOCKET_FILE);
    
        //bind to local file
        struct sockaddr_un addr;
        memset(&addr, 0x00, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, MOCFT_CP_SOCKET_FILE);
        int addrLen = sizeof(struct sockaddr_un);
        int ret = bind(cpServerSockId, (struct sockaddr *)&addr, addrLen);
        if(ret < 0)
        {
            moLoggerError(MOCFT_LOGGER_MODULE_NAME, "Bind failed!" \
                "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
            close(cpServerSockId);
            cpServerSockId = MOCFT_INVALID_SOCKID;
        }
        else
        {
            //start listen for request
            listen(cpServerSockId, MOCFT_CP_SOCKET_SERV_MAXLISTENNUM);
        }
    }

    sem_init(&cpServerSem, 0, 0);
}

int cpServerUnInit()
{
    //socket must be free, or handle leak
    if(cpServerSockId != MOCFT_INVALID_SOCKID)
    {
        close(cpServerSockId);
        cpServerSockId = MOCFT_INVALID_SOCKID;
    }

    sem_destroy(&cpServerSem);
}

static int setFds(fd_set &rFd)
{
    FD_ZERO(&rFd);

    if(cpServerSockId != MOCFT_INVALID_SOCKID)
        FD_SET(cpServerSockId, &rFd);
    if(cpServerCliSockId != MOCFT_INVALID_SOCKID)
        FD_SET(cpServerCliSockId, &rFd);
    
    int maxFd = (cpServerSockId > cpServerCliSockId) ? cpServerSockId : cpServerCliSockId;
    return maxFd;
}

static bool isTaskOver()
{
    return (cpServerProgress >= 100 || cpServerProgress < 0) ? true : false;
}

static void * cpServerRecvReq(void * param)
{
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    fd_set rFd;

    while(1)
    {
        //Should refresh fds
        int maxFd = setFds(rFd);
        moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "maxFd = %d\n", maxFd);
        
        //recv a request
        int ret = select(maxFd + 1, &rFd, NULL, NULL, &tv);
        if(ret < 0)
        {
            moLoggerError(MOCFT_LOGGER_MODULE_NAME, "select failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            break;
        }
        else if(ret == 0)
        {
            //timeout, do nothing
            ;
        }
        else
        {
            if(FD_ISSET(cpServerSockId, &rFd))
            {
                if(cpServerCliSockId > 0)
                {
                    moLoggerError(MOCFT_LOGGER_MODULE_NAME, "mCliSockId = %d, should not recv another connect requst!\n",
                        cpServerCliSockId);
                    break;
                }
                struct sockaddr_un cliAddr;
                memset(&cliAddr, 0x00, sizeof(struct sockaddr_un));
                socklen_t cliAddrLen = 0;
                cpServerCliSockId = accept(cpServerSockId, (struct sockaddr *)&cliAddr, &cliAddrLen);
                if(cpServerCliSockId < 0)
                {
                    moLoggerError(MOCFT_LOGGER_MODULE_NAME, "accept from client failed! " \
                        "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
                    cpServerCliSockId = MOCFT_INVALID_SOCKID;
                    continue;
                }
                else
                {
                    moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "accept a client. mCliSockId = %d\n", cpServerCliSockId);
                }
            }
            else if(FD_ISSET(cpServerCliSockId, &rFd))
            {
                MOCFT_REQMSG reqMsg;
                memset(&reqMsg, 0x00, sizeof(MOCFT_REQMSG));
                int len = recv(cpServerCliSockId, (char *)&reqMsg, sizeof(MOCFT_REQMSG), 0);
                if(len != sizeof(MOCFT_REQMSG))
                {
                    moLoggerError(MOCFT_LOGGER_MODULE_NAME, "recv a request failed! len = %d, " \
                        "sizeof(MOCFT_REQMSG) = %d, errno = %d, desc = [%s]\n",
                        len, sizeof(MOCFT_REQMSG), errno, strerror(errno));
                    moLoggerError(MOCFT_LOGGER_MODULE_NAME, "will do nothing to this request.\n");
                    continue;
                }
                else
                {
                    if(cpServerState == CPSERV_SOCK_STATE_IDLE)
                    {
                        cpServerState = CPSERV_SOCK_STATE_CRYPTING;
                        //do it
                        TaskMgr taskMgr;
                        ret = taskMgr.doCrypt(reqMsg.task);
                        if(ret != 0)
                        {
                            cpServerProgress = ret;
                            //send a semphore to threadSendProgress, this thread will send this progress to cpClient
                            sem_post(&cpServerSem);

                            cpServerState = CPSERV_SOCK_STATE_IDLE;
                        }
                    }
                    else
                    {
                        //currently, just one task can be done at one moment
                        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "A task being done now, cannot do another task!\n");
                        continue;
                    }
                }
            }
        }
    }

    return NULL;
}

void cpServerSetProg(const int prog)
{
    cpServerProgress = prog;
    //send a semphore to thread sendProg
    sem_post(&cpServerSem);
}

static void * cpServerSendProg(void * param)
{
    while(1)
    {
        sem_wait(&cpServerSem);

        moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "recv a semaphore, mProgress now is %d\n", cpServerProgress);
        if(cpServerState == CPSERV_SOCK_STATE_IDLE)
        {
            moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "mState == CPSERV_SOCK_STATE_IDLE! will do nothing!\n");
            continue;
        }
        else
        {
            //Send progress to cpClient firstly
            MOCFT_RESPMSG respMsg;
            memset(&respMsg, 0x00, sizeof(MOCFT_RESPMSG));
            strcpy(respMsg.mark, MOCFT_MARK);
            strcpy(respMsg.prefix, MOCFT_NOTIFYMSG_PREFIX);
            respMsg.value = cpServerProgress;
            int ret = send(cpServerCliSockId, (char *)&respMsg, sizeof(MOCFT_RESPMSG), 0);
            if(ret != sizeof(MOCFT_RESPMSG))
            {
                moLoggerError(MOCFT_LOGGER_MODULE_NAME, "send failed! ret = %d, " \
                    "sizeof(MOCFT_RESPMSG) = %d, errno = %d, desc = [%s]\n",
                    ret, sizeof(MOCFT_RESPMSG), errno, strerror(errno));
            }
            else
            {
                //do nothing
            }

            //If task has been done, reset state to idle
            if(isTaskOver())
            {
                moLoggerDebug(MOCFT_LOGGER_MODULE_NAME, "task over!\n");
                cpServerState = CPSERV_SOCK_STATE_IDLE;
            }
        }
    }

    return NULL;
}

/*
    Running CpServer, start threads here.
*/
void cpServerRun()
{
    pthread_t thRecvReqId = 0, thSendProgId = 0;
    
    int ret = pthread_create(&thRecvReqId, NULL, cpServerRecvReq, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "create thread recvReqTh failed! " \
            "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        return;
    }
    
    ret = pthread_create(&thSendProgId, NULL, cpServerSendProg, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCFT_LOGGER_MODULE_NAME, "create thread sendProgTh failed! " \
            "ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        return;
    }
}




