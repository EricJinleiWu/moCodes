#ifndef __THMGR_H__
#define __THMGR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <semaphore.h>

#include "taskQueue.h"

typedef enum
{
    TH_STATE_IDLE,  //thread is idle, can be used
    TH_STATE_RUNNING,   //thread is running, cannot be used
}TH_STATE;

typedef struct
{
    pthread_t thId;
    TH_STATE state;
}TH_INFO;

typedef enum
{
    THMGR_STATE_RUN,
    THMGR_STATE_STOP
}THMGR_STATE;

typedef struct
{
    //A list to save all threads info
    TH_INFO *pThInfoList;
    //Var, to limit the thMgr size;
    unsigned int thNum;
    //a task queue being used in this thread pool
    TASKDQUEUE taskQueue;
    //when uninit this threadpool, use this flag to tell all threads to stop
    THMGR_STATE state;
}THMGR_INFO;

#ifdef __cplusplus
}
#endif

#endif
