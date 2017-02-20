#ifndef __CLIENT_H__
#define __CLIENT_H__

using namespace std;

#include <string>

#define CLIENT_DEF_PORT 8081
#define INVALID_SOCK_ID (-1)

class Client
{
public:
    Client();
    Client(const string & ip);
    Client(const Client & other);
    virtual ~Client();

public:
    Client & operator = (const Client & other);

private:
    string mIp;
    int mSockId;
};

#endif