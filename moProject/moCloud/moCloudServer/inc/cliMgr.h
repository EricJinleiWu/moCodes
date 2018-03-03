#ifndef __CLI_MGR_H__
#define __CLI_MGR_H__

#include <pthread.h>
#include <semaphore.h>

#include "moThread.h"
#include "cliCtrl.h"
#include "moCloudUtilsTypes.h"

using namespace std;

#include <vector>
#include <list>

class CliMgr : public MoThread
{
public:
    CliMgr();
    ~CliMgr();

public:
    virtual void run();

public:
    virtual void setChangedFiletype(const MOCLOUD_FILETYPE & type);
    virtual MOCLOUD_FILETYPE getChangedFiletype();

public:
    virtual int insertCliCtrl(CliCtrl * cliCtrl);

private:
    vector<MOCLOUD_FILETYPE> mChangedTypeVector;
    pthread_mutex_t mChangedTypeVectorLock;
    sem_t mSem;
    list<CliCtrl *> mCliCtrlList;
    pthread_mutex_t mCliCtrlListLock;
};

class CliMgrSingleton
{
public:    
    static CliMgr * getInstance();

private:
    CliMgrSingleton() {}
    ~CliMgrSingleton() {}

    static CliMgr * pInstance;
};


#endif

