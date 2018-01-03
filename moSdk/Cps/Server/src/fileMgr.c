#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "fileMgr.h"
#include "moLogger.h"
#include "moCpsUtils.h"

#define FM_MEM_INC_SIZE    (1 * 1024 * 1024)   //each time, memory increased 1M

typedef enum
{
    FM_CHECKING_STATE_NORMAL,    //checking thread, find this file in normal state, do nothing to it;
    FM_CHECKING_STATE_DELETE,   //checking thread, find this file has been deleted, set this state, then we will delete all of them one time
}FM_CHECKING_STATE;

static const char gSubDirName[MOCPS_FILETYPE_MAX][16] = {
    "Videos", "Audios", "Pictures", "Others"
    };

typedef struct
{
    MOCPS_BASIC_FILEINFO basicInfo;
    FILE * fp;  //read handler
    int readHdrNum; //reader number, after all readers donot read this file, close it.
    FM_CHECKING_STATE checkingState;
}FM_FILEINFO;

typedef struct _FM_FILEINFO_NODE
{
    FM_FILEINFO info;
    struct _FM_FILEINFO_NODE * next;
}FM_FILEINFO_NODE;

typedef struct
{
    char dirPath[DIRPATH_MAXLEN];
    FM_FILEINFO_NODE *pFileList[MOCPS_FILETYPE_MAX];
    int fileNum[MOCPS_FILETYPE_MAX]; //Each type, have how many files, being set in this array, in sequece same as @gSubDirName
    pthread_mutex_t mutex;
}FM_DIRFILE_LIST;

/*
    Call by outsider;
    @pFileInfo in xml format, include all file info;
*/
typedef struct
{
    char dirPath[DIRPATH_MAXLEN];
    int sumFileNum;
    int fileInfoLen;
    //malloc some bytes memory to save fileinfo, larger than fileInfoLen is neccessary, 
    //this can help us to avoid malloc+free each time we find size changed
    int curMemSize; 
    //cannot malloc+free each time we find the fileinfo changed, this too waste our memory
    char *pFileInfo;    
    pthread_mutex_t mutex;
}FM_DIRFILEINFO;

/* Use this by this module itself, save all files info in a list; */
static FM_DIRFILE_LIST gDirFileList;
/* global var, to show FM info to outside */
static FM_DIRFILEINFO gDirFileInfo;
/* flag, to sign this module being inited or not; */
static char gIsInited = 0;
/*
    1 : checkDirThr running;
    0 : checkDirThr stopped;
*/
static int gCheckDirThrRunning = 0;

static void unInitDirfileList();

static void freeDirfileList(const int filetype);


/*
    Check input path being a directory or not;
    Only be a directory, return 1;
    not a directory, or other errors, return 0;
*/
static int isDir(const char *pDirpath, const char *pSubdirName)
{
    if(NULL == pDirpath || NULL == pSubdirName)
        return 0;
    
    char subdirpath[FILEPATH_MAXLEN] = {0X00};
    memset(subdirpath, 0x00, FILEPATH_MAXLEN);
    snprintf(subdirpath, FILEPATH_MAXLEN, "%s/%s", pDirpath, pSubdirName);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
        "dirpath is [%s], subdirname is [%s], subdirpath is [%s]\n", 
        pDirpath, pSubdirName, subdirpath);

    struct stat curStat;
    int ret = stat(subdirpath, &curStat);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "stat failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return 0;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "stat ok.\n");

    return S_ISDIR(curStat.st_mode) ? 1 : 0;
}

/*
    Check input path being a regular file or not;
    Only be a regular file, return 1;
    not a regular, or other errors, return 0;
*/
static int isRegFile(const char *pDirpath, const char *pSubFileName)
{
    if(NULL == pDirpath || NULL == pSubFileName)
        return 0;
    
    char subfilepath[FILEPATH_MAXLEN] = {0X00};
    memset(subfilepath, 0x00, FILEPATH_MAXLEN);
    snprintf(subfilepath, FILEPATH_MAXLEN, "%s/%s", pDirpath, pSubFileName);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
        "dirpath is [%s], subdirname is [%s], subdirpath is [%s]\n", 
        pDirpath, pSubFileName, subfilepath);

    struct stat curStat;
    int ret = stat(subfilepath, &curStat);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "stat failed! ret = %d, errno = %d, desc = [%s]\n",
            ret, errno, strerror(errno));
        return 0;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "stat ok.\n");

    return S_ISREG(curStat.st_mode) ? 1 : 0;
}

/*
    Get sub dir path append on @filetype;
*/
static int getSubdirPath(const int filetype, char *pSubdirPath)
{
    if(filetype < 0 || filetype >= MOCPS_FILETYPE_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "filetype=%d, donot in range [0, %d)\n",
            filetype, MOCPS_FILETYPE_MAX);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "filetype=%d\n", filetype);

    memset(pSubdirPath, 0x00, FILEPATH_MAXLEN);
    sprintf(pSubdirPath, "%s/%s", gDirFileList.dirPath, gSubDirName[filetype]);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "pSubdirPath=[%s]\n", pSubdirPath);
    return 0;
}

static int getFileInfo(const char *pDirpath, const char *pFilename, const int filetype, FM_FILEINFO *pInfo)
{
    if(NULL == pDirpath || NULL == pFilename || NULL == pInfo)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "dirpath=[%s], filename=[%s]\n",
        pDirpath, pFilename);

    char filepath[FILEPATH_MAXLEN] = {0x00};
    memset(filepath, 0x00, FILEPATH_MAXLEN);
    sprintf(filepath, "%s/%s", pDirpath, pFilename);
    filepath[FILEPATH_MAXLEN - 1] = 0x00;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "filepath=[%s]\n", filepath);

    struct stat curStat;
    memset(&curStat, 0x00, sizeof(struct stat));
    int ret = stat(filepath, &curStat);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "stat failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "stat ok.\n");

    strncpy(pInfo->basicInfo.filename, pFilename, FILENAME_MAXLEN);
    pInfo->basicInfo.filename[FILENAME_MAXLEN - 1] = 0x00;
    pInfo->basicInfo.size = curStat.st_size;
    pInfo->basicInfo.type = filetype;
    pInfo->fp = NULL;
    pInfo->readHdrNum = 0;
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "get its info!\n");
    return 0;
}

/*
    Enter the sub dir, get all regular files, get its info, set to list;
*/
static int getSubDirFilelist(const int filetype)
{
    //1.get sub dir path
    char subdirpath[FILEPATH_MAXLEN] = {0x00};
    int ret = getSubdirPath(filetype, subdirpath);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "getSubdirPath failed! ret = %d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "getSubdirPath ok.\n");

    //2.get which list to be set this time
    FM_FILEINFO_NODE *pHeadNode = gDirFileList.pFileList[filetype];
    if(pHeadNode == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "filetype=%d, pHeadNode == NULL, check for why!\n", filetype);
        return -2;
    }
    else
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find the header pointer!\n");

    //3.looply get files in this sub dir, and set its info to list
    DIR *pSubDir = opendir(subdirpath);
    if(NULL == pSubDir)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "opendir failed! filepath=[%s], errno=%d, desc=[%s]\n",
            subdirpath, errno, strerror(errno));
        return -4;
    }
    moLoggerError(MOCPS_MODULE_LOGGER_NAME, "opendir [%s] ok.\n", subdirpath);
    
    FM_FILEINFO_NODE * pCurNode = pHeadNode;
    FM_FILEINFO_NODE * pNewNode = NULL;
    struct dirent * pCurFile = NULL;
    while((pCurFile = readdir(pSubDir)) != NULL)
    {
        if(0 == strcmp(pCurFile->d_name, ".") || 0 == strcmp(pCurFile->d_name, ".."))
            continue;
        if(isRegFile(subdirpath, pCurFile->d_name))
        {
            //add this file info to list
            FM_FILEINFO info;
            ret = getFileInfo(subdirpath, pCurFile->d_name, filetype, &info);
            if(ret < 0)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "getFileInfo failed! ret = %d\n", ret);
                return -5;
            }
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "getFileInfo ok\n");

            pNewNode = (FM_FILEINFO_NODE *)malloc(sizeof(FM_FILEINFO_NODE) * 1);
            if(NULL == pNewNode)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n",
                    errno, strerror(errno));
                return -6;
            }
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc ok.\n");

            memcpy(&pNewNode->info, &info, sizeof(FM_FILEINFO));
            pNewNode->next = NULL;

            pCurNode->next = pNewNode;
            pCurNode = pNewNode;
            pNewNode = NULL;

            gDirFileList.fileNum[filetype]++;
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "New file being saved in local memory, " \
                "file name=[%s], file type = %d(%s), filenum of this type=[%d]\n",
                pCurFile->d_name, filetype, gSubDirName[filetype], gDirFileList.fileNum[filetype]);
        }
        else
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "in dir [%s], file [%s] donot a regular file!\n",
                subdirpath, pCurFile->d_name);
        }
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Get file info list ok!\n");
    closedir(pSubDir);
    pSubDir = NULL;
    
    return 0;
}

/*
    When a sub dir donot exist, use this function, create it, and init its header node for its list;
*/
static int createSubDir(const int filetype)
{
    //1.get sub dir path
    char subdirpath[FILEPATH_MAXLEN] = {0x00};
    int ret = getSubdirPath(filetype, subdirpath);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "getSubdirPath failed! ret = %d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "getSubdirPath ok.\n");

    ret = mkdir(subdirpath, S_IRWXU | S_IRWXG | S_IRWXO); //0777
    if(ret != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "mkdir failed! ret=%d, dir=[%s], errno=%d, desc=[%s]\n",
            ret, subdirpath, errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "mkdir succeed. dir=[%s]\n", subdirpath);

    FM_FILEINFO_NODE *pHeadNode = NULL;
    pHeadNode = gDirFileList.pFileList[filetype];
    if(pHeadNode != NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "When create sub directory, its header node is not NULL! filetype=%d\n", filetype);
        return -4;
    }

    //start malloc for it
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "filetype=%d, start malloc header node for it.\n",
        filetype);
    pHeadNode = (FM_FILEINFO_NODE *)malloc(sizeof(FM_FILEINFO_NODE) * 1);
    if(pHeadNode == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n",
            errno, strerror(errno));
        return -5;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc ok!\n");
    memset(&pHeadNode->info, 0x00, sizeof(FM_FILEINFO_NODE));
    pHeadNode->next = NULL;
    gDirFileList.fileNum[filetype] = 0;
    return 0;
}

/*
    Check input directory, include sub dir we need or not("Video","Audio", and so on)
    Use bit to do signiness;
*/
static int getSubdirExistState(const char *pDirpath, int *state)
{
    DIR * pDir = opendir(pDirpath);
    if(pDir == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "opendir [%s] failed! errno=%d, desc=[%s]\n",
            pDirpath, errno, strerror(errno));
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "opendir [%s] ok.\n", pDirpath);

    int existState = 0;
    struct dirent *pSubdir = NULL;
    while((pSubdir = readdir(pDir)) != NULL)
    {
        if(0 == strcmp(pSubdir->d_name, ".") || 0 == strcmp(pSubdir->d_name, ".."))
            continue;
        if(0 == strcmp(pSubdir->d_name, gSubDirName[MOCPS_FILETYPE_VIDEO]))
        {
            if(isDir(pDirpath, pSubdir->d_name))
                existState |= 1 << MOCPS_FILETYPE_VIDEO;
            else
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                    "We encounter an error! A file named [%s], we just use this name to be our sub dir!\n",
                    pSubdir->d_name);
                closedir(pDir);
                return -2;
            }
        }
        else if(0 == strcmp(pSubdir->d_name, gSubDirName[MOCPS_FILETYPE_AUDIO]))
        {
            if(isDir(pDirpath, pSubdir->d_name))
                existState |= 1 << MOCPS_FILETYPE_AUDIO;
            else
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                    "We encounter an error! A file named [%s], we just use this name to be our sub dir!\n",
                    pSubdir->d_name);
                closedir(pDir);
                return -3;
            }
        }
        else if(0 == strcmp(pSubdir->d_name, gSubDirName[MOCPS_FILETYPE_PIC]))
        {
            if(isDir(pDirpath, pSubdir->d_name))
                existState |= 1 << MOCPS_FILETYPE_PIC;
            else
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                    "We encounter an error! A file named [%s], we just use this name to be our sub dir!\n",
                    pSubdir->d_name);
                closedir(pDir);
                return -4;
            }
        }
        else if(0 == strcmp(pSubdir->d_name, gSubDirName[MOCPS_FILETYPE_OTHERS]))
        {
            if(isDir(pDirpath, pSubdir->d_name))
                existState |= 1 << MOCPS_FILETYPE_OTHERS;
            else
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
                    "We encounter an error! A file named [%s], we just use this name to be our sub dir!\n",
                    pSubdir->d_name);
                closedir(pDir);
                return -5;
            }
        }
        else
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                "sub dir name [%s], donot deal with it now.\n", pSubdir->d_name);
        continue;
    }
    closedir(pDir);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "existState = %d\n", existState);
    *state = existState;
    return 0;
}

/*
    1.Do init to @gDirfileList;
    2.parse the directory, and get its file info to @gDirfileList;
*/
static int initDirfileList(const char *pDirpath)
{
    strcpy(gDirFileList.dirPath, pDirpath);
    pthread_mutex_init(&gDirFileList.mutex, NULL);
    int i = 0;
    for(i = 0; i < MOCPS_FILETYPE_MAX; i++)
    {
        gDirFileList.pFileList[i] = NULL;
        gDirFileList.pFileList[i] = (FM_FILEINFO_NODE *)malloc(sizeof(FM_FILEINFO_NODE) * 1);
        if(gDirFileList.pFileList[i] == NULL)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "malloc failed! errno=%d, desc=[%s]\n",
                errno, strerror(errno));
            int j = 0;
            for(j = 0; j < i; j++)
            {
                free(gDirFileList.pFileList[i]);
                gDirFileList.pFileList[i] = NULL;
            }
        }
        gDirFileList.fileNum[i] = 0x00;
    }
    
    int subdirExistState = 0;
    int ret = getSubdirExistState(pDirpath, &subdirExistState);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "getSubdirExistState failed! ret = %d\n", ret);
        int filetype = 0;
        for(filetype = 0; filetype < MOCPS_FILETYPE_MAX; filetype++)
        {
            freeDirfileList(filetype);
        }
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "getSubdirExistState ok.\n");

    int filetype = 0;
    for(; filetype < MOCPS_FILETYPE_MAX; filetype++)
    {
        if(subdirExistState & (1 << filetype))
        {
            //Sub dir exist, get its file list
            ret = getSubDirFilelist(filetype);
            if(ret < 0)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "getSubDirFilelist failed! ret = %d, filetype = %d(%s)\n",
                    ret, filetype, gSubDirName[filetype]);
                break;
            }
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                "getSubDirFilelist ok, filetype=%d(%s)\n", filetype, gSubDirName[filetype]);
        }
        else
        {
            //Sub dir donot exist, create this sub dir, and set its list to NULL;
            ret = createSubDir(filetype);
            if(ret < 0)
            {
                moLoggerError(MOCPS_MODULE_LOGGER_NAME, "createSubDir failed! ret = %d, filetype = %d(%s)\n",
                    ret, filetype, gSubDirName[filetype]);
                break;
            }
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, 
                "createSubDir ok, filetype=%d(%s)\n", filetype, gSubDirName[filetype]);
        }
    }

    if(ret != 0)
    {
        unInitDirfileList();
        return -2;
    }

    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "initDirfileList succeed!\n");
    return 0;
}

static void freeDirfileList(const int filetype)
{
    FM_FILEINFO_NODE * pHeadNode = NULL;
    pHeadNode = gDirFileList.pFileList[filetype];
    if(pHeadNode != NULL)
    {
        FM_FILEINFO_NODE * pCurNode = pHeadNode;
        FM_FILEINFO_NODE * pNextNode = pCurNode->next;
        while(pNextNode != NULL)
        {
            pCurNode = pNextNode;
            pNextNode = pNextNode->next;
            free(pCurNode);
            pCurNode = NULL;
        }
        free(pHeadNode);
        pHeadNode = NULL;
    }
}

static void unInitDirfileList()
{
    int filetype = 0;
    for(; filetype < MOCPS_FILETYPE_MAX; filetype++)
    {
        freeDirfileList(filetype);
    }
    memset(gDirFileList.fileNum, 0x00, sizeof(int) * MOCPS_FILETYPE_MAX);
    pthread_mutex_destroy(&gDirFileList.mutex);
    memset(gDirFileList.dirPath, 0x00, DIRPATH_MAXLEN);
}

/*
    Read dir file list, set its file info to a char array, we send this char array to caller;
*/
static int initDirfileInfo()
{
    strncpy(gDirFileInfo.dirPath, gDirFileList.dirPath, DIRPATH_MAXLEN);
    gDirFileInfo.dirPath[DIRPATH_MAXLEN - 1] = 0x00;
    pthread_mutex_init(&gDirFileInfo.mutex, NULL);

    //calc how many memory we need to save all files info
    int sumFileNum = 0;
    int i = 0;
    for(; i < MOCPS_FILETYPE_MAX; i++)
        sumFileNum += gDirFileList.fileNum[i];
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "sum file num = %d\n", sumFileNum);
    gDirFileInfo.sumFileNum = sumFileNum;
    
    gDirFileInfo.fileInfoLen = sumFileNum * sizeof(MOCPS_BASIC_FILEINFO);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "gDirFileInfo.fileInfoLen = %d\n", 
        gDirFileInfo.fileInfoLen);

    gDirFileInfo.curMemSize = gDirFileInfo.fileInfoLen + FM_MEM_INC_SIZE;
    if(gDirFileInfo.curMemSize < gDirFileInfo.fileInfoLen)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "Error! gDirFileInfo.curMemSize=%d, gDirFileInfo.fileInfoLen=%d\n",
            gDirFileInfo.curMemSize, gDirFileInfo.fileInfoLen);
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "gDirFileInfo.fileInfoLen=%d, FM_MEM_INC_SIZE=%d, " \
        "gDirFileInfo.curMemSize=%d\n", gDirFileInfo.fileInfoLen, FM_MEM_INC_SIZE,
        gDirFileInfo.curMemSize);

    //malloc for it
    gDirFileInfo.pFileInfo = NULL;
    gDirFileInfo.pFileInfo = (char *)malloc(gDirFileInfo.curMemSize * sizeof(char));
    if(NULL == gDirFileInfo.pFileInfo)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Malloc failed! errno=%d, desc=[%s], size=%d\n",
            errno, strerror(errno), gDirFileInfo.curMemSize);
        return -2;
    }
    memset(gDirFileInfo.pFileInfo, 0x00, gDirFileInfo.curMemSize);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "malloc ok.\n");

    //set value to it
    int filetype = 0;
    for(filetype = 0; filetype < MOCPS_FILETYPE_MAX; filetype++)
    {
        int startOffset = 0;
        for(i = 0; i < filetype; i++)
        {
            startOffset += gDirFileList.fileNum[i];
        }
        startOffset *= sizeof(MOCPS_BASIC_FILEINFO);
        
        FM_FILEINFO_NODE * pCurNode = gDirFileList.pFileList[filetype];
        for(i= 0; i < gDirFileList.fileNum[filetype]; i++)
        {
            pCurNode = pCurNode->next;
            memcpy(
                gDirFileInfo.pFileInfo + startOffset + (i * sizeof(MOCPS_BASIC_FILEINFO)),
                (char *)&pCurNode->info.basicInfo, sizeof(MOCPS_BASIC_FILEINFO));
        }
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "set all file info ok.\n");
    
    return 0;
}

static void unInitDirfileInfo()
{
    if(gDirFileInfo.pFileInfo != NULL)
    {
        free(gDirFileInfo.pFileInfo);
        gDirFileInfo.pFileInfo = NULL;
    }
    gDirFileInfo.fileInfoLen = 0;
    gDirFileInfo.curMemSize = 0;
    gDirFileInfo.sumFileNum = 0;
    pthread_mutex_destroy(&gDirFileInfo.mutex);
    memset(gDirFileInfo.dirPath, 0x00, DIRPATH_MAXLEN);
}

static void * checkDirThr(void * args)
{
    args = args;
    while(gCheckDirThrRunning)
    {
        //TODO, do it
        sleep(60);
    }
    
    return NULL;
}

static int startCheckDirThr()
{
    gCheckDirThrRunning = 1;
    
    pthread_t thId;
    int ret = pthread_create(&thId, NULL, checkDirThr, NULL);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "Create checkDirthread failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        gCheckDirThrRunning = 0;
        return -1;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "create checkDirThr OK.\n");
    return 0;
}

static void stopCheckDirThr()
{
    gCheckDirThrRunning = 0;
}

/*
    1.check input directory path valid or not;
    2.get all file info in this directory;
    3.save in local memory;
    4.start a thread, to check directory info looply, and refresh our local memory;
*/
int fmInit(const char * pDirpath)
{
    //1.check input pDirpath valid or not;
    if(NULL == pDirpath)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    if(strlen(pDirpath) >= DIRPATH_MAXLEN)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "pDirpath=[%s], its length=%d, max length=%d, too larger!\n",
            pDirpath, strlen(pDirpath), DIRPATH_MAXLEN);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "input directory path is [%s]\n", pDirpath);

    //2.get dir-file list info
    int ret = initDirfileList(pDirpath);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "initDirfileList failed! ret = %d\n", ret);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "initDirfileList ok.\n");

    //3.get dir-file info to local memory
    ret = initDirfileInfo();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "initDirfileInfo failed! ret = %d\n", ret);
        unInitDirfileList();
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "initDirfileInfo ok.\n");

    //4.start thread to check directory changes
    ret = startCheckDirThr();
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "startCheckDirThr failed! ret = %d\n", ret);
        unInitDirfileInfo();
        unInitDirfileList();
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "startCheckDirThr ok.\n");

    gIsInited = 1;

    return 0;
}

/*
    Do uninit, free our resources;
*/
void fmUnInit()
{
    stopCheckDirThr();
    unInitDirfileInfo();
    unInitDirfileList();
    gIsInited = 0;
}

/*
    Get the length of filelist, caller must use this function firstly, then 
    malloc memory in this size, then call @fmGetFilelist to get file info.
*/
int fmGetFileinfoLength(int * pLen)
{
    if(!gIsInited || (NULL == pLen))
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "gIsInited=%d, get length failed!\n", gIsInited);
        return -1;
    }
    
    *pLen = gDirFileInfo.fileInfoLen;
    return 0;
}

/*
    Get file list info from this directory;
    return :
        0- : error, param or other error;
        0 : ok, @pFileinfo is valid, can be used;
        0+ : error, because input @len donot equal with current gDirFileInfo.fileInfoLen, this means,
                after call @fmGetFileinfoLength(), the length being changed by FM itself.
                the ret is the new length;
                caller should malloc a new memory to get fileinfo;
                this will not frequency, so caller being retry at most 3times is our suggestion;
*/
int fmGetFileinfo(char * pFileinfo, const int len)
{
    if(!gIsInited)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Donot init yet, cannot get fileinfo.\n");
        return -1;
    }
    if(NULL == pFileinfo)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -2;
    }

    pthread_mutex_lock(&gDirFileInfo.mutex);
    if(len != gDirFileInfo.fileInfoLen)
    {
        moLoggerWarn(MOCPS_MODULE_LOGGER_NAME, "input length=%d, current fileInfoLen=%d, donot equal!\n",
            len, gDirFileInfo.fileInfoLen);
        pthread_mutex_unlock(&gDirFileInfo.mutex);
        return gDirFileInfo.fileInfoLen;
    }
    memcpy(pFileinfo, gDirFileInfo.pFileInfo, gDirFileInfo.fileInfoLen);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "get fileinfo succeed.\n");
    pthread_mutex_unlock(&gDirFileInfo.mutex);
    return 0;
}

/*
    From @gDirfileInfo, find a node, which has name and type equal with input @pFilename and @type;
    If error, or donot find it, return NULL;
    This function must be locked by caller!
    It will not lock itself!!
*/
static FM_FILEINFO_NODE * findFileNode(const char * pFilename, const MOCPS_FILETYPE type)
{
    if(NULL == pFilename || type >= MOCPS_FILETYPE_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "Input param invalid! pFilename=[%s], type=%d, MOCPS_FILETYPE_MAX=%d\n",
            pFilename, type, MOCPS_FILETYPE_MAX);
        return NULL;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "pFilename=[%s], type=%d(%s)\n",
        pFilename, type, gSubDirName[type]);

    if(gDirFileList.pFileList[type] == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "donot have fileNode in this type!\n");
        return NULL;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "start find this node now.\n");

    int isFind = 0;
    FM_FILEINFO_NODE * pNode = NULL;
    pNode = gDirFileList.pFileList[type]->next;
    while(pNode != NULL)
    {
        if(0 == strcmp(pNode->info.basicInfo.filename, pFilename))
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Find it!\n");
            isFind = 1;
            break;
        }
        pNode = pNode->next;
        continue;
    }

    if(isFind == 0)
    {
        moLoggerInfo(MOCPS_MODULE_LOGGER_NAME, "Donot find this file!\n");
        return NULL;
    }
    return pNode;
}

/*
    Caller want to read this file, and the first time to read it,
    should call this function to open this file;
*/
static int openFile(FM_FILEINFO_NODE * pNode)
{
    if(NULL == pNode || NULL == pNode->info.basicInfo.filename)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    if(pNode->info.fp != NULL || pNode->info.readHdrNum != 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Somebody has read this file yet!\n");
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Start to open file [%s] now.\n",
        pNode->info.basicInfo.filename);

    char filepath[FILEPATH_MAXLEN] = {0x00};
    memset(filepath, 0x00, FILEPATH_MAXLEN);
    sprintf(filepath, "%s/%s/%s", gDirFileList.dirPath, gSubDirName[pNode->info.basicInfo.type],
        pNode->info.basicInfo.filename);
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "filepath = [%s]\n", filepath);

    pNode->info.fp = fopen(filepath, "r+");
    if(pNode->info.fp == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "fopen failed! filepath=[%s], errno=%d, desc=[%s]\n",
            filepath, errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "fopen succeed.\n");

    pNode->info.readHdrNum = 1;
    return 0;
}

static int readFile(FM_FILEINFO_NODE *pNode, const int offset, 
    const int length, char *pBuf)
{
    if(NULL == pNode || NULL == pNode->info.fp || NULL == pBuf)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    rewind(pNode->info.fp);
    int ret = fseek(pNode->info.fp, offset, SEEK_SET);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "fseek failed! ret=%d, errno=%d, desc=[%s]\n",
            ret, errno, strerror(errno));
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "fseek succeed.\n");

    ret = fread(pBuf, 1, length, pNode->info.fp);
    if(ret != length)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "fread failed! ret=%d, length=%d, errno=%d, desc=[%s]\n",
            ret, length, errno, strerror(errno));
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "fread succeed.\n");
    return 0;
}

/*
    Read file;
*/
int fmReadFile(const MOCPS_BASIC_FILEINFO fileInfo, const int offset, const int length, char *pBuf)
{
    if(NULL == pBuf)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    if(offset + length > fileInfo.size)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "offset(%d)+length(%d)=%d, larger than fileSize(%lld)!\n",
            offset, length, offset + length, fileInfo.size);
        return -2;
    }
    if(fileInfo.filename == NULL || fileInfo.type >= MOCPS_FILETYPE_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, 
            "fileInfo.type=%d(maxValue=%d), fileInfo.filename=[%s], invalid argument!\n",
            fileInfo.type, MOCPS_FILETYPE_MAX, fileInfo.filename);
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Input info : " \
        "Filename:[%s], fileSize:%lld, fileType:%d(%s), offset:%d, length:%d\n",
        fileInfo.filename, fileInfo.size, fileInfo.type, gSubDirName[fileInfo.type],
        offset, length);

    pthread_mutex_lock(&gDirFileList.mutex);
    FM_FILEINFO_NODE * pNode = NULL;
    pNode = findFileNode(fileInfo.filename, fileInfo.type);
    if(NULL == pNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "findFileNode failed! fileName=[%s], fileType=%d(%s)\n",
            fileInfo.filename, fileInfo.type, gSubDirName[fileInfo.type]);
        pthread_mutex_unlock(&gDirFileList.mutex);
        return -4;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "findFileNode succeed.\n");

    if(pNode->info.fp == NULL)
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "The first time to read this file, open it now.\n");
        int ret = openFile(pNode);
        if(ret < 0)
        {
            moLoggerError(MOCPS_MODULE_LOGGER_NAME, "openFile failed! ret=%d, filename=[%s], type=%d(%s)\n",
                ret, fileInfo.filename, fileInfo.type, gSubDirName[fileInfo.type]);
            pthread_mutex_unlock(&gDirFileList.mutex);
            return -5;
        }
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "openFile succeed.\n");
    }

    int ret = readFile(pNode, offset, length, pBuf);
    if(ret < 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "readFile failed! ret = %d\n", ret);
        pthread_mutex_unlock(&gDirFileList.mutex);
        return -6;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "readFile succeed.\n");
    pthread_mutex_unlock(&gDirFileList.mutex);
    return 0;
}

int fmStopReadFile(const MOCPS_BASIC_FILEINFO fileInfo)
{
    if(fileInfo.filename == NULL)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input param is NULL!\n");
        return -1;
    }
    if(fileInfo.type >= MOCPS_FILETYPE_MAX)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "Input type=%d, MOCPS_FILETYPE_MAX=%d, invalid argument!\n",
            fileInfo.type, MOCPS_FILETYPE_MAX);
        return -2;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "Stop read file : filename=[%s], filetype=%d(%s)\n",
        fileInfo.filename, fileInfo.type, gSubDirName[fileInfo.type]);

    pthread_mutex_lock(&gDirFileList.mutex);
    FM_FILEINFO_NODE * pNode = NULL;
    pNode = findFileNode(fileInfo.filename, fileInfo.type);
    if(NULL == pNode)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "findFileNode failed! fileName=[%s], fileType=%d(%s)\n",
            fileInfo.filename, fileInfo.type, gSubDirName[fileInfo.type]);
        pthread_mutex_unlock(&gDirFileList.mutex);
        return -3;
    }
    moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "findFileNode succeed.\n");

    int ret = 0;
    if(pNode->info.readHdrNum <= 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "pNode->info.readHdrNum=%d, check for why!!\n",
            pNode->info.readHdrNum);
        ret = -4;
    }
    else if(pNode->info.readHdrNum == 0)
    {
        moLoggerError(MOCPS_MODULE_LOGGER_NAME, "pNode->info.readHdrNum == 0, have been closed yet!\n");
        ret = -5;
    }
    else
    {
        moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "pNode->info.readHdrNum = %d\n",
            pNode->info.readHdrNum);
        pNode->info.readHdrNum--;
        if(pNode->info.readHdrNum == 0)
        {
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "File should be close now!\n");
            fclose(pNode->info.fp);
            pNode->info.fp = NULL;
            moLoggerDebug(MOCPS_MODULE_LOGGER_NAME, "closeFile succeed.\n");
            ret = 0;
        }
    }
    pthread_mutex_unlock(&gDirFileList.mutex);
    return ret;
}

void fmDump()
{
    printf("==================== Dump FileMgr module resources start now ====================\n");
    
    do
    {
        printf("\t FileList info(dirpath=[%s]) : \n", gDirFileList.dirPath);

        pthread_mutex_lock(&gDirFileList.mutex);
        int i = 0;
        for(; i < MOCPS_FILETYPE_MAX; i++)
        {
            int cnt = 0;
            FM_FILEINFO_NODE *pCurNode = gDirFileList.pFileList[i]->next;
            while(pCurNode != NULL)
            {
                cnt++;
                printf("\t\t FileType=[%s], fileName=[%s], fileSie=%lld, typeId=%d, readHdrNum=%d\n", 
                    gSubDirName[i], pCurNode->info.basicInfo.filename, pCurNode->info.basicInfo.size, 
                    pCurNode->info.basicInfo.type, pCurNode->info.readHdrNum);
                pCurNode = pCurNode->next;
            }
            printf("\t\t In summary, %d files of this type.\n", cnt);
        }
        pthread_mutex_unlock(&gDirFileList.mutex);    
    }while(0);
    
    printf("==================== Dump FileMgr module resources stop now ====================\n");
}



