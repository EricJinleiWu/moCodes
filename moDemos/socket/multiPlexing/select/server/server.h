#ifndef __SERVER_H__
#define __SERVER_H__

#include <pthread.h>

using namespace std;

#include <string>
#include <map>


typedef enum
{
    SOCKET_MODE_TCP,
    SOCKET_MODE_UDP    
}SOCKET_MODE;

//The max number of clients being listend
#define MAX_LISTEN_NUM 128 

#define INVALID_SOCK_ID -1

class Server
{
public:
    Server();
    Server(const Server & other);
    Server(const string & ip, const unsigned int port, const SOCKET_MODE mode = SOCKET_MODE_TCP);
    virtual ~Server();

public:
    Server & operator = (const Server & other);

public:
    void run();

public:
    int initSocket();
    void dump();

private:
    int initTcpSocket();
    int initUdpSocket();

    void initModeDescMap();

    /* set all member in this array to INVALID_SOCK_ID */
    void initCliSockIdArr();

    /* insert a new client socket id to mCliSockIdArr, 
            if this socket id has been in it,do nothing and return ok, 
            if mCliSockIdArr has full, return err;
    */
    int insertCliSockId(const unsigned int id);
    /* delete a client socket id from mCliSockIdArr;
       after delete, set all valid client socket id to the head of mCliSockIdArr;
    */
    int delCliSockId(const unsigned int id);
    /* dump mCliSockIdArr */
    void dumpCliSockIds();

    void runInTcpMode();
    void runInUdpMode();

    ssize_t readn(int fd, void *buf, size_t num);

private:
    string mServIp;
    unsigned int mServPort;
    int mServSockId;
    SOCKET_MODE mMode;  //TCP or UDP
    map<int, string> mModeDescMap;
    //Record all client socket id here
    //initial value is -1, plus number is valid
    int mCliSockIdArr[MAX_LISTEN_NUM];
    pthread_mutex_t mCliSockIdArrMutex;
};

#endif
