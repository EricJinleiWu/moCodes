#ifndef __SERVER_H__
#define __SERVER_H__

using namespace std;

#include <string>
#include <vector>

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
    void Run();

private:
    int initSock();

private:
    string mIp;
    unsigned int mPort;
    int mSockId;
    int mEpfd;  //The fd of epoll
    vector<int> mCliFdsVector;  //A vector which save all fd of clients
    unsigned int mMaxFds;   //When epoll_wait, we need to know how many fd will be checked, this is the max value.
};

#endif