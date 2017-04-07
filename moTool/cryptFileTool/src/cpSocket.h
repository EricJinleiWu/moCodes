#ifndef __CP_SOCKET_H__
#define __CP_SOCKET_H__

using namespace std;

#include <list>

#include "conProtocol.h"

typedef enum
{
    RESP_TYPE_RESULT,
    RESP_TYPE_PROGRESS,
    RESP_TYPE_ERROR
}RESP_TYPE;

class CpClientSocket : public cpClient
{
public:
    CpClientSocket();
    CpClientSocket(const CpClientSocket & other);
    ~CpClientSocket();

public:
    virtual int sendReq(const MOCFT_TASKINFO &task);
    virtual int recvEvent();
    virtual int addListener(UI *ui);
    virtual int removeListener(UI *ui);

private:
    RESP_TYPE getRespType(const MOCFT_RESPMSG & respMsg);

private:
    int mSockId;
};


typedef enum
{
    CPSERV_SOCK_STATE_IDLE,
    CPSERV_SOCK_STATE_CRYPTING
}CPSERV_SOCK_STATE;

class CpServerSocket : public cpServer
{
public:
    CpServerSocket();
    CpServerSocket(const CpServerSocket & other);
    ~CpServerSocket();

public:
    virtual int recvReq();
    virtual int setProg(const int prog);
    virtual int sendEvent(const int prog);

private:
    int mSockId;
    list<int> mCliSockIdList;
};


#endif
