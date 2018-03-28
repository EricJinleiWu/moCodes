#ifndef __CLI_DATA_H__
#define __CLI_DATA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moCloudUtilsTypes.h"

typedef void (*notifyDwldProgressCallback)(double progress);


/*
    We define, mostly 8 files can be dwld at the same time!
*/
#define DWLD_TASK_MAX_NUM   8

typedef struct
{
    char isUsed;    //0, donot used; else, used;
    int unitId;
    int bodyLen;
    char body[MOCLOUD_DATA_UNIT_LEN];
}DWLD_UNIT_INFO;

typedef struct __DWLD_UNIT_INFO
{
    DWLD_UNIT_INFO dataUnitInfo;
    struct __DWLD_UNIT_INFO * prev;
    struct __DWLD_UNIT_INFO * next;
}DWLD_UNIT_INFO_NODE;

typedef struct
{
    char isUsing;   //0, not using, can be used; 1, being used, cannot used again.
    
    MOCLOUD_FILEINFO_KEY fileKey;
    size_t fileLength;
    char localFilepath[MOCLOUD_FILEPATH_MAXLEN];
    FILE * fd;

    notifyDwldProgressCallback pProgressNotifyFunc;
    
    pthread_t writeFileThrId;   //the thread id of writeFileThr
    char isStopWriteFileThr;    //1, thread should stop; 0, thread can still running;
    sem_t writeFileThrsem;  //semaphore, using in writeFileThr;
    pthread_mutex_t writeFileThrMutex;
    DWLD_UNIT_INFO_NODE * pDwldUnitForwardListHead; //head node of this forward list
    
}DWLD_FILE_INFO;

/*
    create socket, set socket attr;
    connect to server;
    create thread: recvDataThr, writeFileThr;
*/
int cliDataInit(const char * ip, const int port, const char * servIp, const int servPort);

/*
    recv all data from server, but just throw them away, donot write to file;
    stop recvDataThr and writeFileThr;
    close socket;
*/
int cliDataUnInit();

/*
    Start dwld a file;
*/
int cliDataStartDwld(const MOCLOUD_FILEINFO_KEY key, const size_t filesize, const char * pLocalFilepath,
    int * pFileId, notifyDwldProgressCallback pProgressNotifyFunc);

/*
    Stop dwld;
    When call this function, if dwlding now, will stop it, and delete local file!!!
*/
int cliDataStopDwld(const int fileId);

#ifdef __cplusplus
}
#endif

#endif
