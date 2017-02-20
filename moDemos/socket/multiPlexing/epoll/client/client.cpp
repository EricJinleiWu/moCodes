#include <stdio.h>
#include <stdlib.h>

#include "client.h"

Client::Client() : mIp("127.0.0.1"), mSockId(INVALID_SOCK_ID)
{}

Client::Client(const string & ip) : mIp(ip), mSockId(INVALID_SOCK_ID)
{}

Client::Client(const Client & other) : mIp(other.mIp), mSockId(other.mSockId)
{}

Client::~Client()
{}

Client & Client::operator=(const Client & other)
{
    if(this == &other)
    {
        return *this;
    }

    mIp = other.mIp;
    mSockId = other.mSockId;
}
