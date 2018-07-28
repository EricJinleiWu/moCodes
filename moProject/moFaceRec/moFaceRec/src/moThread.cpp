#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "moThread.h"
#include "utils.h"

MoThread::MoThread() : mThrId(INVALID_THR_ID), mThrName(DEFAULT_THR_NAME),
    mIsRun(true)
{
    ;
}

MoThread::MoThread(const string & thrName) : mThrId(INVALID_THR_ID), 
    mThrName(thrName), mIsRun(true)
{
    ;
}

MoThread::~MoThread()
{
    ;
}

pthread_t MoThread::getThrId()
{
    return mThrId;
}

string & MoThread::getThrName()
{
    return mThrName;
}

int MoThread::start()
{
    if(mThrId != INVALID_THR_ID)
    {
        dbgError("Thread being running now, cannot start it again, thrId=%lu.\n", mThrId);
        return -1;
    }

    mIsRun = true;
    int ret = pthread_create(&mThrId, NULL, MoThread::runProxy, this);
    if(ret < 0)
    {
        dbgError("Create thread failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        mIsRun = false;
        mThrId = INVALID_THR_ID;
        return -2;
    }
    dbgError("create thread succeed, thrId=%lu.\n", mThrId);
    return 0;
}

bool MoThread::getRunState()
{
    return mIsRun;
}

int MoThread::stop()
{
    mIsRun = false;
    return 0;
}

int MoThread::join()
{
    if(mThrId != INVALID_THR_ID)
        pthread_join(mThrId, NULL);
    return 0;
}

void * MoThread::runProxy(void * args)
{
    if(NULL != args)
    {
        MoThread * pObj = static_cast<MoThread *>(args);
        pObj->run();
    }
    return NULL;
}

