#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "cliMgr.h"
#include "moCloudUtilsTypes.h"

CliMgr::CliMgr()
{
    mChangedTypeVector.clear();
    pthread_mutex_init(&mChangedTypeVectorLock, NULL);
    pthread_mutex_init(&mCliCtrlListLock, NULL);
    sem_init(&mSem, 0, 0);
    mCliCtrlList.clear();
}

CliMgr::~CliMgr()
{
    stop();
    join();
    sem_destroy(&mSem);
    pthread_mutex_destroy(&mCliCtrlListLock);
    pthread_mutex_destroy(&mChangedTypeVectorLock);
}

void CliMgr::run()
{
    //TODO, wait for signal, do it
}

void CliMgr::setChangedFiletype(const MOCLOUD_FILETYPE & type)
{
    if(type >= MOCLOUD_FILETYPE_MAX)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "type=%d, invalid value!\n", type);
        return ;
    }

    pthread_mutex_lock(&mChangedTypeVectorLock);

    bool isFind = false;
    for(vector<MOCLOUD_FILETYPE>::iterator it = mChangedTypeVector.begin();
        it != mChangedTypeVector.end(); it++)
    {
        if(*it == type)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                "type=%d has been exist yet.\n", type);
            isFind = true;
            break;
        }
    }

    if(!isFind)
    {
        mChangedTypeVector.push_back(type);
    }
    
    pthread_mutex_unlock(&mChangedTypeVectorLock);
}

MOCLOUD_FILETYPE CliMgr::getChangedFiletype()
{
    if(mChangedTypeVector.empty())
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "vector is empty now.\n");
        return MOCLOUD_FILETYPE_MAX;
    }

    pthread_mutex_lock(&mChangedTypeVectorLock);

    MOCLOUD_FILETYPE ret = mChangedTypeVector.back();
    mChangedTypeVector.pop_back();
    
    pthread_mutex_unlock(&mChangedTypeVectorLock);

    return ret;
}

int CliMgr::insertCliCtrl(const CliCtrl & cliCtrl)
{
    pthread_mutex_lock(&mCliCtrlListLock);

    for(list<CliCtrl>::iterator it = mCliCtrlList.begin();
        it != mCliCtrlList.end(); it++)
    {
        if(*it == cliCtrl)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "Client(ip=[%s], ctrlport=%d, dataport=%d) has been in now, cannot insert again.\n",
                it->getIp().c_str(), it->getCtrlPort(), it->getDataPort());
            pthread_mutex_unlock(&mCliCtrlListLock);
            return 0;
        }
    }
    
    mCliCtrlList.push_back(cliCtrl);
    pthread_mutex_unlock(&mCliCtrlListLock);
    return 0;
}

CliMgr * CliMgrSingleton::pInstance = new CliMgr;

CliMgr * CliMgrSingleton::getInstance() 
{
    return pInstance;
}



