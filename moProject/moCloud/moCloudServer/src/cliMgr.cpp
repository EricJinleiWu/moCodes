#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
        dump();
        
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

void CliMgr::dump()
{
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Dump CliMgr now.\n");
    pthread_mutex_lock(&mCliCtrlListLock);

    for(list<CliCtrl * >::iterator it = mCliCtrlList.begin(); it != mCliCtrlList.end(); it++)
    {
        CliCtrl * pCurNode = *it;
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "Ip=[%s], ctrlSockId=%d, ctrlPort=%d, dataSockId=%d, dataPort=%d\n",
            pCurNode->getIp().c_str(),
            pCurNode->getCtrlSockId(), pCurNode->getCtrlPort(),
            pCurNode->getDataSockId(), pCurNode->getDataPort());
    }
    
    pthread_mutex_unlock(&mCliCtrlListLock);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Dump CliMgr end.\n");
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

/*
    1.find this connect by it IP;
    2.if IP donot exist yet, its a client ctrl socket connect request, new an object, add to list;
    3.if IP exist and data socket is invalid, this is a client data socket connect request, set its info to its CliCtrl;
    4.if IP exist and two sockets all valid, its an invalid request!
*/
int CliMgr::doNewConn(struct sockaddr_in & cliAddr, const int cliSockId)
{
    pthread_mutex_lock(&mCliCtrlListLock);

    string cliIp(inet_ntoa(cliAddr.sin_addr));
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "client IP = [%s]\n", cliIp.c_str());

    for(list<CliCtrl *>::iterator it = mCliCtrlList.begin(); it != mCliCtrlList.end(); it++)
    {
        CliCtrl * pCurCliCtrl = *it;
        if(pCurCliCtrl->getIp() == cliIp)
        {
            //this ip has been exist, should check its data socket is valid or not
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                "Client(ip=[%s] being found, check its data port now.\n",
                pCurCliCtrl->getIp().c_str());

            if(pCurCliCtrl->getDataSockId() > 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "This IP has valid dataport yet! cannot get new connect request!\n");
                pthread_mutex_unlock(&mCliCtrlListLock);
                return -1;
            }

            pCurCliCtrl->setDataSockId(cliSockId);
            pCurCliCtrl->setDataPort(ntohs(cliAddr.sin_port));
            
            pthread_mutex_unlock(&mCliCtrlListLock);
            return 0;
        }
    }

    //donot find this IP in list, means this is the cliCtrl connect request
    string cliName("thread_ip=");
    cliName = cliName + inet_ntoa(cliAddr.sin_addr);

    CliCtrl * pCurCliCtrl = new CliCtrl(cliIp, cliSockId, ntohs(cliAddr.sin_port), cliName);
    pCurCliCtrl->setState(CLI_STATE_IDLE);
    pCurCliCtrl->start();
    
    mCliCtrlList.push_back(pCurCliCtrl);

    pthread_mutex_unlock(&mCliCtrlListLock);
    return 0;
}

CliMgr * CliMgrSingleton::pInstance = new CliMgr;

CliMgr * CliMgrSingleton::getInstance() 
{
    return pInstance;
}



