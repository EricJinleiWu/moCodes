#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "moUtils.h"

/*
    check @path point to a file or a directory;
    
    If a file(regular file or link file), return 1;
    else, return 0;

    If @path is NULL, return 0, too.
*/
static int isFile(const char *path)
{
    if(NULL == path)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return 0;
    }

    struct stat curStat;
    memset(&curStat, 0x00, sizeof(struct stat));
    int ret = lstat(path, &curStat);
    if(ret != 0)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
            "stat failed! path is [%s], errno = %d, desc = [%s]\n", 
            path, errno, strerror(errno));
        return 0;
    }

    if(S_ISREG(curStat.st_mode))
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "path [%s] is a regular file.\n", path);
        return 1;
    }
    if(S_ISLNK(curStat.st_mode))
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, "path [%s] is a linked file.\n", path);
        return 1;
    }
    else
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "path [%s] is not a linked file or regular file.\n", path);
        return 0;
    }
}

static int isDir(const char *path)
{
    if(NULL == path)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return 0;
    }

    struct stat curStat;
    memset(&curStat, 0x00, sizeof(struct stat));
    int ret = stat(path, &curStat);
    if(ret != 0)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
            "stat failed! path is [%s], errno = %d, desc = [%s]\n", path, errno, strerror(errno));
        return 0;
    }

    if(S_ISDIR(curStat.st_mode))
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, "path [%s] is a directory.\n", path);
        return 1;
    }
    else
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "path [%s] is not a directory.\n", path);
        return 0;
    }

}

/*
    get size of @pFilepath;
    size in bytes;
*/
int moUtils_File_getSize(const char *pFilepath)
{
    if(NULL == pFilepath)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOUTILS_FILE_ERR_INPUTPARAMNULL;
    }

    if(isFile(pFilepath))
    {
        struct stat curStat;
        memset(&curStat, 0x00, sizeof(struct stat));
        int ret = lstat(pFilepath, &curStat);
        if(ret != 0)
        {
            moLoggerError(MOUTILS_LOGGER_MODULE_NAME, 
                "lstat failed! pFilepath = [%s], errno = %d, desc = [%s]\n",
                pFilepath, errno, strerror(errno));
            return MOUTILS_FILE_ERR_STATFAILED;
        }
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, "lstat succeed, size = %ld\n", curStat.st_size);
        return curStat.st_size;
    }
    else
    {
        //Not a file, cannot get size, return error
        return MOUTILS_FILE_ERR_NOTFILE;
    }   
}

/*
    get abstract filepath;
    If get failed, NULL return;
*/
char * moUtils_File_getAbsFilePath(const char *pFilepath)
{
    if(NULL == pFilepath)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return NULL;
    }

    //Get dirpath and name from filepath
    MOUTILS_FILE_DIR_FILENAME info;
    memset(&info, 0x00, sizeof(MOUTILS_FILE_DIR_FILENAME));
    int ret = moUtils_File_getDirAndFilename(pFilepath, &info);
    if(ret != 0)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
            "moUtils_File_getDirAndName failed! ret = %d, filepath = [%s]\n",
            ret, pFilepath);
        return NULL;
    }
    moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
        "filepath = [%s], dirpath = [%s], filename = [%s]\n",
        pFilepath, info.pDirpath, info.pFilename);

    //convert dir path to abstract directory path 
    char *pAbsDirpath = moUtils_File_getAbsDirPath(info.pDirpath);
    if(NULL == pAbsDirpath)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "moUtils_File_getAbsDirPath failed!\n");
        free(info.pDirpath);
        info.pDirpath = NULL;
        free(info.pFilename);
        info.pFilename = NULL;
        return NULL;
    }
    moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
        "pDirpath = [%s], pAbsDirpath = [%s]\n", info.pDirpath, pAbsDirpath);

    //set abstract filepath
    int len = strlen(pAbsDirpath) + 1 + strlen(info.pFilename) + 1; //We will add a '/' between dir and name, so size should add 1
    char *pAbsFilepath = NULL;
    pAbsFilepath = (char *)malloc(sizeof(char) * len);
    if(NULL == pAbsFilepath)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, 
            "malloc for pAbsFilepath failed! len = %d, errno = %d, desc = [%s]\n",
            len, errno, strerror(errno));
        free(info.pDirpath);
        info.pDirpath = NULL;
        free(info.pFilename);
        info.pFilename = NULL;
        free(pAbsDirpath);
        pAbsDirpath = NULL;
        return NULL;
    }
    memset(pAbsFilepath, 0x00, len);
    sprintf(pAbsFilepath, "%s/%s", pAbsDirpath, info.pFilename);
    moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
        "pAbsDirpath = [%s], info.pFilename = [%s], pAbsFilepath = [%s]\n",
        pAbsDirpath, info.pFilename, pAbsFilepath);

    free(info.pDirpath);
    info.pDirpath = NULL;
    free(info.pFilename);
    info.pFilename = NULL;
    free(pAbsDirpath);
    pAbsDirpath = NULL;

    return pAbsFilepath;
}

/*
    get abstract dir path;
    If get failed, return NULL;
*/
char * moUtils_File_getAbsDirPath(const char * pDirpath)
{
    if(NULL == pDirpath)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return NULL;
    }

    if(!(isDir(pDirpath)))
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input path [%s] is not a directory.\n", pDirpath);
        return NULL;
    }

    //use cmd : cd dir; pwd; to get abstract dir path;
    char *pCmd = NULL;
    int cmdLen = strlen(pDirpath) + strlen("cd ;pwd") + 1;
    pCmd = (char *)malloc(sizeof(char) * cmdLen);
    if(NULL == pCmd)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
            "malloc for cmd failed! cmdLen = %d, errno = %d, desc = [%s]\n",
            cmdLen, errno, strerror(errno));
        return NULL;
    }
    memset(pCmd, 0x00, cmdLen);
    sprintf(pCmd, "cd %s;pwd", pDirpath);
    moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
        "pDirpath = [%s], cmdLen = %d, cmd = [%s]\n", pDirpath, cmdLen, pCmd);

    FILE * fp = NULL;
    fp = popen(pCmd, "r");
    if(fp == NULL)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, 
            "popen failed! cmd is [%s], errno = %d, desc = [%s]\n", 
            pCmd, errno, strerror(errno));
        free(pCmd);
        pCmd = NULL;
        return NULL;
    }

    int unitSize = 256;
    int i = 0;
    for(i = 0; ; i++)
    {
        rewind(fp);
        int blkSize = unitSize * (i + 1);
        char * pBlk = NULL;
        pBlk = (char *)malloc(sizeof(char) * blkSize);
        if(NULL == pBlk)
        {
            moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
                "malloc for pBlk failed! length = %d, errno = %d, desc = [%s]\n",
                blkSize, errno, strerror(errno));
            free(pCmd);
            pCmd = NULL;
            return NULL;            
        }
        memset(pBlk, 0x00, blkSize);
        int readSize = 0;
        if((readSize = fread(pBlk, 1, blkSize, fp)) == blkSize)
        {
            moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME,  
                "i = %d, readSize = %d, blkSize = %d\n", i, readSize, blkSize);
            free(pBlk);
            pBlk = NULL;
            continue;
        }
        else
        {
            //Use \0 to replace CR
            pBlk[readSize - 1] = 0x00;
            moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
                "i = %d, readSize = %d, blkSize = %d, pBlk == [%s]\n", 
                i, readSize, blkSize, pBlk);
            char * pAbsDirpath = NULL;
            pAbsDirpath = (char *)malloc(sizeof(char) * (strlen(pBlk) + 1));
            if(NULL == pAbsDirpath)
            {
                moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
                    "malloc for pAbsDirpath failed! length = %d, errno = %d, desc = [%s]\n",
                    strlen(pBlk) + 1, errno, strerror(errno));
                //In this case, we will return pBlk directly, this will waste some memory, but will not interrupt user calling.
                moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
                    "pAbsDirpath is [%s]\n", pBlk);
                return pBlk;
            }
            memset(pAbsDirpath, 0x00, strlen(pBlk) + 1);
            strcpy(pAbsDirpath, pBlk);
            moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, "pAbsDirpath is [%s]\n", pAbsDirpath);

            free(pBlk);
            pBlk = NULL;

            free(pCmd);
            pCmd = NULL;

            pclose(fp);
            fp = NULL;
            
            return pAbsDirpath;
        }
    }
}

/*
    Input @pFilepath is a filepath, get dirpath and filename from it, and set to @pDirName;
*/
int moUtils_File_getDirAndFilename(const char *pFilepath, MOUTILS_FILE_DIR_FILENAME * pInfo)
{
    if(NULL == pFilepath || NULL == pInfo)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        return MOUTILS_FILE_ERR_INPUTPARAMNULL;
    }

    if(NULL != pInfo->pDirpath && NULL != pInfo->pFilename)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, 
            "pInfo->pDirpath and pInfo->pFilename must be NULL, or memory leak will occur!\n");
        return MOUTILS_FILE_ERR_INPUTPARAMNULL;
    }

    //Get the last symbol '/' position, this is the separator between dir and filename;
    char *pLastSymbPos = strrchr(pFilepath, '/');
    if(NULL == pLastSymbPos)
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "pFilepath = [%s], donot find \'/\' in it. this means its directory is current directory.\n",
            pFilepath);
        
        int dirLen = 3; //its value is "./"
        pInfo->pDirpath = (char *)malloc(sizeof(char) * dirLen);
        if(NULL == pInfo->pDirpath)
        {
            moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
                "malloc for pDirpath failed! errno = %d, desc = [%s]\n",
                errno, strerror(errno));
            return MOUTILS_FILE_ERR_MALLOCFAILED;
        }
        memset(pInfo->pDirpath, 0x00, dirLen);
        strcpy(pInfo->pDirpath, "./");

        int filenameLen = strlen(pFilepath) + 1;
        pInfo->pFilename = (char *)malloc(sizeof(char) * filenameLen);
        if(pInfo->pFilename == NULL)
        {
            moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
                "malloc for pInfo->pFilename failed! len = %d, errno = %d, desc = [%s]\n",
                filenameLen, errno, strerror(errno));
            free(pInfo->pDirpath);
            pInfo->pDirpath = NULL;
            return MOUTILS_FILE_ERR_MALLOCFAILED;
        }
        memset(pInfo->pFilename, 0x00, filenameLen);
        strcpy(pInfo->pFilename, pFilepath);

        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "pFilepath = [%s], pInfo->pDirpath = [%s], pInfo->pFilename = [%s]\n",
            pFilepath, pInfo->pDirpath, pInfo->pFilename);
        return MOUTILS_FILE_ERR_OK;
    }
    else
    {
        int dirLen = pLastSymbPos + 1 - pFilepath + 1;
        pInfo->pDirpath = (char *)malloc(sizeof(char) * dirLen);
        if(NULL == pInfo->pDirpath)
        {
            moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
                "malloc for pDirpath failed! dirLen = %d, errno = %d, desc = [%s]\n",
                dirLen, errno, strerror(errno));
            return MOUTILS_FILE_ERR_MALLOCFAILED;
        }
        memset(pInfo->pDirpath, 0x00, dirLen);
        strncpy(pInfo->pDirpath, pFilepath, dirLen - 1);
        pInfo->pDirpath[dirLen - 1] = 0x00;
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "pFilepath = [%s], pInfo->pDirpath = [%s]\n", pFilepath, pInfo->pDirpath);

        int filenameLen = strlen(pFilepath) - (pLastSymbPos - pFilepath);
        pInfo->pFilename = (char *)malloc(sizeof(char) * filenameLen);
        if(pInfo->pFilename == NULL)
        {
            moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
                "malloc for pInfo->pFilename failed! len = %d, errno = %d, desc = [%s]\n",
                filenameLen, errno, strerror(errno));
            free(pInfo->pDirpath);
            pInfo->pDirpath = NULL;
            return MOUTILS_FILE_ERR_MALLOCFAILED;
        }
        memset(pInfo->pFilename, 0x00, filenameLen);
        strncpy(pInfo->pFilename, pLastSymbPos + 1, filenameLen - 1);
        pInfo->pFilename[filenameLen - 1] = 0x00;
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "pFilepath = [%s], pInfo->pFilename = [%s]\n", pFilepath, pInfo->pFilename);        

        return MOUTILS_FILE_ERR_OK;
    }
}

int moUtils_File_getFilepathSameState(const char *pSrcFilepath, const char *pDstFilepath, 
    MOUTILS_FILE_ABSPATH_STATE * pState)
{
    if(NULL == pSrcFilepath || NULL == pDstFilepath || NULL == pState)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME, "Input param is NULL.\n");
        *pState = MOUTILS_FILE_ABSPATH_STATE_ERR;
        return MOUTILS_FILE_ERR_INPUTPARAMNULL;
    }

    //If they have same value, of course they are same path.
    if(0 == strcmp(pSrcFilepath, pDstFilepath))
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "pSrcFilepath exactly same with pDstFilepath! they are the same path.\n");
        *pState = MOUTILS_FILE_ABSPATH_STATE_SAME;
        return MOUTILS_FILE_ERR_OK;
    }

    //Get src abstrace filepath
    char *pSrcAbsFilepath = NULL;
    pSrcAbsFilepath = moUtils_File_getAbsFilePath(pSrcFilepath);
    if(NULL == pSrcAbsFilepath)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
            "moUtils_File_getAbsFilePath failed! pSrcFilepath = [%s]\n",
            pSrcFilepath);
        *pState = MOUTILS_FILE_ABSPATH_STATE_ERR;
        return MOUTILS_FILE_ERR_MALLOCFAILED;
    }

    //Get dst abstract filepath
    char *pDstAbsFilepath = NULL;
    pDstAbsFilepath = moUtils_File_getAbsFilePath(pDstFilepath);
    if(NULL == pDstAbsFilepath)
    {
        moLoggerError(MOUTILS_LOGGER_MODULE_NAME,
            "moUtils_File_getAbsFilePath failed! pAbsFilepath = [%s]\n",
            pDstFilepath);
        *pState = MOUTILS_FILE_ABSPATH_STATE_ERR;
        return MOUTILS_FILE_ERR_MALLOCFAILED;
    }

    //Compare the abstract filepath
    if(0 == strcmp(pSrcAbsFilepath, pDstAbsFilepath))
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "pSrcFilepath = [%s], pSrcAbstractFilepath=[%s], pDstFilepath = [%s], pDstAbstractFilepath=[%s]," \
            "The abstract filepath is same.\n", pSrcFilepath, pSrcAbsFilepath, pDstFilepath, pDstAbsFilepath);

        free(pSrcAbsFilepath);
        pSrcAbsFilepath = NULL;
        free(pDstAbsFilepath);
        pDstAbsFilepath = NULL;
        
        *pState = MOUTILS_FILE_ABSPATH_STATE_SAME;
        return MOUTILS_FILE_ERR_OK;
    }
    else
    {
        moLoggerDebug(MOUTILS_LOGGER_MODULE_NAME, 
            "pSrcFilepath = [%s], pSrcAbstractFilepath=[%s], pDstFilepath = [%s], pDstAbstractFilepath=[%s]," \
            "The abstract filepath is different.\n", pSrcFilepath, pSrcAbsFilepath, pDstFilepath, pDstAbsFilepath);

        free(pSrcAbsFilepath);
        pSrcAbsFilepath = NULL;
        free(pDstAbsFilepath);
        pDstAbsFilepath = NULL;

        *pState = MOUTILS_FILE_ABSPATH_STATE_DIFF;
        return MOUTILS_FILE_ERR_OK;
    }
}



