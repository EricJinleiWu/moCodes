#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>

#include "moUtils.h"
#include "taskQueue.h"
#include "thMgr.h"

static void workTh(void *args)
{
    if(NULL == args)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "Input param is NULL.\n");
        return ;
    }
    THMGR_INFO * pThMgrInfo = (THMGR_INFO *)args;
    while(1)
    {
        sem_wait(&(pThMgrInfo->taskQueue.sem));
        //When we get a semaphore, should check is a task should be done, or threadPool will be destroyed.
        if(pThMgrInfo->state == THMGR_STATE_STOP)
        {
            //debug("pThMgrInfo->state == THMGR_STATE_STOP, work thread will exit now!\n");
            //This thread pool will be destroyed, so this thread will exit now.
            pthread_exit(NULL);
        }
        else
        {
            //get the task from taskQueue, and do it
            TASKNODE * pTaskNode = taskQueueGetTask(&(pThMgrInfo->taskQueue));
            if(NULL == pTaskNode)
            {
                moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "taskQueueGetTask failed!\n");
            }
            else
            {
                //do this task
                pTaskNode->taskInfo.pTaskFunc(pTaskNode->taskInfo.args4TaskFunc);

                //memory should be free
                free(pTaskNode);
                pTaskNode = NULL;
            }
        }
    }
}

/* 
    Init a ThMgr pointer, and return for use;
*/
void * moUtils_TP_init(const unsigned int thNum)
{   
    THMGR_INFO * pThMgrInfo = NULL;
    pThMgrInfo = (THMGR_INFO *)malloc(sizeof(THMGR_INFO) * 1);
    if(NULL == pThMgrInfo)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "Malloc for a thMgr failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return NULL;
    }
    memset(pThMgrInfo, 0x00, sizeof(THMGR_INFO));

    //init the task queue in this threadPool
    int ret = taskQueueInit(&(pThMgrInfo->taskQueue));
    if(ret != 0)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "taskQueueInit failed! ret = 0x%x\n", ret);
        free(pThMgrInfo);
        pThMgrInfo = NULL;
        return NULL;
    }

    //malloc for the head node of thList
    pThMgrInfo->pThInfoList = NULL;
    pThMgrInfo->pThInfoList = (TH_INFO *)malloc(sizeof(TH_INFO) * thNum);
    if(NULL == pThMgrInfo->pThInfoList)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "Malloc for thInfoList failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));

        taskQueueUnInit(&(pThMgrInfo->taskQueue));
    
        free(pThMgrInfo);
        pThMgrInfo = NULL;

        return NULL;
    }
    memset(pThMgrInfo->pThInfoList, 0x00, sizeof(TH_INFO) * thNum);

    //create threads and set to thMgr
    unsigned int i = 0;
    for(i = 0; i < thNum; i++)
    {
        pthread_t thId = 0;
        ret = pthread_create(&thId, NULL, (void *)workTh, (void *)pThMgrInfo);
        if(ret != 0)
        {
            moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "pthread_create failed! ret = %d, errno = %d, desc = [%s]\n", 
                ret, errno, strerror(errno));

            pThMgrInfo->state = THMGR_STATE_STOP;

            //If create failed, we should stop the running threads firstly, then do other operations
            unsigned int j = 0;
            for(j = 0; j < i; j++)
            {
                sem_post(&(pThMgrInfo->taskQueue.sem));
            }
            for(j = 0; j < pThMgrInfo->thNum; j++)
            {
                pthread_join(pThMgrInfo->pThInfoList[j].thId, NULL);
            }

            //free the list memory
            if(NULL != pThMgrInfo->pThInfoList)
            {
                free(pThMgrInfo->pThInfoList);
                pThMgrInfo->pThInfoList = NULL;
            }

            //uninit the task queue in this threadPool
            taskQueueUnInit(&(pThMgrInfo->taskQueue));

            //free the ThMgr
            if(NULL != pThMgrInfo)
            {
                free(pThMgrInfo);
                pThMgrInfo = NULL;
            }

            return NULL;
        }

        pThMgrInfo->pThInfoList[i].thId = thId;
        pThMgrInfo->pThInfoList[i].state = TH_STATE_IDLE;
    }

    //If all threads being create succeed, save it to var
    pThMgrInfo->thNum = thNum;

    return pThMgrInfo;
}

void moUtils_TP_uninit(void *pTpMgr)
{
    if(pTpMgr)
    {
        THMGR_INFO * pThMgrInfo = (THMGR_INFO *)pTpMgr;

        pThMgrInfo->state = THMGR_STATE_STOP;
        
        //Stop all threads
        unsigned int j = 0;
        for(j = 0; j < pThMgrInfo->thNum; j++)
        {
            sem_post(&(pThMgrInfo->taskQueue.sem));
        }
        for(j = 0; j < pThMgrInfo->thNum; j++)
        {
            pthread_join(pThMgrInfo->pThInfoList[j].thId, NULL);
        }

        //free the list memory
        if(NULL != pThMgrInfo->pThInfoList)
        {
            free(pThMgrInfo->pThInfoList);
            pThMgrInfo->pThInfoList = NULL;
        }

        //uninit the taskqueue
        taskQueueUnInit(&(pThMgrInfo->taskQueue));

        //free the ThMgr
        if(NULL != pThMgrInfo)
        {
            free(pThMgrInfo);
            pThMgrInfo = NULL;
        }
    }
}

int moUtils_TP_addTask(void *pTpMgr, MOUTILS_TP_TASKINFO taskInfo)
{
    if(NULL == pTpMgr)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "Input param is NULL.\n");
        return MOUTILS_TP_ERR_INPUTPARAMNULL;
    }
    
    THMGR_INFO * pThMgrInfo = (THMGR_INFO *)pTpMgr;
    int ret = taskQueueAddTask(&(pThMgrInfo->taskQueue), taskInfo);
    if(ret != 0)
    {
        moLogger(MOUTILS_LOGGER_MODULE_NAME, moLoggerLevelError, "taskQueueAddTask failed! ret = 0x%x\n", ret);
        return ret;
    }
    
    return MOUTILS_TP_ERR_OK;
}


