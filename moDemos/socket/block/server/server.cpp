#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#include "server.h"

#define TCP_MODE    0

Server::Server() : mIp("127.0.0.1"), mPort(8082), mIsSocketInited(false), mSockId(-1)
{
    ;
}

Server::Server(const string & ip, const unsigned int port) :
    mIp(ip), mPort(port), mIsSocketInited(false), mSockId(-1)
{
    ;
}
    
Server::Server(const Server & other)
{
    mIp = other.mIp;
    mPort = other.mPort;
    mIsSocketInited = other.mIsSocketInited;
    mSockId = other.mSockId;
}

Server::~Server()
{
    close(mSockId);
    mSockId = -1;
}

Server & Server::operator = (const Server & other)
{
    mIp = other.mIp;
    mPort = other.mPort;
    mIsSocketInited = other.mIsSocketInited;
    mSockId = other.mSockId;

    return *this;
}

/*
    Looping here, to receive request, deal with it, then wait for the next request...... 
*/
void Server::run()
{
    while(true)
    {
        if(!mIsSocketInited)
        {
            int ret = initSocket();
            if(0 != ret)
            {
                printf("initSocket failed! ret = %d\n", ret);
                sleep(1);
                continue;
            }
            else
            {
                printf("initSocket succeed!\n");
            }
        }

#if TCP_MODE
        printf("Start accept reqeust from client now.\n");
        int clientSockId = accept(mSockId, NULL, NULL);
        if(clientSockId > 0)
        {
            printf("accept a request from client.\n");
//            printf("accept a request from client : ip = [%s], port = [%d]\n");
            char recvMsg[128] = {0x00};
            memset(recvMsg, 0x00, 128);
            int ret = recv(clientSockId, recvMsg, 128, 0);
            if(ret < 0)
            {
                printf("recv failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
                continue;
            }
            else
            {
                recvMsg[127] = 0x00;
                printf("recv succeed. msg being send is [%s]\n", recvMsg);
            }

            //Send a message to be response
            string respMsg("Response");
            ret = send(clientSockId, respMsg.c_str(), respMsg.length(), 0);
            if(ret < 0)
            {
                printf("send response failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
                continue;
            }
            else
            {
                printf("send response succeed.\n");
            }
        }
#else
        //UDP mode, just recvfrom is OK.
        struct sockaddr_in clientAddr;
        memset(&clientAddr, 0x00, sizeof(struct sockaddr_in));
        socklen_t clientAddrLen = sizeof(struct sockaddr_in);
        char recvBuf[128] = {0x00};
        memset(recvBuf, 0x00, 128);
        recvBuf[127] = 0x00;
        int recvLen = 128;
        int ret = recvfrom(mSockId, (void *)recvBuf, recvLen, 0, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if(ret < 0)
        {
            printf("recv failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
            continue;
        }
        else
        {
            recvBuf[127] = 0x00;
            printf("recv succeed. recv msg is [%s], client ip = [%s], port = %d\n", 
                recvBuf, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

            //Send a message to be response
            string respMsg("Response");
            ret = sendto(mSockId, respMsg.c_str(), respMsg.length(), 0, (struct sockaddr *)&clientAddr, sizeof(struct sockaddr));
            if(ret < 0)
            {
                printf("send response to client failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
                continue;
            }
            else
            {
                printf("send response to client succeed.\n");
            }
        }
#endif
    }
}

void Server::dump()
{
    printf("ip = [%s], port = [%d]\n", mIp.c_str(), mPort);
}

int Server::initSocket()
{
    //init socket with ip and port
    if(!mIsSocketInited)
    {
#if TCP_MODE
        mSockId = socket(AF_INET, SOCK_STREAM, 0);
#else
        mSockId = socket(AF_INET, SOCK_DGRAM, 0);
#endif
        if(-1 == mSockId)
        {
            printf("socket failed!\n");
            return -1;
        }
        printf("server socket being created.\n");
        
        int on=1;
        int ret = setsockopt(mSockId,SOL_SOCKET, SO_REUSEADDR,&on,sizeof(on));
        if(ret != 0)
        {
            printf("setsockopt SO_REUSEADDR failed! ret = %d\n", ret);
            close(mSockId);
            mSockId = -1;
            return -1;
        }
        printf("setsockopt SO_REUSEADDR succeed.\n");
        ret = setsockopt(mSockId,SOL_SOCKET, SO_BROADCAST,&on,sizeof(on));
        if(ret != 0)
        {
            printf("setsockopt SO_BROADCAST failed! ret = %d\n", ret);
            close(mSockId);
            mSockId = -1;
            return -1;
        }
        printf("setsockopt SO_BROADCAST succeed.\n");

        //binding socket
        struct sockaddr_in clientAddr;
        bzero(&clientAddr, sizeof(struct sockaddr_in));
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = htons(mPort);
        clientAddr.sin_addr.s_addr = inet_addr(mIp.c_str());
        ret = bind(mSockId, (struct sockaddr *)&clientAddr, sizeof(struct sockaddr_in));
        if(0 != ret)
        {
            printf("bind to ip[%s],port[%d] failed! ret = %d\n", mIp.c_str(), mPort);
            close(mSockId);
            mSockId = -1;
            return -2;
        }
        printf("server being binded.\n");

#if TCP_MODE
        //Listen
        ret = listen(mSockId, 128);
        if(ret < 0)
        {
            printf("Listen failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
            close(mSockId);
            mSockId = -1;
            return -3;
        }
        printf("Server being listened.\n");
#endif
        mIsSocketInited = true;
    }

    printf("initSocket OK.\n");
    
    return 0;
}