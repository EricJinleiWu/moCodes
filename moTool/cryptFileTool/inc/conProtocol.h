#ifndef __CON_PROTOCOL_H__
#define __CON_PROTOCOL_H__

#include "utils.h"
#include "ui.h"

class cpClient
{
public:
    cpClient() {}
    virtual ~cpClient() {}

public:
    /*
        send msg to cpServer, and cpServer will send request to taskMgr;
    */
    virtual int sendReq(const MOCFT_TASKINFO &task) = 0;

    /*
        recv the event which being sent from cpServer, the progress will be sent;
    */
    virtual int recvEvent() = 0;

    /*
        A callback function, ui register to cpClient, when recvProg being called, 
        should tell ui this progress using this function pointer;
    */
    virtual int addListener(UI *ui) = 0;

    /*
        remove the listener function;
    */
    virtual int removeListener(UI *ui) = 0;

protected:
    UI *mpUI;
};

#endif
