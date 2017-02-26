#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "taskQueue.h"

int taskQueueInit(TASKDQUEUE * pQueue)
{
    if(NULL == pQueue)
    {
        error("Input param is NULL.\n");
        return MOUTILS_TP_ERR_INPUTPARAMNULL;
    }
    
    int ret = pthread_mutex_init(&(pQueue->mutex), NULL);
    if(ret != 0)
    {
        error("pthread_mutex_init failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return MOUTILS_TP_ERR_MUTEXINITFAILED;
    }

    ret = sem_init(&(pQueue->sem), 0, 0);
    if(ret != 0)
    {
        error("sem_init failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return MOUTILS_TP_ERR_SEMINITFAILED;
    }
    
    pQueue->head = NULL;    //just point to valid node, when init, do not have node, so NULL;
    pQueue->tail = NULL;    //same as pQueue->head
    pQueue->taskNum = 0;    //when init, do not has task in, so number is 0

    return MOUTILS_TP_ERR_OK;
}

void taskQueueUnInit(TASKDQUEUE * pQueue)
{
    if(pQueue != NULL)
    {
        //free memory 
        TASKNODE *pCurNode = pQueue->head;
        TASKNODE *pNextNode = NULL;
        while(pCurNode != NULL)
        {
            pNextNode = pCurNode->next;
            free(pCurNode);
            pCurNode = NULL;
            pCurNode = pNextNode;
        }

        //destroy mutex and semaphore
        pthread_mutex_destroy(&(pQueue->mutex));
        sem_destroy(&(pQueue->sem));
    }
}

/*
    New task should add to the head, because will get from tail;
*/
int taskQueueAddTask(TASKDQUEUE * pQueue, const MOUTILS_TP_TASKINFO taskInfo)
{
    if(NULL == pQueue)
    {
        error("Input param is NULL.\n");
        return MOUTILS_TP_ERR_INPUTPARAMNULL;
    }

    TASKNODE * pNewNode = NULL;
    pNewNode = (TASKNODE *)malloc(sizeof(TASKNODE) * 1);
    if(NULL == pNewNode)
    {
        error("malloc for new task node failed! errno = %d, desc = [%s]\n",
            errno, strerror(errno));
        return MOUTILS_TP_ERR_MALLOCFAILED;
    }

    //set values to this node
    memset(pNewNode, 0x00, sizeof(TASKNODE));
    memcpy(&(pNewNode->taskInfo), &taskInfo, sizeof(MOUTILS_TP_TASKINFO));
    pNewNode->prev = NULL;
    pNewNode->next = NULL;

    pthread_mutex_lock(&(pQueue->mutex));
    
    //Add this node to queue as head node
    TASKNODE *pCurHeadNode = pQueue->head;
    if(NULL == pCurHeadNode)
    {
        pQueue->head = pNewNode;
        pQueue->tail = pNewNode;
        pQueue->taskNum = 1;
    }
    else
    {
        pQueue->head = pNewNode;
        pNewNode->next = pCurHeadNode;
        pCurHeadNode->prev = pNewNode;
        pQueue->taskNum++;
    }
    
    pthread_mutex_unlock(&(pQueue->mutex));

    //Send semaphore, this semaphore will be get by thMgr, and thMgr will get task then.
    sem_post(&(pQueue->sem));

    return MOUTILS_TP_ERR_OK;
}

TASKNODE* taskQueueGetTask(TASKDQUEUE * pQueue)
{
    if(NULL == pQueue)
    {
        error("Input param is NULL.\n");
        return NULL;
    }

    pthread_mutex_lock(&(pQueue->mutex));


    TASKNODE * pLastNode = NULL;
    pLastNode = pQueue->tail;
    if(pLastNode == NULL)
    {
        error("When get task node, no task nodes being find, this is not in our logical range! check for reason!\n");
    }
    else
    {
        TASKNODE * pNewLastNode = pQueue->tail->prev;
        if(pNewLastNode == NULL)    //taskQueue is empty now.
        {
            pQueue->head = NULL;
            pQueue->tail = NULL;
            pQueue->taskNum = 0;
        }
        else
        {
            pQueue->tail = pNewLastNode;
            pQueue->taskNum--;
        }
    }
    pthread_mutex_unlock(&(pQueue->mutex));

    return pLastNode;
}

