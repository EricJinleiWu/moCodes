#ifndef __CLIENT_H__
#define __CLIENT_H__

using namespace std;

#include <string>

#define INVALID_SOCK_ID (-1)

class Client
{
public:
    Client();
    Client(const string & ip, const string & servIp, const unsigned int servPort);
    Client(const Client & other);
    virtual ~Client();

public:
    Client & operator = (const Client & other);

public:
    /*
        Connect to server;
        Send msg;
        Recv msg;
        Loop;
    */
    void Run();

private:
    int connect2Serv();
    int sendMsg(const string & msg);
    int recvMsg(string & msg);

private:
    string mIp;
    int mSockId;
    string mServIp;
    unsigned int mServPort;
};

#endif