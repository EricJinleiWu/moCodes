#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>


#include "moLogger.h"
#include "cftProt.h"

#ifndef MOCFT_LOG_MODULE_NAME
#define MOCFT_LOG_MODULE_NAME "moCft"
#endif

typedef struct REQ_NODE_t
{
    CFT_REQUEST_INFO req;
    struct REQ_NODE_t *prev;
    struct REQ_NODE_t *next;
}REQ_NODE;

//The head node of queue;
static REQ_NODE *gReqQueue = NULL;

//The semaphore being used with @thDoRequests;
static sem_t gSem;

/*
    A thread, to get requests from @gReqQueue, and do crypt to them;
*/
static void thDoRequests(void * args)
{
    
}

/*
    Init @gReqQueue;
    start thread thDoRequests;
*/
int cftProtInit(void)
{
    //1.init @gReqQueue
    if(NULL != gReqQueue)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "CFT PROT module has inited yet! donot init it again!\n");
        return CFT_PROT_INITED_YET;
    }

    //2.TODO, start @thDoRequests

    
    return 0;
}

void cftProtUnInit(void)
{
    ;
}

int cftProtAddRequest(const CFT_REQUEST_INFO * pRequests, const unsigned int reqNum)
{
    return 0;
}


