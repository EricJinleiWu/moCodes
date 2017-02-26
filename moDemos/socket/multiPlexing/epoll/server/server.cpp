#include <stdio.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "server.h"

#define error(format, ...) printf("EPOLL_DEMO_SERVER : [%s, %s, %d ERR] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define debug(format, ...) printf("EPOLL_DEMO_SERVER : [%s, %s, %d DBG] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)


Server::Server() : mIp("127.0.0.1"), mPort(8082), mSockId(INVALID_SOCK_ID), mEpfd(-1), mMaxFds(0)
{
    mCliFdsVector.clear();
}

Server::Server(const string & ip, const unsigned int port) : 
    mIp(ip), mPort(port), mSockId(INVALID_SOCK_ID), mEpfd(-1), mMaxFds(0)
{
    mCliFdsVector.clear();
}

Server::Server(const Server &other) : mIp(other.mIp), mPort(other.mPort), mSockId(other.mSockId),
    mCliFdsVector(other.mCliFdsVector), mEpfd(other.mEpfd), mMaxFds(mMaxFds)
{
}

Server::~Server()
{
    if(mSockId != INVALID_SOCK_ID)
    {
        close(mSockId);
        mSockId = INVALID_SOCK_ID;
    }
    if(mEpfd >= 0)
    {
        close(mEpfd);
        mEpfd = -1;
    }
    mCliFdsVector.clear();
}

Server & Server::operator = (const Server & other)
{
    if(this == &other)
    {
        return *this;
    }

    mIp = other.mIp;
    mPort = other.mPort;
    mSockId= other.mSockId;
    mEpfd = other.mEpfd;
    mCliFdsVector = other.mCliFdsVector;
    mMaxFds = other.mMaxFds;
    return *this;
}

void Server::Run()
{
    if(mSockId != INVALID_SOCK_ID)
    {
        error("Server has been running, cannot run again.\n");
        return ;
    }

    int ret = initSock();
    if(ret != 0)
    {
        error("initSock failed! ret = %d\n", ret);
        return ;
    }

    while(true)
    {
        struct epoll_event *pEvts = NULL;
        pEvts = (struct epoll_event *)malloc(sizeof(struct epoll_event) * mMaxFds);
        if(NULL == pEvts)
        {
            error("Malloc for pEvts failed! errno = %d, desc = [%s]", 
                errno, strerror(errno));
            break;
        }
        int timeout = 5000;  //ms
        ret = epoll_wait(mEpfd, pEvts, mMaxFds, timeout);
        if(ret == 0)
        {
            debug("timeout when epoll_wait. will wait again.\n");
            free(pEvts);
            pEvts = NULL;
            continue;
        }
        else if(ret < 0)
        {
            error("epoll_wait failed! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
            free(pEvts);
            pEvts = NULL;
            break;
        }
        else
        {
//            debug("epoll_wait OK. ret = %d.\n", ret);

            int i = 0;
            for(i = 0; i < mMaxFds; i++)
            {
                if(pEvts[i].data.fd == mSockId)
                {
                    struct sockaddr_in clientAddr;
                    memset(&clientAddr, 0x00, sizeof(struct sockaddr_in));
                    socklen_t cliAddrLen = sizeof(struct sockaddr_in);
                    int clientFd = accept(mSockId, (struct sockaddr *)&clientAddr, &cliAddrLen); //dont care the client ip and port
                    if(clientFd < 0)
                    {
                        error("accept failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
                        free(pEvts);
                        pEvts = NULL;
                        break;
                    }
                    else
                    {
                        debug("accept OK.\n");
                        //Add this client fd to local, refresh mMaxFds, add it to mEpfd event
                        struct epoll_event event;
                        event.data.fd = clientFd;
                        event.events = EPOLLIN;
                        ret = epoll_ctl(mEpfd, EPOLL_CTL_ADD, clientFd, &event);
                        if(ret != 0)
                        {
                            error("epoll_ctl failed! ret = %d, errno = %d, desc = [%s]\n",
                                ret, errno, strerror(errno));
                            free(pEvts);
                            pEvts = NULL;
                            break;
                        }

                        mMaxFds++;

                        mCliFdsVector.push_back(clientFd);
                    }
                }
                else if(pEvts[i].events & EPOLLIN)  //data can be read from client
                {
                    char rmsg[128] = {0x00};
                    memset(rmsg, 0x00, 128);
                    int recvNum = recv(pEvts[i].data.fd, rmsg, 128, 0);
                    if(recvNum < 0)
                    {
                        error("recv from client failed! errno = %d, desc = [%s]\n",
                            errno, strerror(errno));
                        free(pEvts);
                        pEvts = NULL;
                        break;
                    }
                    rmsg[127] = 0x00;
                    debug("receive a msg [%s] from client!\n", rmsg);

                    //Modify this socket to write state
                    struct epoll_event event;
                    event.data.fd = pEvts[i].data.fd;
                    event.events = EPOLLOUT;
                    ret = epoll_ctl(mEpfd, EPOLL_CTL_MOD, pEvts[i].data.fd, &event);
                    if(ret != 0)
                    {
                        error("Modify client from EPOLLIN to EPOLLOUT failed! errno = %d, desc = [%s]\n",
                            errno, strerror(errno));
                        free(pEvts);
                        pEvts = NULL;
                        break;
                    }
                    debug("Modify client from EPOLLIN to EPOLLOUT succeed.\n");
                }
                else if(pEvts[i].events & EPOLLOUT)  //data can be send to client
                {
                    char wmsg[] = "response from epoll server.";
                    int sendNum = send(pEvts[i].data.fd, wmsg, strlen(wmsg) + 1, 0);
                    if(sendNum < 0)
                    {
                        error("send failed! errno = %d, desc = [%s]\n", errno, strerror(errno));

                        //delete this socket from clientVector, and refresh mMaxFds, and do epoll_ctl
                        ret = epoll_ctl(mEpfd, EPOLL_CTL_DEL, pEvts[i].data.fd, (struct epoll_event *)&(pEvts[i].events));
                        if(ret != 0)
                        {
                            error("epoll ctl to delete a client socket failed! errno = %d, desc = [%s]\n",
                                errno, strerror(errno));    
                        }

                        //TODO
//                        mCliFdsVector

                        mMaxFds--;
                        
                        free(pEvts);
                        pEvts = NULL;
                        
                        break;
                    }
                    debug("send [%s] to client succeed.\n", wmsg);

                    //Modify this socket to read state
                    struct epoll_event event;
                    event.data.fd = pEvts[i].data.fd;
                    event.events = EPOLLIN;
                    ret = epoll_ctl(mEpfd, EPOLL_CTL_MOD, pEvts[i].data.fd, &event);
                    if(ret != 0)
                    {
                        error("Modify client from EPOLLOUT to EPOLLIN failed! errno = %d, desc = [%s]\n",
                            errno, strerror(errno));
                        free(pEvts);
                        pEvts = NULL;
                        break;
                    }
                    debug("Modify client from EPOLLOUT to EPOLLIN succeed.\n");
                }
                else
                {
                    debug("The events will not be done now.\n");
                }
            }
        }

        free(pEvts);
        pEvts = NULL;
    }
}

int Server::initSock()
{
    mSockId = socket(AF_INET, SOCK_STREAM, 0);
    if(mSockId < 0)
    {
        error("create socket failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        return -1;
    }
    debug("create socket OK.\n");

    int on = 1;
    int ret = setsockopt(mSockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret != 0)
    {
        error("setsockopt failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return -2;
    }
    debug("setsockopt for reused OK\n");

    //nonblock
    on = 1;
    ioctl(mSockId, FIONBIO, &on);
    debug("set socket to non-blocked OK.\n");

    //bind
    struct sockaddr_in servAddr;
    memset(&servAddr, 0x00, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(mIp.c_str());
    servAddr.sin_port = htons(mPort);
    ret = bind(mSockId, (struct sockaddr *)&servAddr, sizeof(struct sockaddr));
    if(ret != 0)
    {
        error("bind failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(mSockId);
        mSockId = INVALID_SOCK_ID;
        return -3;
    }
    debug("bind OK.\n");

    //listen
    ret = listen(mSockId, 128);
    if(ret < 0)
    {
        error("listen failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        close(mSockId);
        mSockId = INVALID_SOCK_ID;
        return -4;
    }

    //create epoll
    mEpfd = epoll_create(1024);
    if(mEpfd < 0)
    {
        error("epoll_create failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        close(mSockId);
        mSockId = INVALID_SOCK_ID;
        return -5;
    }
    debug("epoll_create OK.\n");

    //Add server socket to epoll set firstly, this will help us to get request from client to connect
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = mSockId;
    ret = epoll_ctl(mEpfd, EPOLL_CTL_ADD, mSockId, &event);
    if(ret != 0)
    {
        error("epoll_ctl failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(mSockId);
        mSockId = INVALID_SOCK_ID;
        close(mEpfd);
        mEpfd= -1;
        return -6;
    }
    debug("epoll_ctl succeed.\n");

    mMaxFds = 1;

    return 0;
}


