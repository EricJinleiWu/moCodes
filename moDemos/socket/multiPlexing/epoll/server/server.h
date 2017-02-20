#ifndef __SERVER_H__
#define __SERVER_H__

using namespace std;

#include <string>

#define INVALID_SOCK_ID (-1)

class Server
{
public:
    Server();
    Server(const string & ip, const unsigned int port);
    Server(const Server & other);
    virtual ~Server();

public:
    Server & operator = (const Server & other);

public:
    //TODO, other APIs

private:
    //TODO, other functions


private:
    string mIp;
    unsigned int mPort;
    int mSockId;
};

#endif