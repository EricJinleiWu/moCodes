#ifndef __CLIENT_H__
#define __CLIENT_H__

using namespace std;

#include <string>

class Client
{
public:
    Client();
    Client(const string &ip, const unsigned int port);
    Client(const Client & other);
    ~Client();

public:
    Client & operator = (const Client & other);

public:
    /* Send a message to server, and recv message from server, 
       if function running OK, return 0; 
       else, return 0-;
       */
    int sendMsg(const string &reqMsg, string &respMsg);

    /* Dump the ip and port info; */
    void dump(void);

private:
    /* Init the socket of client */
    int initSocket();

private:
    string mIp;
    unsigned int mPort;
    int mSockId;
    bool mIsSocketInited; 
};

#endif
