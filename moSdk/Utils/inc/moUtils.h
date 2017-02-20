#ifndef __MO_UTILS_H__
#define __MO_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
    The log info output format;
*/
#if 1
#define error(format, ...) printf("MO_UTILS : [%s, %s, %d ERR] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define debug(format, ...) printf("MO_UTILS : [%s, %s, %d DBG] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define error(format, ...)
#define debug(format, ...)
#endif

/**********************************************************************************
    TP is ThreaPool, "moUtils_TP_" will be prefix to ThreadPool
***********************************************************************************/

#define MOUTILS_TP_ERR_OK  0
#define MOUTILS_TP_ERR_INPUTPARAMNULL           (0x00002000)    //input param is NULL.
#define MOUTILS_TP_ERR_INITPARAMERR             (0x00002001)    //when init threadPool, initThNum larger than maxThNum
#define MOUTILS_TP_ERR_CREATETHREADFAILED       (0x00002002)    //pthread_create for work thread failed
#define MOUTILS_TP_ERR_MALLOCFAILED             (0x00002003)    //malloc for memory failed!
#define MOUTILS_TP_ERR_SEMINITFAILED            (0x00002004)    //init semaphore failed
#define MOUTILS_TP_ERR_MUTEXINITFAILED            (0x00002005)    //init mutex failed

/*
    The task API, should be given by caller;
*/
typedef void *(*pMoThTaskFunc)(void *);

/*
    To be a task to ThreadPool, should give its function pointer to do work;
    Functions should not block! Or the thread in ThreadPool which deal with this task will be blocked!
*/
typedef struct
{
    pMoThTaskFunc pTaskFunc;
    void *args4TaskFunc;
}MOUTILS_TP_TASKINFO;

/*
    Init ThreadPool;

    @thNum : how many threads will be created and be used;

    return : 
        NULL : if init failed(memory or create threads failed, and so on), return NULL, threadPool cannot be used;
        pointer : The pointer of a manager;
*/
void * moUtils_TP_init(const unsigned int thNum);

/*
    UnInit ThreadPool;

    @pTpMgr : the pointer get from moUtils_TP_init;
*/
void moUtils_TP_uninit(void *pTpMgr);

/*
    Add a new task to ThreadPool;

    @pTpMgr : The pointer get from moUtils_TP_init;
    @taskInfo : The task info;

    return : 
        0 : add ok;
        errNo : add failed.
*/
int moUtils_TP_addTask(void *pTpMgr, MOUTILS_TP_TASKINFO taskInfo);

#ifdef __cplusplus
}
#endif

#endif