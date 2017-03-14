#ifndef __MO_UTILS_H__
#define __MO_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moLogger.h"

#define MOUTILS_LOGGER_MODULE_NAME  "MOUTILS"

/**********************************************************************************
    TP is ThreaPool, "moUtils_TP_" will be prefix to ThreadPool
***********************************************************************************/

#define MOUTILS_TP_ERR_OK  0
#define MOUTILS_TP_ERR_INPUTPARAMNULL           (0x00010000)    //input param is NULL.
#define MOUTILS_TP_ERR_INITPARAMERR             (0x00010001)    //when init threadPool, initThNum larger than maxThNum
#define MOUTILS_TP_ERR_CREATETHREADFAILED       (0x00010002)    //pthread_create for work thread failed
#define MOUTILS_TP_ERR_MALLOCFAILED             (0x00010003)    //malloc for memory failed!
#define MOUTILS_TP_ERR_SEMINITFAILED            (0x00010004)    //init semaphore failed
#define MOUTILS_TP_ERR_MUTEXINITFAILED          (0x00010005)    //init mutex failed

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



/**********************************************************************************
    "moUtils_File_" will be prefix to File operation
***********************************************************************************/

#define MOUTILS_FILE_ERR_OK                         (0x00000000)
#define MOUTILS_FILE_ERR_INPUTPARAMNULL             (0 - 0x00011000)    //input param is NULL.
#define MOUTILS_FILE_ERR_MALLOCFAILED               (0 - 0x00011001)    //malloc for memory failed!
#define MOUTILS_FILE_ERR_OPENFAILED                 (0 - 0x00011002)    //open for reading or writing failed!
#define MOUTILS_FILE_ERR_NOTFILE                    (0 - 0x00011003)    //Input path point to a directory
#define MOUTILS_FILE_ERR_STATFAILED                 (0 - 0x00011004)    //Input path point to a directory


typedef struct
{
    char * pDirpath;
    char * pFilename;
}MOUTILS_FILE_DIR_FILENAME;

/*
    To mean two filepath is same or not in abstract format;
*/
typedef enum
{
    MOUTILS_FILE_ABSPATH_STATE_ERR,
    MOUTILS_FILE_ABSPATH_STATE_SAME,
    MOUTILS_FILE_ABSPATH_STATE_DIFF
}MOUTILS_FILE_ABSPATH_STATE;

/*
    Get the size of file which has path @pFilepath;

    pFilepath : The path of file;

    return :
        0&0+ : The size in bytes;
        0- : Error!
*/
int moUtils_File_getSize(const char *pFilepath);

/*
    Get the abstract filepath of @pFilepath;
    just like, @pFilepath is ./a.txt, will return /home/.../a.txt;

    return : 
        NULL if error ocurred;

    Must free this ret pointer by yourself!
*/
char * moUtils_File_getAbsFilePath(const char *pFilepath);

/*
    Get the abstract of @pDirpath;
    The difference to moUtils_File_getAbsFilePath is : this funciton input a directory!

    return NULL if input is not a directory, or directory not exist, or malloc ret failed, and so on.

    Must free this ret pointer by yourself!
*/
char * moUtils_File_getAbsDirPath(const char *pDirpath);

/*
    Parse @pFilepath, and get its directory and filename sepratorlly;

    return : 
        0 : get succeed, and save dirpath and filename to @pDirName;
        0-: get failed, maybe input not a filepath, maybe malloc failed, and so on.
*/
int moUtils_File_getDirAndFilename(const char *pFilepath, MOUTILS_FILE_DIR_FILENAME * pInfo);

/*
    Check the two files is same file or not in abstrace filepath level;

    return : 
        0 : check OK, same or not will be set in @pState;
        0-: check failed!
*/
int moUtils_File_getFilepathSameState(const char *pSrcFilepath, const char *pDstFilepath, 
    MOUTILS_FILE_ABSPATH_STATE * pState);

#ifdef __cplusplus
}
#endif

#endif
