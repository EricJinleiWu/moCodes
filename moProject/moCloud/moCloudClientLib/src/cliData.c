#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>

#include "cliData.h"
#include "moCloudUtilsTypes.h"
#include "moCloudUtils.h"
#include "cliUtils.h"
#include "moUtils.h"

static int gDataSockId = MOCLOUD_INVALID_SOCKID;
static char gIsInited = 0;   //0,DONOT INIT; others, have inited;
static pthread_t gRecvDataThrId = MOCLOUD_INVALID_THR_ID;

static DWLD_FILE_INFO gDwldFileInfo[DWLD_TASK_MAX_NUM];


/*
    Read a header from @gDataSockId;
*/
static int getDataHeader(MOCLOUD_DATA_HEADER * pHeader)
{
    if(NULL == pHeader)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    MOCLOUD_DATA_HEADER tmp;
    memset(&tmp, 0x00, sizeof(MOCLOUD_DATA_HEADER));
    char * pTmp = (char *)&tmp;
    int startPos = 0;
    int length = sizeof(MOCLOUD_DATA_HEADER);

    //find header looply, MARK and CHECKSUM must be right
    while(1)
    {
        int readLen = readn(gDataSockId, pTmp + startPos, length);
        if(readLen != length)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "readn failed! readLen=%d, length=%d\n", readLen, length);
            return -2;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "check the header now.\n");

        int pos = moUtils_Search_BF((unsigned char *)pTmp, sizeof(MOCLOUD_DATA_HEADER),
            (unsigned char *)MOCLOUD_MARK_SERVER, strlen(MOCLOUD_MARK_SERVER));
        if(pos < 0)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Donot find MARK(%s), should find it looply.\n", MOCLOUD_MARK_SERVER);
            memmove(pTmp, pTmp + sizeof(MOCLOUD_DATA_HEADER) - strlen(MOCLOUD_MARK_SERVER), strlen(MOCLOUD_MARK_SERVER));
            startPos = strlen(MOCLOUD_MARK_SERVER);
            length = sizeof(MOCLOUD_DATA_HEADER) - strlen(MOCLOUD_MARK_SERVER);
            continue;
        }

        if(pos != 0)
        {
            //MARK donot at the beginning, should recv the left data
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Find mark, donot at the beginning, pos=%d\n", pos);
            memmove(pTmp, pTmp + pos, sizeof(MOCLOUD_DATA_HEADER) - pos);
            startPos = pos + 1;
            length = pos;
            continue;
        }

        //MARK being find, and at the beginning, should check its checksum
        if(moCloudUtilsCheck_checksumCheckValue(pTmp, sizeof(MOCLOUD_DATA_HEADER) - sizeof(unsigned char), tmp.checkSum) == 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "moCloudUtilsCheck_checksumCheckValue failed!\n");
            memset(pTmp, 0x00, sizeof(MOCLOUD_DATA_HEADER));
            startPos = 0;
            length = sizeof(MOCLOUD_DATA_HEADER);
            continue;
        }

        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
            "Get header succeed. fileId=%d, unitId=%d, bodyLen=%d\n",
            tmp.fileId, tmp.unitId, tmp.bodyLen);
        break;
    }

    memcpy(pHeader, &tmp, sizeof(MOCLOUD_DATA_HEADER));
    return 0;
}

/*
    save this data to @gDwldFileInfo;
*/
static int saveDataToBuffer(const MOCLOUD_DATA_HEADER * pHeader, const char * pBody)
{
    if(NULL == pHeader || NULL == pBody)
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&gDwldFileInfo[pHeader->fileId].writeFileThrMutex);

    if(gDwldFileInfo[pHeader->fileId].isStopWriteFileThr)
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "fileId=%d, donot in using state, will not save data.\n", pHeader->fileId);
        pthread_mutex_unlock(&gDwldFileInfo[pHeader->fileId].writeFileThrMutex);    
        return -2;
    }

    //if donot have any node in gDwldFileInfo[fileId].pDwldUnitForwardListHead, should malloc a new node
    if(gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead->next == NULL && 
        gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead->prev == NULL)
    {
        DWLD_UNIT_INFO_NODE * pNewNode = NULL;
        pNewNode = (DWLD_UNIT_INFO_NODE *)malloc(sizeof(DWLD_UNIT_INFO_NODE) * 1);
        if(NULL == pNewNode)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
            pthread_mutex_unlock(&gDwldFileInfo[pHeader->fileId].writeFileThrMutex);
            return -3;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc new node succeed.\n");
        pNewNode->dataUnitInfo.isUsed = 1;
        pNewNode->dataUnitInfo.unitId = pHeader->unitId;
        pNewNode->dataUnitInfo.bodyLen = pHeader->bodyLen;
        memcpy(pNewNode->dataUnitInfo.body, pBody, MOCLOUD_DATA_UNIT_LEN);

        gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead->next = pNewNode;
        gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead->prev = pNewNode;
        pNewNode->next = gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead;
        pNewNode->prev = gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead;

        pthread_mutex_unlock(&gDwldFileInfo[pHeader->fileId].writeFileThrMutex);
        return 0;
    }

    //find a node if exist in gDwldFileInfo[fileId]->list
    DWLD_UNIT_INFO_NODE * pCurNode = gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead->next;
    while(pCurNode != gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead)
    {
        if(pCurNode->dataUnitInfo.isUsed)
        {
            break;
        }
    }

    //all nodes being used, must malloc new node for it
    if(pCurNode->prev == gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead)
    {
        DWLD_UNIT_INFO_NODE * pNewNode = NULL;
        pNewNode = (DWLD_UNIT_INFO_NODE *)malloc(sizeof(DWLD_UNIT_INFO_NODE) * 1);
        if(NULL == pNewNode)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
            pthread_mutex_unlock(&gDwldFileInfo[pHeader->fileId].writeFileThrMutex);
            return -4;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "malloc new node succeed.\n");
        pNewNode->dataUnitInfo.isUsed = 1;
        pNewNode->dataUnitInfo.unitId = pHeader->unitId;
        pNewNode->dataUnitInfo.bodyLen = pHeader->bodyLen;
        memcpy(pNewNode->dataUnitInfo.body, pBody, MOCLOUD_DATA_UNIT_LEN);

        gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead->next = pNewNode;
        pNewNode->next = pCurNode;
        pCurNode->prev = pNewNode;
        pNewNode->prev = gDwldFileInfo[pHeader->fileId].pDwldUnitForwardListHead;

        pthread_mutex_unlock(&gDwldFileInfo[pHeader->fileId].writeFileThrMutex);
        return 0;
    }

    //pCurNode->prev is a node not being used, and not the head node, can be set value directly
    pCurNode = pCurNode->prev;
    pCurNode->dataUnitInfo.isUsed = 1;
    pCurNode->dataUnitInfo.unitId = pHeader->unitId;
    pCurNode->dataUnitInfo.bodyLen = pHeader->bodyLen;
    memcpy(pCurNode->dataUnitInfo.body, pBody, MOCLOUD_DATA_UNIT_LEN);

    pthread_mutex_unlock(&gDwldFileInfo[pHeader->fileId].writeFileThrMutex);
    return 0;
}

/*
    Use the sockId, recv data;
*/
static void * recvDataThr(void * args)
{
    args = args;
    sigset_t set;
    int ret = threadRegisterSignal(&set);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "threadRegisterSignal failed! ret=%d\n", ret);
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "threadRegisterSignal succeed.\n");
    
    struct timespec tmInterval;
    tmInterval.tv_sec = MOCLOUD_HEARTBEAT_INTEVAL;
    tmInterval.tv_nsec = 0;  
    while(1)
    {
        fd_set rFds;
        FD_ZERO(&rFds);
        FD_SET(gDataSockId, &rFds);
        ret = pselect(gDataSockId + 1, &rFds, NULL, NULL, &tmInterval, &set);
        if(ret < 0 && errno == EINTR)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "pselect recv a signal to exit.\n");
            break;
        }
        else if(ret == 0)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pselect timeout\n");
            continue;
        }
        else
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "pselect return ret=%d, errno=%d, desc=[%s]\n",
                ret, errno, strerror(errno));
        }

        while(1)
        {
            //read some data from moCloudServer in plain
            MOCLOUD_DATA_HEADER header;
            memset(&header, 0x00, sizeof(MOCLOUD_DATA_HEADER));
            ret = getDataHeader(&header);
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "getDataHeader failed! ret=%d\n", ret);
                break;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
                "getDataHeader succeed. fileId=%d, unitId=%d, bodyLen=%d\n",
                header.fileId, header.unitId, header.bodyLen);

            //get body then
            char body[MOCLOUD_DATA_UNIT_LEN];
            ret = readn(gDataSockId, body, header.bodyLen);
            if(ret != header.bodyLen)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "readn return %d, bodyLen=%d\n", ret, header.bodyLen);
                break;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "read body succeed.\n");

            //add this data unit to local memory
            ret = saveDataToBuffer(&header, body);
            if(ret != 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "saveData failed! ret=%d\n", ret);
                //This data will be dropped!
                continue;
            }
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "saveData succeed.\n");

            //notify writeFileThr to do write
            sem_post(&gDwldFileInfo[header.fileId].writeFileThrsem);
        }
    }
    
    pthread_exit(NULL);
}

/*
    Heartbeat thread being started here;
*/
static int startRecvDataThr()
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int ret = startThread(&gRecvDataThrId, &attr, recvDataThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "startThread failed! ret=%d\n", ret);
        gRecvDataThrId = MOCLOUD_INVALID_THR_ID;
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "startThread succeed.\n");
    
    return 0;
}

/*
    stop heartbeat thread here;
*/
static int stopRecvDataThr()
{
    if(gRecvDataThrId != MOCLOUD_INVALID_THR_ID)
    {
        int ret = killThread(gRecvDataThrId);
        if(ret < 0)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "killThread failed! ret=%d\n", ret);
            return -1;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "killThread succeed.\n");
    }
    return 0;
}

/*
    Write data to file;
*/
static int writeFile(const int fileId, const DWLD_UNIT_INFO * pInfo)
{
    if(NULL == pInfo)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "fileId=%d\n", fileId);

    int offset = pInfo->unitId * MOCLOUD_DATA_UNIT_LEN;
    int ret = fseek(gDwldFileInfo[fileId].fd, offset, SEEK_SET);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "fseek failed! errno=%d, desc=[%s]\n", errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "seek succeed. offset=%d\n", offset);
    
    int writeLen = fwrite(pInfo->body, 1, pInfo->bodyLen, gDwldFileInfo[fileId].fd);
    if(writeLen != pInfo->bodyLen)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "write failed! writeLen=%d, bodyLen=%d\n", writeLen, pInfo->bodyLen);
        return -3;
    }
    fflush(gDwldFileInfo[fileId].fd);
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "write succeed.\n");

    //if needed, send progress by callback function
    if(gDwldFileInfo[fileId].pProgressNotifyFunc)
    {
        double progress = ((pInfo->unitId + 1) * MOCLOUD_DATA_UNIT_LEN / gDwldFileInfo[fileId].fileLength) * 100;
        if(progress > 100)
        {
            moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, 
                "fileId=%d, unitId=%d, progress=%f, fileLength=%d, to the end of file.\n",
                fileId, pInfo->unitId, progress, gDwldFileInfo[fileId].fileLength);
            progress = 100;
        }
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Progress=%f\n", progress);
        gDwldFileInfo[fileId].pProgressNotifyFunc(fileId, pInfo->unitId);
    }
    
    return 0;
}

static int isEof(const int fileId, const int unitId)
{
    return (unitId * MOCLOUD_DATA_UNIT_LEN >= gDwldFileInfo[fileId].fileLength) ? 1 : 0;
}

/*
    @args is the fileId;
*/
static void * writeFileThr(void * args)
{
    int * pFileId = (int *)args;
    int fileId = *pFileId;
    if(fileId < 0 || fileId >= DWLD_TASK_MAX_NUM)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "fileId=%d, valid range is [%d, %d), start WriteFileThread failed!\n",
            fileId, 0, DWLD_TASK_MAX_NUM);
        return NULL;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "fileId=%d\n", fileId);

    int isExit = 0;

    while(1)
    {
        sem_wait(&gDwldFileInfo[fileId].writeFileThrsem);

        while(1)
        {
            pthread_mutex_lock(&gDwldFileInfo[fileId].writeFileThrMutex);

            if(gDwldFileInfo[fileId].isStopWriteFileThr)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                    "gDwldFileInfo[fileId].isStopWriteFileThr=%d, fileId=%d, should exit thread now.\n",
                    gDwldFileInfo[fileId].isStopWriteFileThr, fileId);

                //drop all data
                DWLD_UNIT_INFO_NODE *pCurNode = gDwldFileInfo[fileId].pDwldUnitForwardListHead->next;
                while(pCurNode != gDwldFileInfo[fileId].pDwldUnitForwardListHead && pCurNode != NULL)
                {
                    gDwldFileInfo[fileId].pDwldUnitForwardListHead->next = pCurNode->next;
                    free(pCurNode);
                    pCurNode = NULL;
                    pCurNode = gDwldFileInfo[fileId].pDwldUnitForwardListHead->next;
                }

                gDwldFileInfo[fileId].pDwldUnitForwardListHead->next = NULL;
                gDwldFileInfo[fileId].pDwldUnitForwardListHead->prev = NULL;
                gDwldFileInfo[fileId].isUsing = 0;
                
                isExit = 1;
                pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);
                break;
            }
            
            //get data from buffer
            DWLD_UNIT_INFO_NODE * pLastNode = NULL;
            pLastNode = gDwldFileInfo[fileId].pDwldUnitForwardListHead->prev;
            if(NULL == pLastNode || pLastNode->dataUnitInfo.isUsed == 0)
            {
                moLoggerWarn(MOCLOUD_MODULE_LOGGER_NAME, "Donot have valid node can be write now!\n");
                pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);
                break;
            }
            
            DWLD_UNIT_INFO curInfo;
            memset(&curInfo, 0x00, sizeof(DWLD_UNIT_INFO));
            memcpy(&curInfo, &pLastNode->dataUnitInfo, sizeof(DWLD_UNIT_INFO));
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "get curInfo now.\n");
            
            pLastNode->dataUnitInfo.isUsed = 0;
            
            //just one node in this list, do nothing is OK.
            if(gDwldFileInfo[fileId].pDwldUnitForwardListHead->prev == gDwldFileInfo[fileId].pDwldUnitForwardListHead->next)
            {
                moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "just one node in this list.\n");
                pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);
                break;
            }
            
            //move last node to head->next
            DWLD_UNIT_INFO_NODE * pNewLastNode = pLastNode->prev;
            DWLD_UNIT_INFO_NODE * pFirstNode = gDwldFileInfo[fileId].pDwldUnitForwardListHead->next;
            gDwldFileInfo[fileId].pDwldUnitForwardListHead->next = pLastNode;
            gDwldFileInfo[fileId].pDwldUnitForwardListHead->prev = pNewLastNode;
            pLastNode->next = pFirstNode;
            pLastNode->prev = gDwldFileInfo[fileId].pDwldUnitForwardListHead;
            pFirstNode->prev = pLastNode;
            pNewLastNode->next = gDwldFileInfo[fileId].pDwldUnitForwardListHead;
            
            pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);

            //write this unit to file
            int ret = writeFile(fileId, &curInfo);
            if(ret < 0)
            {
                moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "writeFile failed! ret=%d, fileId=%d\n", ret, fileId);
                continue;
            }

            //If to the end of file, should write all data to file, then free buffer, then exit thread
            if(isEof(fileId, curInfo.unitId))
            {
                moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "To the end of file!\n");
                gDwldFileInfo[fileId].isStopWriteFileThr = 1;
                continue;
            }
        }

        if(isExit)
            break;
    }
    pthread_mutex_lock(&gDwldFileInfo[fileId].writeFileThrMutex);
    gDwldFileInfo[fileId].isUsing = 0;
    pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);
    
    moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "writeFileThr exit now, fileId=%d\n", fileId);
    pthread_exit(NULL);
}

/*
    Heartbeat thread being started here;
*/
static int startWriteFileThr(const int fileId)
{
    int ret = pthread_create(&gDwldFileInfo[fileId].writeFileThrId, NULL, writeFileThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "create thread failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "create thread succeed!\n");
    
    return 0;
}

/*
    stop heartbeat thread here;
*/
static int stopWriteFileThr(const int fileId)
{
    if(gDwldFileInfo[fileId].writeFileThrId != MOCLOUD_INVALID_THR_ID)
    {
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "start stop writeFileThr now, fileId=%d.\n", fileId);
        gDwldFileInfo[fileId].isStopWriteFileThr = 1;

        sem_post(&gDwldFileInfo[fileId].writeFileThrsem);
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "semaphore sent yet.\n");

        pthread_join(gDwldFileInfo[fileId].writeFileThrId, NULL);
        moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "thread being exit.\n");

        gDwldFileInfo[fileId].writeFileThrId = MOCLOUD_INVALID_THR_ID;

        fclose(gDwldFileInfo[fileId].fd);
        gDwldFileInfo[fileId].fd = NULL;
    }
    
    return 0;
}

/*
    create socket, set socket attr;
    connect to server;
    create thread: recvDataThr, writeFileThr;
*/
int cliDataInit(const char * ip, const int port, const char * servIp, const int servPort)
{
    if(gIsInited)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "CliData has been inited, cannot init again.\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "ip=[%s], port=%d\n", ip, port);

    int ret = createSocket(ip, port, &gDataSockId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "createDataSocket failed! ret=%d, ip=[%s], port=%d\n",
            ret, ip, port);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "createDataSocket succeed.\n");

    ret = connectToServer(gDataSockId, servIp, servPort);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "connectToServer failed! ret=%d, sockId=%d, servIp=[%s], servPort=%d\n",
            ret, gDataSockId, servIp, servPort);
        destroySocket(&gDataSockId);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "connectToServer succeed.\n");

    ret = startRecvDataThr();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "startRecvDataThr failed! ret=%d\n", ret);
        destroySocket(&gDataSockId);
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "startRecvDataThr succeed.\n");

    memset(&gDwldFileInfo, 0x00, sizeof(DWLD_FILE_INFO) * DWLD_TASK_MAX_NUM);
    int i = 0;
    for(i = 0; i < DWLD_TASK_MAX_NUM; i++)
    {
        gDwldFileInfo[i].isUsing = 0;
        gDwldFileInfo[i].writeFileThrId = MOCLOUD_INVALID_THR_ID;
        sem_init(&gDwldFileInfo[i].writeFileThrsem, 0, 0);
        pthread_mutex_init(&gDwldFileInfo[i].writeFileThrMutex, NULL);
        gDwldFileInfo[i].pDwldUnitForwardListHead = NULL;
        gDwldFileInfo[i].pDwldUnitForwardListHead = (DWLD_UNIT_INFO_NODE *)malloc(sizeof(DWLD_UNIT_INFO_NODE) * 1);
        if(NULL == gDwldFileInfo[i].pDwldUnitForwardListHead)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "malloc failed! i = %d, errno=%d, desc=[%s]\n", i, errno, strerror(errno));
            break;
        }
        memset(&gDwldFileInfo[i].pDwldUnitForwardListHead->dataUnitInfo, 0x00, sizeof(DWLD_UNIT_INFO));
        gDwldFileInfo[i].pDwldUnitForwardListHead->prev = NULL;
        gDwldFileInfo[i].pDwldUnitForwardListHead->next = NULL;
    }

    if(i < DWLD_TASK_MAX_NUM)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Init gDwldFileInfo failed!\n");
        int j = 0;
        for(j = 0; j < i; j++)
        {
            sem_destroy(&gDwldFileInfo[i].writeFileThrsem);
            pthread_mutex_destroy(&gDwldFileInfo[i].writeFileThrMutex);
            free(gDwldFileInfo[i].pDwldUnitForwardListHead);
            gDwldFileInfo[i].pDwldUnitForwardListHead = NULL;
        }
        
        return -5;
    }
    
    gIsInited = 1;

    return 0;
}

/*
    recv all data from server, but just throw them away, donot write to file;
    stop recvDataThr and writeFileThr;
    close socket;
*/
int cliDataUnInit()
{
    if(gIsInited)
    {
        stopRecvDataThr();
        int i = 0;
        for(i = 0; i < DWLD_TASK_MAX_NUM; i++)
        {
            stopWriteFileThr(i);
        }
        for(i = 0; i < DWLD_TASK_MAX_NUM; i++)
        {
            gDwldFileInfo[i].isUsing = 0;
            gDwldFileInfo[i].writeFileThrId = MOCLOUD_INVALID_THR_ID;
            sem_init(&gDwldFileInfo[i].writeFileThrsem, 0, 0);
            pthread_mutex_init(&gDwldFileInfo[i].writeFileThrMutex, NULL);
            sem_destroy(&gDwldFileInfo[i].writeFileThrsem);
            pthread_mutex_destroy(&gDwldFileInfo[i].writeFileThrMutex);

            DWLD_UNIT_INFO_NODE *pCurNode = gDwldFileInfo[i].pDwldUnitForwardListHead->next;
            while(pCurNode != NULL)
            {
                gDwldFileInfo[i].pDwldUnitForwardListHead->next = pCurNode->next;
                free(pCurNode);
                pCurNode = NULL;
                pCurNode = gDwldFileInfo[i].pDwldUnitForwardListHead->next;
            }
            
            free(gDwldFileInfo[i].pDwldUnitForwardListHead);
            gDwldFileInfo[i].pDwldUnitForwardListHead = NULL;
        }
        destroySocket(&gDataSockId);
        gIsInited = 0;
    }

    return 0;
}

/*
    To @gDwldFileInfo, insert a new node;
*/
static int insertFileDwldTask(const int fileId, const MOCLOUD_FILEINFO_KEY key, 
    const size_t filesize, const char *pLocalFilepath)
{
    if(NULL == pLocalFilepath || fileId < 0 || fileId >= DWLD_TASK_MAX_NUM)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is invalid!\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "fileId=%d, localpath=[%s], filetype=%d, filename=[%s], filesize=%d\n",
        fileId, pLocalFilepath, key.filetype, key.filename, filesize);

    pthread_mutex_lock(&gDwldFileInfo[fileId].writeFileThrMutex);
    
    //open file is neccessary
    if(gDwldFileInfo[fileId].fd != NULL)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "fd!=NULL!\n");
        fclose(gDwldFileInfo[fileId].fd);
        gDwldFileInfo[fileId].fd = NULL;
    }
    gDwldFileInfo[fileId].fd = fopen(pLocalFilepath, "ab+");
    if(NULL == gDwldFileInfo[fileId].fd)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Open failed! filepath=[%s], errno=%d, desc=[%s]\n",
            pLocalFilepath, errno, strerror(errno));
        pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "Open file succeed.\n");
    
    //gDwldFileInfo[fileId].writeFileThrId will be set in this function
    gDwldFileInfo[fileId].isStopWriteFileThr = 0;
    int ret = startWriteFileThr(fileId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "startWriteFileThr failed! ret=%d\n", ret);
        fclose(gDwldFileInfo[fileId].fd);
        gDwldFileInfo[fileId].fd = NULL;
        pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "startWriteFileThr start succeed.\n");

    memcpy(&gDwldFileInfo[fileId].fileKey, &key, sizeof(MOCLOUD_FILEINFO_KEY));
    gDwldFileInfo[fileId].fileLength = filesize;
    strncpy(gDwldFileInfo[fileId].localFilepath, pLocalFilepath, MOCLOUD_FILEPATH_MAXLEN);
    gDwldFileInfo[fileId].localFilepath[MOCLOUD_FILEPATH_MAXLEN - 1] = 0x00;
    gDwldFileInfo[fileId].fd = NULL;
    
    pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);

    return 0;
}

#if 0
/*
    stop thread;
    delete all buffer node;
    set isUsing flag;
*/
static void delFileDwldTask(const int fileId)
{
    int ret = stopWriteFileThr(fileId);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "stopWriteFileThr failed! ret=%d\n", ret);
    }
    else
    {
        moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "stopWriteFileThr succeed.\n");
    }

    pthread_mutex_lock(&gDwldFileInfo[fileId].writeFileThrMutex);
    
    DWLD_UNIT_INFO_NODE *pCurNode = gDwldFileInfo[fileId].pDwldUnitForwardListHead->next;
    while(pCurNode != gDwldFileInfo[fileId].pDwldUnitForwardListHead && pCurNode != NULL)
    {
        gDwldFileInfo[fileId].pDwldUnitForwardListHead->next = pCurNode->next;
        free(pCurNode);
        pCurNode = NULL;
        pCurNode = gDwldFileInfo[fileId].pDwldUnitForwardListHead->next;
    }

    gDwldFileInfo[fileId].pDwldUnitForwardListHead->next = NULL;
    gDwldFileInfo[fileId].pDwldUnitForwardListHead->prev = NULL;
    gDwldFileInfo[fileId].isUsing = 0;
    
    pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);
}
#endif
/*
    Check this file being dwlding or not;
    Get a fileId of this file;
    Subscribe this file dwlding task to @gDwldFileInfo;
    Start a writeFileThr to this file;
*/
int cliDataStartDwld(const MOCLOUD_FILEINFO_KEY key, const size_t filesize, const char * pLocalFilepath,
    int * pFileId, notifyDwldProgressCallback pProgressNotifyFunc)
{
    if(NULL == pLocalFilepath)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "Filetype=%d, filename=[%s], filesize=%d, localFilepath=[%s]\n",
        key.filetype, key.filename, filesize, pLocalFilepath);

    int i = 0;
    for(i = 0; i < DWLD_TASK_MAX_NUM; i++)
    {
        if(gDwldFileInfo[i].isUsing && 
            0 == strcmp(gDwldFileInfo[i].fileKey.filename, key.filename) && 
            gDwldFileInfo[i].fileKey.filetype == key.filetype)
        {
            moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
                "filetype=%d, filename=[%s], i=%d, task has been dwlding.\n",
                key.filetype, key.filename, i);
            return -2;
        }
    }

    for(i = 0; i < DWLD_TASK_MAX_NUM; i++)
    {
        pthread_mutex_lock(&gDwldFileInfo[i].writeFileThrMutex);
        if(!gDwldFileInfo[i].isUsing)
        {
            moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "i=%d, donot used yet.\n", i);
            //set it templly.
            gDwldFileInfo[i].isUsing = 1;
            pthread_mutex_unlock(&gDwldFileInfo[i].writeFileThrMutex);
            break;
        }
    }

    if(i >= DWLD_TASK_MAX_NUM)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "Donot have unused fileId, cannot start new dwld task.\n");
        return -3;
    }
    int fileId = i;
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "fileId=%d\n", fileId);
    
    int ret = insertFileDwldTask(fileId, key, filesize, pLocalFilepath);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, "insertFileDwldTask failed! ret=%d\n", ret);
        pthread_mutex_lock(&gDwldFileInfo[fileId].writeFileThrMutex);
        gDwldFileInfo[fileId].isUsing = 0;
        pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "insertFileDwldTask succeed. filetype=%d, filename=[%s], fileId=%d\n",
        key.filetype, key.filename, fileId);

    gDwldFileInfo[fileId].pProgressNotifyFunc = pProgressNotifyFunc;
    *pFileId = fileId;

    return 0;
}

/*
    Get its fileId;
    Set flag, after this, 
        all data recv from server will not write to file, just drop them;
        writeFileThr stopped;
    DisSubscribe to gpDwldFileInfoList;
    Free its fileId and other resources;
    
*/
int cliDataStopDwld(const int fileId)
{
    if(fileId < 0 || fileId >= DWLD_TASK_MAX_NUM)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "FileId=%d, invalid one! valid range is [0, %d).\n", fileId, DWLD_TASK_MAX_NUM);
        return -1;
    }

    if(!gDwldFileInfo[fileId].isUsing)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "fileId=%d, donot in using! cannot stop dwld!\n", fileId);
        return -2;
    }

    stopWriteFileThr(fileId);

    pthread_mutex_lock(&gDwldFileInfo[fileId].writeFileThrMutex);
    gDwldFileInfo[fileId].isUsing = 0;
    pthread_mutex_unlock(&gDwldFileInfo[fileId].writeFileThrMutex);

    return 0;
}



