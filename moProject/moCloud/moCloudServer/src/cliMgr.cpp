#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

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
    while(getRunState())
    {
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_sec += 3;
        int ret = sem_timedwait(&mSem, &t);
        if(ret < 0)
        {
            if(errno = ETIMEDOUT)
            {
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "check the cliCtrl list now.\n");
                pthread_mutex_lock(&mCliCtrlListLock);

                for(list<CliCtrl * >::iterator it = mCliCtrlList.begin();
                    it != mCliCtrlList.end(); )
                {
                    if((*it)->getState() == CLI_STATE_INVALID)
                    {
                        CliCtrl * pCur = *it;
                        mCliCtrlList.erase(it++);
                        delete pCur;
                    }
                    else
                    {
                        it++;
                    }
                }
                
                pthread_mutex_unlock(&mCliCtrlListLock);
            }
            else
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "sem_timedwait failed! ret=%d, errno=%d, desc=[%s]\n",
                    ret, errno, strerror(errno));
            }
        }
        else
        {
            //If recv signal, means filelist changed;
            moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "recv a signal, file list changed.\n");

            vector<MOCLOUD_FILETYPE > v;
            pthread_mutex_lock(&mChangedTypeVectorLock);
            v = mChangedTypeVector;
            mChangedTypeVector.clear();
            pthread_mutex_unlock(&mChangedTypeVectorLock);

            int changeValue = 0;
            for(vector<MOCLOUD_FILETYPE>::iterator it = mChangedTypeVector.begin();
                it != mChangedTypeVector.end(); it++)
            {
                changeValue += (*it);
            }
            moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "changeValue=%d\n", changeValue);

            pthread_mutex_lock(&mCliCtrlListLock);
            for(list<CliCtrl *>::iterator it = mCliCtrlList.begin();
                it != mCliCtrlList.end(); it++)
            {
                CliCtrl * pCurCliCtrl = *it;
                if(CLI_STATE_LOGIN == pCurCliCtrl->getState())
                {
                    pCurCliCtrl->setFilelistChangeValue(changeValue);
                    pCurCliCtrl->setFilelistChangedFlag();
                }
            }
            pthread_mutex_unlock(&mCliCtrlListLock);
        }
    }
    
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

    sem_post(&mSem);
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

int CliMgr::insertCliCtrl(CliCtrl * cliCtrl)
{
    pthread_mutex_lock(&mCliCtrlListLock);

    for(list<CliCtrl *>::iterator it = mCliCtrlList.begin();
        it != mCliCtrlList.end(); it++)
    {
        CliCtrl * pCurCliCtrl = *it;
        if(pCurCliCtrl == cliCtrl)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "Client(ip=[%s], ctrlport=%d) has been in now, cannot insert again.\n",
                pCurCliCtrl->getIp().c_str(), pCurCliCtrl->getCtrlPort());
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



