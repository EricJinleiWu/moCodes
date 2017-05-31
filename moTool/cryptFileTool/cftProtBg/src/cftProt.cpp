#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>


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
    A flag to determine thread @thDoRequests running or not;
    when its value is 0, thread donot started, or will be stoped;
    its value is 1, thread is running;
*/
typedef enum
{
    CFT_PROT_TH_RUNNING_STATE_STOP,
    CFT_PROT_TH_RUNNING_STATE_ACTIVE,
}CFT_PROT_TH_RUNNING_STATE;
static CFT_PROT_TH_RUNNING_STATE gIsThRunning = CFT_PROT_TH_RUNNING_STATE_STOP;

/*
    The thread id of thread @thDoRequests;
    when we do @cftProtUnInit, we want to stop this thread, the id is necessary;
*/
#define CFT_PROT_INVALID_TH_ID -1
static pthread_t gThId = CFT_PROT_INVALID_TH_ID;

/*
    Get a request node from @gReqQueue from head;
*/
static REQ_NODE * deQueue()
{
    if(NULL == gReqQueue)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "gReqQueue == NULL, cannot deQueue anything!\n");
        return NULL;
    }

    if(gReqQueue->next = NULL)
    {
        moLoggerDebug(MOCFT_LOG_MODULE_NAME, "gReqQueue->next == NULL, cannot deQueue anything!\n");
        return NULL;
    }

    //Get the first node from @gReqQueue
    REQ_NODE *pRet = gReqQueue->next;
    if(pRet->next == gReqQueue)
    {
        //Just one node in queue now.
        gReqQueue->next = NULL;
        gReqQueue->prev = NULL;
    }
    else
    {
        gReqQueue->next = pRet->next;
        pRet->next->prev = gReqQueue;
    }

    return pRet;
}

/*
    A thread, to get requests from @gReqQueue, and do crypt to them;
*/
static void thDoRequests(void * args)
{
    moLoggerInfo(MOCFT_LOG_MODULE_NAME, "thread thDoRequests started!\n");

    //@gIsThRunning is a flag, to determine thread running or not;
    while(gIsThRunning)
    {
        //Wait for signal
        sem_wait(&gSem);

        //Query all requests in @gReqQueue, do crypt to them;
        REQ_NODE * pCurReq = deQueue();
        while(pCurReq != NULL)
        {
            moLoggerDebug(MOCFT_LOG_MODULE_NAME, "get a request from queue.\n");

            //TODO, do this request

            //After this request being done, just free its memory
            free(pCurReq);
            pCurReq = NULL;

            //get next request from queue
            pCurReq = deQueue();
        }
    }
    
    moLoggerInfo(MOCFT_LOG_MODULE_NAME, "thread thDoRequests stopped!\n");
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
    //@gReqQueue to be the head node of this queue;
    gReqQueue = (REQ_NODE *)malloc(sizeof(REQ_NODE) * 1);
    if(NULL == gReqQueue)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "Malloc failed! size = %d, errno = %d, desc = [%s]\n",
            sizeof(REQ_NODE), errno, strerror(errno));
        return CFT_PROT_MALLOC_FAILED;
    }
    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "Malloc for gReqQueue OK.\n");
    
    memset(gReqQueue, 0x00, sizeof(REQ_NODE));
    
    //2.start @thDoRequests
    int ret = sem_init(&gSem, 0, 0);
    if(ret < 0)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "sem_init failed! ret = %d\n", ret);
        free(gReqQueue);
        gReqQueue = NULL;
        return CFT_PROT_SEM_INIT_FAILED;
    }
    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "sem_init OK.\n");
    
    gIsThRunning = CFT_PROT_TH_RUNNING_STATE_ACTIVE;
    ret = pthread_create(&gThId, NULL, thDoRequests, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "pthread_create failed! ret = %d\n", ret);
        sem_destroy(&gSem);
        free(gReqQueue);
        gReqQueue = NULL;
        gIsThRunning = CFT_PROT_TH_RUNNING_STATE_STOP;
        return CFT_PROT_CREATE_TH_FAILED;
    }
    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "create thread thDoRequests OK. \n");
    
    return CFT_PROT_OK;
}

void cftProtUnInit(void)
{
    if(NULL != gReqQueue)
    {
        //Stop thread @thDoRequests firstly
        gIsThRunning = CFT_PROT_TH_RUNNING_STATE_STOP;
        sem_post(&gSem);
        pthread_join(gThId, NULL);
        gThId = CFT_PROT_INVALID_TH_ID;
        sem_destroy(gSem);
        moLoggerInfo(MOCFT_LOG_MODULE_NAME, "thread thDoRequests stopped.\n");

        //clear @gReqQueue then
        REQ_NODE *pCurReq = gReqQueue;
        REQ_NODE *pNextReq = pCurReq->next;
        while(pNextReq != NULL && pNextReq != gReqQueue)
        {
            pCurReq = pNextReq;
            pNextReq = pNextReq->next;
            free(pCurReq);
            pCurReq = NULL;
        }

        //head node must be freeed, too
        free(gReqQueue);
        gReqQueue = NULL;

        moLoggerInfo(MOCFT_LOG_MODULE_NAME, "Free gReqQueue over.\n");
    }
    
    moLoggerInfo(MOCFT_LOG_MODULE_NAME, "cftProtUnInit over.\n");
}

int cftProtAddRequest(const CFT_REQUEST_INFO * pRequests, const unsigned int reqNum)
{
    if(NULL == pRequests || reqNum == 0)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "Input param is invalid.\n");
        return CFT_PROT_INPUT_INVALID;
    }

    //Add this request to @gReqQueue
    //Need a list, to save requests temply, if malloc failed, all requests will not add to gReqQueue;
    REQ_NODE *pTmpList = NULL;

    unsigned char isMallocOk = 1;
    unsigned int i = 0;
    for(i = 0; i < reqNum; i++)
    {
        REQ_NODE *pCurNode = NULL;
        pCurNode = (REQ_NODE *)malloc(sizeof(REQ_NODE) * 1);
        if(NULL == pCurNode)
        {
            moLoggerError(MOCFT_LOG_MODULE_NAME, "malloc failed! size = %d, errno = %d, desc = [%s]\n",
                sizeof(REQ_NODE), errno, strerror(errno));
            isMallocOk = 0;
            break;
        }

        //malloc succeed, set a request to this node
        pCurNode->req.algoNo = pRequests[i].algoNo;
        strcpy(pCurNode->req.srcFilepath, pRequests[i].srcFilepath);
        strcpy(pCurNode->req.passwd, pRequests[i].passwd);
        pCurNode->next = NULL;
        pCurNode->prev = NULL;

        //add this node to tmp list;
        if(pTmpList == NULL)
        {
            pTmpList = pCurNode;
        }
        else
        {
            if(NULL == pTmpList->prev)
            {
                //tmp list just has one node
                pTmpList->prev = pCurNode;
                pTmpList->next = pCurNode;
                pCurNode->prev = pTmpList;
                pCurNode->next = pTmpList;
            }
            else
            {
                //tmp list has more than one nodes
                REQ_NODE *pCurLastNode = pTmpList->prev;
                pTmpList->prev = pCurNode;
                pCurLastNode->next = pCurNode;
                pCurNode->prev = pCurLastNode;
                pCurNode->next = pTmpList;
            }
        }
    }

    //Some node malloc failed! all requests will not be done
    if(0 == isMallocOk)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "Some request malloc failed! all requests will not be done!\n");
        //Free pTmpList
        REQ_NODE *pCurNode = pTmpList;
        while(pCurNode != NULL)
        {
            REQ_NODE *pNextNode = pCurNode->next;
            free(pCurNode);
            pCurNode = NULL;
            pCurNode = pNextNode;
        }

        return CFT_PROT_MALLOC_FAILED;
    }
    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "All requests being malloced as nodes.\n");

    //Add these requests to the end of gReqQueue
    if(gReqQueue->prev == NULL)
    {
        //queue is empty
        gReqQueue->next = pTmpList;
        gReqQueue->prev = (pTmpList->prev == NULL) ? pTmpList : pTmpList->prev;
        if(pTmpList->prev == NULL)
        {
            pTmpList->prev = gReqQueue;
            pTmpList->next = gReqQueue;            
        }
        else
        {
            pTmpList->prev->next = gReqQueue;
            pTmpList.prev = gReqQueue;
        }
    }
    else
    {
        //queue has nodes in.
        REQ_NODE * pCurLastNode = gReqQueue->prev;
        pCurLastNode->next = pTmpList;
        gReqQueue->prev = (pTmpList->prev == NULL) ? pTmpList : pTmpList->prev;
        if(pTmpList->prev == NULL)
        {
            pTmpList->prev = pCurLastNode;
            pTmpList->next = gReqQueue;            
        }
        else
        {
            pTmpList->prev->next = gReqQueue;
            pTmpList.prev = pCurLastNode;
        }
    }

    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "All requests being added to gReqQueue.\n");

    return CFT_PROT_OK;
}


