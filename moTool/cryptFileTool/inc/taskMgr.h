#ifndef __TASK_MGR_H__
#define __TASK_MGR_H__

#include "utils.h"
#include "cpSocket.h"

class TaskMgr
{
public:
    TaskMgr();
    TaskMgr(const TaskMgr & other);
    ~TaskMgr();

public:
    /*
        start do crypt;
    */
    virtual int doCrypt(const MOCFT_TASKINFO & info);
};

//class TaskMgrSingleton
//{
//public:
//    ~TaskMgrSingleton();
//    TaskMgrSingleton(CpServerSocket  * pServ, pSetProgCallbackFunc pFunc);
//    static TaskMgr * GetInstance();
//    
//private:
//    TaskMgrSingleton();

//    static TaskMgr * instance;
//};

#endif
