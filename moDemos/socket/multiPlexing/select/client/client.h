#ifndef __CLIENT_H__
#define __CLIENT_H__

using namespace std;

#include <string>
#include <map>

typedef enum
{
    SOCKET_MODE_TCP,
    SOCKET_MODE_UDP    
}SOCKET_MODE;

/* In multiplexing mode, we will not use a defined port to create socket;
   OS will give a port to us;
 */
class Client
{
public:
    Client();
    Client(const Client & other);
    Client(const string &ip, const SOCKET_MODE mode = SOCKET_MODE_TCP);
    virtual ~Client();

public:
    Client & operator = (const Client & other);

public:
    int initSocket();
    void dump();
    int sendMsg(const string & msg);
    int recvMsg(string & msg);

private:
    int initTcpSocket();
    int initUdpSocket();
    
    void initModeDescMap();

private:
    string mIp;
    SOCKET_MODE mMode;
    int mSocketId;
    map<int, string> mModeDescMap;
    int mServerSockId;  //The socket id of server, when tcp mode, this is needed.
};

#endif