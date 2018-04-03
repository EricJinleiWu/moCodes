#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>

#include "cliUtils.h"
#include "moCloudUtilsTypes.h"
#include "moCloudUtils.h"

/*
    create socket;
    set socket id to @gDataSockId;
    set this socket to non block;
    bind it to the ctrl port;
*/
int createSocket(const char * ip, const int port, int * pSockId)
{
    if(!isValidIpAddr(ip) || !isValidUserPort(port) || NULL == pSockId)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "ip=[%s], port=%d, have invalid one!\n", ip, port);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "ip=[%s], port=%d\n", ip, port);

    //1.create socket
    *pSockId = MOCLOUD_INVALID_SOCKID;
    *pSockId = socket(AF_INET, SOCK_STREAM, 0);
    if(*pSockId < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "create socket failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "create socket succeed, socketId=%d.\n", *pSockId);

    //2.set to reusable and nonblock
    int ret = setSockReusable(*pSockId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "setSockReusable failed! ret=%d, sockId=%d\n", ret, *pSockId);
        close(*pSockId);
        *pSockId = MOCLOUD_INVALID_SOCKID;
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "setSockReusable succeed.\n");

    ret = setSock2NonBlock(*pSockId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "setSock2NonBlock failed! ret=%d, sockId=%d\n", ret, *pSockId);
        close(*pSockId);
        *pSockId = MOCLOUD_INVALID_SOCKID;
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "setSock2NonBlock succeed.\n");

    //3.bind to the port being defined
    struct sockaddr_in cliAddr;
    memset(&cliAddr, 0x00, sizeof(struct sockaddr_in));
    cliAddr.sin_family = AF_INET;
    cliAddr.sin_port = htons(port);
    cliAddr.sin_addr.s_addr = inet_addr(ip);
    ret = bind(*pSockId, (struct sockaddr *)&cliAddr, sizeof(struct sockaddr_in));
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Bind failed! ret=%d, port=%d, errno=%d, desc=[%s]\n",
            ret, port, errno, strerror(errno));
        close(*pSockId);
        *pSockId = MOCLOUD_INVALID_SOCKID;
        return -5;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Bind succeed.\n");
    
    return 0;
}

/*
    connect to server with TCP mode;
    @sockId is the id of a socket, because we have data socket id and ctrl socket id, 
        so we should has this param to define them;
*/
int connectToServer(const int sockId, const char *servIp, const int servPort)
{
    struct sockaddr_in serAddr;
    memset(&serAddr, 0x00, sizeof(struct sockaddr_in));
    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(servPort);
    serAddr.sin_addr.s_addr = inet_addr(servIp);
    int ret = connect(sockId, (struct sockaddr *)&serAddr, sizeof(struct sockaddr_in));
    if(0 == ret)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "Connect to Server succeed, donot waste any time^_^!\n");
    }
    else
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
            "errno=%d, desc=[%s]\n", errno, strerror(errno));
        if(errno == EINPROGRESS)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                "errno == EINPROGRESS, should check result now.\n");
            //Do select here.
            fd_set wFdSet;
            FD_ZERO(&wFdSet);
            FD_SET(sockId, &wFdSet);
            fd_set rFdSet;
            FD_ZERO(&rFdSet);
            FD_SET(sockId, &rFdSet);
            struct timeval timeout;
            timeout.tv_sec = CONNECT_TIMEOUT;
            timeout.tv_usec = 0;
            ret = select(sockId + 1, &rFdSet, &wFdSet, NULL, &timeout);
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "select failed! ret = %d, errno = %d, desc = [%s]\n", 
                    ret, errno, strerror(errno));
                return -1;
            }
            else if(ret == 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "select timeout, we think this means connect to server failed!\n");
                return -2;
            }
            else
            {
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "select ok, check its result now.\n");
                if(FD_ISSET(sockId, &wFdSet))
                {
                    if(FD_ISSET(sockId, &rFdSet))
                    {
                        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                            "select ok, but wFdSet and rFdSet all being set, we should check!\n");
                        //check this socket is OK or not.
                        int err = 0;
                        socklen_t errLen = sizeof(err);
                        ret = getsockopt(sockId, SOL_SOCKET, SO_ERROR, &err, &errLen);
                        if(0 != ret)
                        {
                            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                                "getsockopt failed! ret = %d, errno = %d, desc = [%s]\n", 
                                ret, errno, strerror(errno));
                            return -3;
                        }
                        else
                        {
                            if(err != 0)
                            {
                                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
                                    "getsockopt ok, but err=%d, desc=[%s], donot mean OK, just mean some error!\n",
                                    err, strerror(err));
                                return -4;
                            }
                            else
                            {
                                moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME,
                                    "getsockopt ok, and err = %d, means connect succeed!\n", err);
                            }
                        }
                    }
                    else
                    {
                        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME,
                            "select ok, and just wFdSet being set, means connect being OK.\n");
                    }
                }
                else
                {
                    //Just this socket being set to wFdSet, if not this socket, error ocurred!
                    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
                        "select ok, but the fd is not client socket!\n");
                    return -5;
                }
            }
        }
        else
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
                "Connect failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            return -6;
        }
    }
    
    return 0;
}



/*
    If socket has valid socket id, should free it;
*/
void destroySocket(int * pSockId)
{
    if(*pSockId != MOCLOUD_INVALID_SOCKID)
    {
        close(*pSockId);
        *pSockId = MOCLOUD_INVALID_SOCKID;
    }
}


