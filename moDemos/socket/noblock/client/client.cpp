#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "client.h"

#define TCP_MODE    1

Client::Client() : mIp("127.0.0.1"), mPort(8081), mIsSocketInited(false), mSockId(-1)
{
    ;
}

Client::Client(const string & ip, const unsigned int port) :
    mIp(ip), mPort(port), mIsSocketInited(false), mSockId(-1)
{
    ;
}
    
Client::Client(const Client & other)
{
    mIp = other.mIp;
    mPort = other.mPort;
    mIsSocketInited = other.mIsSocketInited;
    mSockId = other.mSockId;
}

Client::~Client()
{
    close(mSockId);
    mSockId = -1;
}

Client & Client::operator = (const Client & other)
{
    mIp = other.mIp;
    mPort = other.mPort;
    mIsSocketInited = other.mIsSocketInited;
    mSockId = other.mSockId;

    return *this;
}

int Client::sendMsg(const string & reqMsg,string & respMsg)
{
    if(!mIsSocketInited)
    {
        int ret = initSocket();
        if(0 != ret)
        {
            printf("initSocket failed! ret = %d\n", ret);
            return -1;
        }
    }

    printf("Being inited, will send msg now.\n");

#if TCP_MODE
    printf("Send msg in TCP case.\n");
    //send, recv
    int ret = 0;
    while(1)
    {
        if((ret = send(mSockId, reqMsg.c_str(), reqMsg.length(), 0)) < 0)
        {
            printf("send ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
            if(errno = SIGPIPE)
            {
                printf("send failed! donot find server running!\n");
            }
            else if(errno = EWOULDBLOCK)
            {
                printf("send failed! This is because we are in noblock state. retry in 100ms.\n");
                usleep(100 * 1000);
                continue;
            }
            else
            {        
                printf("send failed! ret = %d, reqMsg = [%s], errno = %d, desc = [%s]\n", 
                    ret, reqMsg.c_str(), errno, strerror(errno));
            }
            return -2;
        }
        else
        {
            printf("send succeed.\n");
            break;
        }
    }

    char recvMsg[128] = {0x00};
    while(1)
    {
        memset(recvMsg, 0x00, 128);
        if((ret = recv(mSockId, recvMsg, 128, 0)) < 0)
        {
            if(errno == EWOULDBLOCK)
            {
                printf("recv failed because its noblock, will retry in 100ms.\n");
                usleep(100 * 1000);
                continue;
            }
            printf("recv failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
            return -3;
        }
        else
        {
            recvMsg[127] = 0x00;
            printf("recv succeed.\n");
            break;
        }
    }

    respMsg = recvMsg;

    return 0;
#else
    printf("Send msg in UDP case.\n");
    //sendto, recvfrom
    struct sockaddr_in serverAddr;
    bzero(&serverAddr, sizeof(struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8082);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int ret = 0;

    if((ret = sendto(mSockId, reqMsg.c_str(), reqMsg.length(), 0, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in))) < 0)
    {
        if(errno = SIGPIPE)
        {
            printf("sendto failed! donot find server running! ret = %d, errno = %d, desc = [%s]\n",
                ret, errno, strerror(errno));
        }
        else
        {        
            printf("sendto failed! ret = %d, reqMsg = [%s], errno = %d, desc = [%s]\n", 
                ret, reqMsg.c_str(), errno, strerror(errno));
        }
        return -2;
    }
    printf("sendto succeed.\n");

    char recvBuff[128] = {0x00};
    memset(recvBuff, 0x00, 128);
    unsigned int sockAddrLen = 0;
    if((ret = recvfrom(mSockId, recvBuff, 128, 0, (struct sockaddr *)&serverAddr, &sockAddrLen)) < 0)
    {
        printf("recvFrom failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        return -3;
    }
    recvBuff[127] = 0x00;
    printf("recvFrom succeed. recvBuff = [%s]\n", recvBuff);

    respMsg = recvBuff;
    
    return 0;
#endif
}

void Client::dump()
{
    printf("ip = [%s], port = [%d]\n", mIp.c_str(), mPort);
}

int Client::initSocket()
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
        printf("client socket being created.\n");

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
            printf("bind to ip[%s],port[%d] failed! ret = %d, errno = %d, desc = [%s]\n", 
                mIp.c_str(), mPort, ret, errno, strerror(errno));
            close(mSockId);
            mSockId = -1;
            return -2;
        }
        printf("client being binded.\n");

#if TCP_MODE
        //connect to server
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0x00, sizeof(sockaddr_in));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(8082);
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        ret = connect(mSockId, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in));
        if(ret < 0)
        {
            printf("connect to server failed! ret = %d, server ip = 127.0.0.1, port = 8082, errno = %d, desc = [%s]\n", 
                ret, errno, strerror(errno));
            close(mSockId);
            mSockId = -1;
            return -3;
        }
        printf("connect to server OK.\n");
#endif

#if 0
        //Set this socket to noBlock mode
        int flags = fcntl(mSockId, F_GETFL, 0);
        if(flags < 0)
        {
            printf("Get the flags of current socket failed!\n");
            close(mSockId);
            mSockId = -1;
            return -4;
        }
        ret = fcntl(mSockId, F_SETFL, flags | O_NONBLOCK);
        if(ret < 0)
        {
            printf("Get the flags of current socket failed!\n");
            close(mSockId);
            mSockId = -1;
            return -4;
        }
#else
        //Set this socket to noBlock mode in another method
        int b_on = 1;
        ioctl(mSockId, FIONBIO, &b_on);
#endif
        mIsSocketInited = true;
    }

    printf("initSocket OK.\n");
    
    return 0;
}