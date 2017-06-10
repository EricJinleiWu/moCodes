#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>

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
    The progress of crypting, just one task being crypted one time, so a global variable being used.
*/
static int gCryptProgress = 0;

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

    if(NULL == gReqQueue->next)
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
    A callback function, which will register to moCrypt, the progress value will notify us 
    by this function;
*/
void cryptProgress(int value)
{
    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "gCryptProgress = %d, value = %d\n", 
        gCryptProgress, value);
    gCryptProgress = value;
}

static int getDstFilepath(const char *pSrcFilepath, const MOCRYPT_METHOD method, char * pDstFilepath)
{
    if(NULL == pSrcFilepath || NULL == pDstFilepath)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "Input param is NULL.\n");
        return CFT_PROT_INPUT_INVALID;
    }

    int ret = 0;
    switch(method)
    {
        case MOCRYPT_METHOD_DECRYPT:
            if(strlen(pSrcFilepath) + CFT_PROT_PLAIN_FILE_SUFFIX_LEN > MOCRYPT_FILEPATH_LEN)
            {
                moLoggerError(MOCFT_LOG_MODULE_NAME, "srcFilepath has length = %d, " \
                    "CFT_PROT_PLAIN_FILE_SUFFIX_LEN = %d, Allowed dst filepath max value is %d," \
                    "So this is an invalid srcfilepath! Too long!\n",
                    strlen(pSrcFilepath), CFT_PROT_PLAIN_FILE_SUFFIX_LEN, MOCRYPT_FILEPATH_LEN);
                ret = CFT_PROT_INPUT_INVALID;
            }
            else
            {
                memset(pDstFilepath, 0x00, MOCRYPT_FILEPATH_LEN);
                sprintf(pDstFilepath, "%s%s", pSrcFilepath, CFT_PROT_PLAIN_FILE_SUFFIX);
                moLoggerDebug(MOCFT_LOG_MODULE_NAME, "srcFilepath = [%s], dstFilepath = [%s]\n",
                    pSrcFilepath, pDstFilepath);
                ret = CFT_PROT_OK;
            }
            break;
        case MOCRYPT_METHOD_ENCRYPT:
            if(strlen(pSrcFilepath) + CFT_PROT_CIPHER_FILE_SUFFIX_LEN > MOCRYPT_FILEPATH_LEN)
            {
                moLoggerError(MOCFT_LOG_MODULE_NAME, "srcFilepath has length = %d, " \
                    "CFT_PROT_CIPHER_FILE_SUFFIX_LEN = %d, Allowed dst filepath max value is %d," \
                    "So this is an invalid srcfilepath! Too long!\n",
                    strlen(pSrcFilepath), CFT_PROT_CIPHER_FILE_SUFFIX_LEN, MOCRYPT_FILEPATH_LEN);
                ret = CFT_PROT_INPUT_INVALID;
            }
            else
            {
                memset(pDstFilepath, 0x00, MOCRYPT_FILEPATH_LEN);
                sprintf(pDstFilepath, "%s%s", pSrcFilepath, CFT_PROT_CIPHER_FILE_SUFFIX);
                moLoggerDebug(MOCFT_LOG_MODULE_NAME, "srcFilepath = [%s], dstFilepath = [%s]\n",
                    pSrcFilepath, pDstFilepath);
                ret = CFT_PROT_OK;
            }
            break;
        default:
            moLoggerError(MOCFT_LOG_MODULE_NAME, "Input method = %d, invalid!\n", method);
            ret = CFT_PROT_INPUT_INVALID;
            break;
    }

    return ret;
}

static int doBase64Request(const char * pSrcFilepath, const char * pPasswd, const MOCRYPT_METHOD method)
{
    if(NULL == pSrcFilepath || NULL == pPasswd)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "Input param is NULL.\n");
        return CFT_PROT_INPUT_INVALID;
    }

    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "srcfilepath = [%s], passwd = [%s]\n",
        pSrcFilepath, pPasswd);
    
    char pDstFilepath[MOCRYPT_FILEPATH_LEN] = {0x00};
    int ret = getDstFilepath(pSrcFilepath, method, pDstFilepath);
    if(ret != CFT_PROT_OK)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "getDstFilepath failed! srcFilepath = [%s], method = %d, ret = %d\n",
            pSrcFilepath, method, ret);
        return ret;
    }
    ret = moCrypt_BASE64_File(method, pSrcFilepath, pDstFilepath, cryptProgress);
    if(ret != 0)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "moCrypt_BASE64_File failed! ret = %d\n", ret);
        return ret;
    }

    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "moCrypt_BASE64_File done!\n");
    return CFT_PROT_OK;
}

static int doRc4Request(const char * pSrcFilepath, const char * pPasswd, const MOCRYPT_METHOD method)
{
    if(NULL == pSrcFilepath || NULL == pPasswd)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "Input param is NULL.\n");
        return CFT_PROT_INPUT_INVALID;
    }

    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "srcfilepath = [%s], passwd = [%s]\n",
        pSrcFilepath, pPasswd);

    
    char pDstFilepath[MOCRYPT_FILEPATH_LEN] = {0x00};
    int ret = getDstFilepath(pSrcFilepath, method, pDstFilepath);
    if(ret != CFT_PROT_OK)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "getDstFilepath failed! srcFilepath = [%s], method = %d, ret = %d\n",
            pSrcFilepath, method, ret);
        return ret;
    }
    
    MOCRYPT_RC4_FILEINFO rc4Info;
    memset(&rc4Info, 0x00, sizeof(MOCRYPT_RC4_FILEINFO));
    strncpy(rc4Info.pSrcFilepath, pSrcFilepath, MOCRYPT_FILEPATH_LEN);
    rc4Info.pSrcFilepath[MOCRYPT_FILEPATH_LEN - 1] = 0x00;
    strncpy(rc4Info.pDstFilepath, pDstFilepath, MOCRYPT_FILEPATH_LEN);
    rc4Info.pDstFilepath[MOCRYPT_FILEPATH_LEN - 1] = 0x00;
    rc4Info.pCallback = cryptProgress;
    strcpy((char *)rc4Info.pKey, pPasswd);
    rc4Info.keyLen = strlen((char *)rc4Info.pKey);
    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "RC4 info: srcfilepath = [%s], dstfilepath = [%s], " \
        "key = [%s], keylen = [%d]\n", rc4Info.pSrcFilepath, rc4Info.pDstFilepath, rc4Info.pKey, rc4Info.keyLen);

    ret = moCrypt_RC4_cryptFile(&rc4Info);
    if(ret != 0)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "moCrypt_RC4_cryptFile failed! ret = %d\n", ret);
        return ret;
    }

    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "moCrypt_RC4_cryptFile done!\n");
    return CFT_PROT_OK;
}

static void notifyProgress2UI(const int progress)
{
    //TODO, should notify UI to show Progress, I want to realize this function with socket;
    (void )progress;
    return ;
}

/*
    Deal with a crypt request;
*/
static int doRequest(const REQ_NODE * pReqNode)
{
    if(NULL == pReqNode)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "Input param is NULL.\n");
        return CFT_PROT_INPUT_INVALID;
    }

    //Start crypt
    int ret = 0;
    MO_CFT_ALGO algoNo = pReqNode->req.algoNo;
    switch(algoNo)
    {
//        case MO_CFT_ALGO_3DES:
//            ret = doDes3Request(pReqNode->req.srcFilepath, pReqNode->req.passwd, pReqNode->req.cryptMethod);
//            break;
        case MO_CFT_ALGO_BASE64:
            ret = doBase64Request(pReqNode->req.srcFilepath, pReqNode->req.passwd, pReqNode->req.cryptMethod);
            break;
//        case MO_CFT_ALGO_DES:
//            ret = doDesRequest(pReqNode->req.srcFilepath, pReqNode->req.passwd, pReqNode->req.cryptMethod);
//            break;
        case MO_CFT_ALGO_RC4:
            ret = doRc4Request(pReqNode->req.srcFilepath, pReqNode->req.passwd, pReqNode->req.cryptMethod);
            break;
        default:
            ret = CFT_PROT_INPUT_INVALID;
            moLoggerError(MOCFT_LOG_MODULE_NAME, "Input algoNo = %d, donot support it now.\n", algoNo);
            break;
    }

    if(ret < 0)
    {
        moLoggerError(MOCFT_LOG_MODULE_NAME, "Start crypt failed! ret = %d\n", ret);
        notifyProgress2UI(ret);
    }
    else
    {
        //Get progress, and send it to cftUI
        while(1)
        {
            moLoggerDebug(MOCFT_LOG_MODULE_NAME, "gCryptProgress = %d\n", gCryptProgress);
            notifyProgress2UI(gCryptProgress);
            if(gCryptProgress >=0 && gCryptProgress < 100)
            {
                usleep(10 * 1000);  //10ms
                continue;
            }
            else if(gCryptProgress == 100)
            {
                moLoggerDebug(MOCFT_LOG_MODULE_NAME, "crypt done!\n");
                ret = 0;
                break;
            }
            else
            {
                moLoggerDebug(MOCFT_LOG_MODULE_NAME, "gCryptProgress invalid! crypt failed!\n");
                ret = gCryptProgress;
                break;
            }
        }
    }
    
    return ret;
}

/*
    Clear the gReqQueue, should free all nodes;
    head node of this queue will not be deleted;
*/
static void clearQueue()
{
    REQ_NODE *pCurReq = gReqQueue;
    REQ_NODE *pNextReq = pCurReq->next;
    while(pNextReq != NULL && pNextReq != gReqQueue)
    {
        pCurReq = pNextReq;
        pNextReq = pNextReq->next;
        free(pCurReq);
        pCurReq = NULL;
    }

    gReqQueue->prev = NULL;
    gReqQueue->next = NULL;
}

/*
    A thread, to get requests from @gReqQueue, and do crypt to them;
*/
static void * thDoRequests(void * args)
{
    args = args;
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

            //do this request
            int ret = doRequest(pCurReq);
            if(ret != CFT_PROT_OK)
            {
                moLoggerError(MOCFT_LOG_MODULE_NAME, "doRequest failed! ret = %d\n", ret);
                //If a task has error, clear all tasks from gReqQueue, and user must deal with this error
                free(pCurReq);
                pCurReq = NULL;
                clearQueue();
                break;
            }
            moLoggerDebug(MOCFT_LOG_MODULE_NAME, "doRequest OK.\n");

            //After this request being done, just free its memory
            free(pCurReq);
            pCurReq = NULL;

            //get next request from queue
            pCurReq = deQueue();
        }
    }
    
    moLoggerInfo(MOCFT_LOG_MODULE_NAME, "thread thDoRequests stopped!\n");

    return NULL;
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
        sem_destroy(&gSem);
        moLoggerInfo(MOCFT_LOG_MODULE_NAME, "thread thDoRequests stopped.\n");

        //clear @gReqQueue then
        clearQueue();

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
        pCurNode->req.cryptMethod = pRequests[i].cryptMethod;
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
            pTmpList->prev = gReqQueue;
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
            pTmpList->prev = pCurLastNode;
        }
    }

    moLoggerDebug(MOCFT_LOG_MODULE_NAME, "All requests being added to gReqQueue.\n");

    //Send semaphore to @thDoRequest, tell it to work
    sem_post(&gSem);

    return CFT_PROT_OK;
}


