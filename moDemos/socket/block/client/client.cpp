#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/un.h>

#include "client.h"

//#define TCP_MODE    0

Client::Client(const RUNNING_MODE mode) : mMode(mode), 
    mIp("127.0.0.1"), mPort(8081), mIsSocketInited(false), mSockId(-1)
{
    ;
}

Client::Client(const RUNNING_MODE mode, const string & ip, const unsigned int port) :
    mMode(mode), mIp(ip), mPort(port), mIsSocketInited(false), mSockId(-1)
{
    ;
}
    
Client::Client(const Client & other)
{
    mMode = other.mMode;
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
    if(this == &other)
        return *this;

    mMode = other.mMode;
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

    switch(mMode)
    {
        case TCP_MODE:
        {
            printf("Send msg in TCP case.\n");
            //send, recv
            int ret = 0;
            if((ret = send(mSockId, reqMsg.c_str(), reqMsg.length(), 0)) < 0)
            {
                if(errno = EPIPE)
                {
                    printf("send failed! donot find server running!\n");
                }
                else
                {        
                    printf("send failed! ret = %d, reqMsg = [%s], errno = %d, desc = [%s]\n", 
                        ret, reqMsg.c_str(), errno, strerror(errno));
                }
                return -2;
            }
            printf("send succeed.\n");

            char recvMsg[128] = {0x00};
            memset(recvMsg, 0x00, 128);
            if((ret = recv(mSockId, recvMsg, 128, 0)) < 0)
            {
                printf("recv failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
                return -3;
            }
            recvMsg[127] = 0x00;
            printf("recv succeed.\n");

            respMsg = recvMsg;
            
            return 0;
        }
        break;
        
        case UDP_MODE:
        {
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
                if(errno = EPIPE)
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
            printf("sendto succeed. ret = %d, reqMsg.length() = %d\n", ret, reqMsg.length());

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
        }
        break;
        
        case UNIX_MODE:
        {
            struct sockaddr_un servAddr;
            memset(&servAddr, 0x00, sizeof(struct sockaddr_un));
            servAddr.sun_family = AF_UNIX;
            strcpy(servAddr.sun_path, UNIX_SOCK_PATH);

            int ret = 0;
            if((ret = sendto(mSockId, reqMsg.c_str(), reqMsg.length(), 0, (struct sockaddr *)&servAddr, sizeof(struct sockaddr_un))) < 0)
            {
                printf("sendto failed! ret = %d, reqMsg = [%s], errno = %d, desc = [%s]\n",
                    ret, reqMsg.c_str(), errno, strerror(errno));
                return -2;
            }
            printf("sendto succeed. ret = %d, reqMsg = [%s], length is %d\n",
                ret, reqMsg.c_str(), reqMsg.length());

            //TODO, recv donot being done because I have no time now.

            return 0;
        }
        break;
        
        default:
        {
            printf("mMode = %d, donot valid mode! cannot send msg!\n");
            return -2;
        }
        break;
    }
    
    return 0;
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
        mSockId = -1;
        switch(mMode)
        {
        case TCP_MODE:
            mSockId = socket(AF_INET, SOCK_STREAM, 0);
            break;
        case UDP_MODE:
            mSockId = socket(AF_INET, SOCK_DGRAM, 0);
            break;
        case UNIX_MODE:
            mSockId = socket(AF_UNIX, SOCK_DGRAM, 0);
            break;
        default:
            printf("mMode = %d, donot support it.\n", mMode);
            break;
        }
        
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

        //Tcp should connect, binding donot neccessary
        if(mMode == TCP_MODE)
        {
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
        }

        mIsSocketInited = true;
    }

    printf("initSocket OK.\n");
    
    return 0;
}
