#ifndef __TASK_MGR_H__
#define __TASK_MGR_H__

#include "utils.h"

class TaskMgr
{
public:
    TaskMgr();
    TaskMgr(void * pFunc);
    TaskMgr(const TaskMgr & other);
    ~TaskMgr();

public:
    /*
        start do crypt;
    */
    virtual int doCrypt(const MOCFT_TASKINFO & info);

private:
    /*
        The function pointer which will be used to set progress;
    */
    void * mProgCallbackFunc;
};

#endif
