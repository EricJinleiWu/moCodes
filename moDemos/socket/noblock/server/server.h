#ifndef __SERVER_H__
#define __SERVER_H__

using namespace std;

#include <string>

class Server
{
public:
    Server();
    Server(const string &ip, const unsigned int port);
    Server(const Server & other);
    virtual ~Server();

public:
    Server & operator = (const Server & other);

public:
    /* Init socket firstly;
       Then, accept the connects, new a thread to deal with this client;
       This will not exit util receive signal "ctrl+c";
       */
    void run(void);

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
