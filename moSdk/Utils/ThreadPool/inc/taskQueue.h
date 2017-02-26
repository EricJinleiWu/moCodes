#ifndef __TASK_QUEUE_H__
#define __TASK_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <semaphore.h>

#include "moUtils.h"

typedef struct TASKNODE_t
{
    MOUTILS_TP_TASKINFO taskInfo;
    struct TASKNODE_t * prev;
    struct TASKNODE_t * next;
}TASKNODE;

typedef struct TASKDQUEUE_t
{
    TASKNODE * head;
    TASKNODE * tail;
    unsigned int taskNum;   //The number of tasks in taskQueue currently
    pthread_mutex_t mutex;
    sem_t sem;
}TASKDQUEUE;

/*
    Do init to a task queue;

    @pQueue : The task queue should be inited, should not be NULL!

    return :
        0: init OK;
        errNo : init failed, maybe input NULL, or other;
*/
int taskQueueInit(TASKDQUEUE * pQueue);

/*
    Do uninit to a task queue;

    @pQueue : The task queue should be uninited;
*/
void taskQueueUnInit(TASKDQUEUE * pQueue);

/*
    Add a new task to task queue; this will be called by user.

    @pQueue: The task queue;
    @taskInfo : The info of task which will be added;

    return : 
        0 : add OK;
        errNo : add failed;
*/
int taskQueueAddTask(TASKDQUEUE * pQueue, const MOUTILS_TP_TASKINFO taskInfo);

/* 
    Get a task from task queue; this will be called by thMgr;
    
    @pQueue: The task queue;

    return :
        pointer : point to the task node;
        NULL : get failed;
*/
TASKNODE* taskQueueGetTask(TASKDQUEUE * pQueue);

#ifdef __cplusplus
}
#endif

#endif
