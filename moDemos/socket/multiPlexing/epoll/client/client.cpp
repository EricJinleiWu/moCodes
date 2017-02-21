#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include "client.h"


#define error(format, ...) printf("EPOLL_DEMO_CLIENT : [%s, %s, %d ERR] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define debug(format, ...) printf("EPOLL_DEMO_CLIENT : [%s, %s, %d DBG] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)


Client::Client() : mIp("127.0.0.1"), mSockId(INVALID_SOCK_ID), mServIp("127.0.0.1"), mServPort(8082)
{}

Client::Client(const string & ip, const string & servIp, const unsigned int servPort) : 
    mIp(ip), mSockId(INVALID_SOCK_ID), mServIp(servIp), mServPort(servPort)
{}

Client::Client(const Client & other) : mIp(other.mIp), mSockId(other.mSockId), mServIp(other.mServIp), mServPort(other.mServPort)
{}

Client::~Client()
{
    if(mSockId != INVALID_SOCK_ID)
    {
        close(mSockId);
        mSockId = -1;
    }
}

Client & Client::operator=(const Client & other)
{
    if(this == &other)
    {
        return *this;
    }

    mIp = other.mIp;
    mSockId = other.mSockId;
    mServIp = other.mServIp;
    mServPort = other.mServPort;
}

void Client::Run()
{
    if(mSockId != INVALID_SOCK_ID)
    {
        error("Client has been running now, cannot run again.\n");
        return ;
    }

    debug("servIp = [%s], servPort = %d\n", mServIp.c_str(), mServPort);
    int ret = connect2Serv();
    if(0 != ret)
    {
        error("connect2Serv failed! ret = %d\n", ret);
        return;
    }

    while(true)
    {
        string msg("Client msg.");
        ret = sendMsg(msg);
        if(ret != 0)
        {
            error("sendMsg failed! ret = %d\n", ret);
            break;
        }

        string rMsg;
        ret = recvMsg(rMsg);
        if(ret != 0)
        {
            error("recvMsg failed! ret = %d\n", ret);
            break;
        }

        //break;
    }
    
}

int Client::connect2Serv()
{
    mSockId = socket(AF_INET, SOCK_STREAM, 0);
    if(mSockId < 0)
    {
        error("create socket failed! errno = %d, desc = [%s]\n", errno, strerror(errno));
        mSockId = INVALID_SOCK_ID;
        return -1;
    }
    debug("create socket succeed.\n");

    int on = 1;
    int ret = setsockopt(mSockId, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(ret != 0)
    {
        error("setsockopt failed when set reuse! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        close(mSockId);
        mSockId = INVALID_SOCK_ID;
        return -2;
    }
    debug("setsockopt to reused succeed.\n");

    struct sockaddr_in servAddr;
    memset(&servAddr, 0x00, sizeof(struct sockaddr_in));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(mServIp.c_str());
    servAddr.sin_port = htons(mServPort);
    ret = connect(mSockId, (struct sockaddr *)&servAddr, sizeof(struct sockaddr));
    if(ret != 0)
    {
        error("connect failed! ret = %d, errno = %d, desc = [%s]\n", ret, errno, strerror(errno));
        close(mSockId);
        mSockId = INVALID_SOCK_ID;
        return -3;
    }
    debug("connect to server succeed.\n");

    return 0;
}

int Client::sendMsg(const string & msg)
{
    debug("The msg being send is [%s]\n", msg.c_str());

    int sendNum = send(mSockId, msg.c_str(), msg.length(), 0);
    if(sendNum < 0)
    {
        error("send failed! ret = %d, errno = %d, desc = [%s]\n",
            sendNum, errno, strerror(errno));
        if(errno = EPIPE)
        {
            error("Server being closed!\n");
        }
        return -1;
    }
    debug("send to server succeed.\n");
    return 0;
}

int Client::recvMsg(string & msg)
{
    char recvInfo[128] = {0x00};
    memset(recvInfo, 0x00, 128);
    int recvNum = recv(mSockId, recvInfo, 128, 0);
    if(recvNum < 0)
    {
        error("recv failed! ret = %d, errno = %d, desc = [%s]\n",
            recvNum, errno, strerror(errno));
        return -1;
    }
    recvInfo[127] = 0x00;
    debug("recv from server succeed, msg = [%s].\n", recvInfo);
    msg = recvInfo;
    return 0;    
}
