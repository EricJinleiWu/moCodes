#include <stdio.h>
#include <sys/epoll.h>

#include "server.h"

Server::Server() : mIp("127.0.0.1"), mPort(8082), mSockId(INVALID_SOCK_ID)
{}

Server::Server(const string & ip, const unsigned int port) : 
    mIp(ip), mPort(port), mSockId(INVALID_SOCK_ID)
{}

Server::Server(const Server &other) : mIp(other.mIp), mPort(other.mPort), mSockId(other.mSockId)
{}

Server::~Server()
{
    ;
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
    return *this;
}

