#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "moLogger.h"
#include "moLoggerUtils.h"
#include "moIniParser.h"

#define ATTR_LEVEL      "level"
#define ATTR_LOCALFILE  "localfile"

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;
static MO_LOGGER_BOOL gIsInited = MO_LOGGER_FALSE;
static SECTION_INFO_NODE *gpSecHeadNode = NULL;
static char gFileDir[MO_LOGGER_MAX_DIRPATH_LEN] = {0x00};
static unsigned int gInitCnt = 0;

static const char strLevel[5][8] = {
    "debug", "info", "warn", "error", "fatal"
    };


static void setInitFlag(void)
{
	gIsInited = MO_LOGGER_TRUE;
}

static void clearInitFlag(void)
{
	gIsInited = MO_LOGGER_FALSE;
}

static MO_LOGGER_BOOL isInited(void)
{
	return gIsInited;
}

/* Do init for moLogger;
 * 1.find log file from @fileDir;
 * 2.parse log config file;
 * 3.record all settings in log config file, and open local file if neccessary;
 *
 * return :
 * 		0 : init OK;
 * 		0-: init failed;
 * */
int moLoggerInit(const char *fileDir)
{
	if(NULL == fileDir)
	{
		moLoggerOwnInfo("Input param is NULL!\n");
		return -1;
	}

    pthread_mutex_lock(&gLock);

	if(isInited())
	{
        if(0 == strcmp(fileDir, gFileDir))
        {
            //This file has been init by other processes, will not init again
            gInitCnt++;
            pthread_mutex_unlock(&gLock);
            return 0;
        }
        else
        {
            moLoggerOwnInfo("moLogger has been inited yet! should not init again!\n");
            pthread_mutex_unlock(&gLock);
            return -2;
        }
	}

	if(!isConfFileExist(fileDir))
	{
        moLoggerOwnInfo("In dir [%s], donot find moLogger config file : [%s]\n",
            fileDir, MO_LOGGER_CONF_FILENAME);
        pthread_mutex_unlock(&gLock);
		return -3;
	}

    int filepathLen = strlen(fileDir) + strlen(MO_LOGGER_CONF_FILENAME) + 2;
    char *filepath = NULL;
    filepath = (char *)malloc(sizeof(char) * filepathLen);
    if(NULL == filepath)
    {
        moLoggerOwnInfo("Malloc for ini filepath failed!\n");
        pthread_mutex_unlock(&gLock);
        return -4;
    }
    sprintf(filepath, "%s/%s", fileDir, MO_LOGGER_CONF_FILENAME);
    filepath[filepathLen - 1] = 0x00;

    /* Parse this config file */
    gpSecHeadNode = moIniParser_Init(filepath);
    if(NULL == gpSecHeadNode)
	{
		moLoggerOwnInfo("moIniParser_Init failed! file directory is [%s], filepath is [%s].\n",
				fileDir, filepath);
        free(filepath);
        filepath = NULL;
        pthread_mutex_unlock(&gLock);
		return -5;
	}

    //Free the memory we malloced 
    free(filepath);
    filepath = NULL;
    //Save the dir path
    strcpy(gFileDir, fileDir);

    //do count
    gInitCnt++;
    
    //Has init, set flag
	setInitFlag();
    
    pthread_mutex_unlock(&gLock);

	return 0;
}

/* Output log info;
 * 1.get module info with @moduleName;
 * 2.if level larger than configLevel, step3; else, quit;
 * 3.generate info;
 * 4.if configLocalFile valid, output to file; else, output to stdout;
 * */
void logger(const char *moduleName, const MO_LOGGER_LEVEL level, const char *fmt, ...)
{
	if(NULL == moduleName)
	{
		moLoggerOwnInfo("Input param is NULL!\n");
		return ;
	}

	if(!isValidLevel(level))
	{
		moLoggerOwnInfo("Input level = %d, donot a valid value!\n", level);
		return ;
	}

    if(!isInited())
    {
        moLoggerOwnInfo("Donot init yet, cannot logger anything.\n");
        return ;
    }

    /* Get the level firstly */
    char value[ATTR_VALUE_MAX_LEN] = {0x00};
    memset(value, 0x00, ATTR_VALUE_MAX_LEN);
    int ret = moIniParser_GetAttrValue(moduleName, ATTR_LEVEL, value, gpSecHeadNode);
    if(0 != ret)
    {
        moLoggerOwnInfo("moIniParser_GetAttrValue failed! ret = %d, moduleName = [%s], ATTR_LEVEL = [%s]\n",
            ret, moduleName, ATTR_LEVEL);
        return ;
    }
    if(!isValidLevelValue(value))
    {
        moLoggerOwnInfo("Level = [%s], donot valid!\n", value);
        return ;
    }
    unsigned int configLevel = atoi(value);

    if(level >= configLevel)    //Need output this log
    {        
        /* Check logout to file or not */
        memset(value, 0x00, ATTR_VALUE_MAX_LEN);
        MO_LOGGER_BOOL isWriteLog2File = MO_LOGGER_FALSE;
        ret = moIniParser_GetAttrValue(moduleName, ATTR_LOCALFILE, value, gpSecHeadNode);
        if(0 == ret)
        {
            //Need output to file, filepath being defined in @value
            isWriteLog2File = MO_LOGGER_TRUE;
        }
        char filepath[ATTR_VALUE_MAX_LEN] = {0x00};
        strcpy(filepath, value);

        /* generate the log info in my format */
        char fmtbuf[MO_LOGGER_MAX_LOG_LEN] = {0x00};
        memset(fmtbuf, 0x00, MO_LOGGER_MAX_LOG_LEN);
        char curLoginfo[MO_LOGGER_MAX_LOG_LEN] = {0x00};
        memset(curLoginfo, 0x00, MO_LOGGER_MAX_LOG_LEN);
        char curTimeStr[MO_LOGGER_TIME_LEN] = {0x00};
        memset(curTimeStr, 0x00, MO_LOGGER_TIME_LEN);
        getCurTime(curTimeStr);

        va_list args;
        va_start(args, fmt);

        snprintf(fmtbuf, sizeof(fmtbuf), "%s[%s] %s", curTimeStr, strLevel[level], fmt);
        vsnprintf(curLoginfo, sizeof(curLoginfo), fmtbuf, args);
        
        va_end(args);
        
        if(isWriteLog2File)
        {
            FILE *fp = NULL;
            fp = fopen(filepath, "a");
            if(NULL == fp)
            {
                moLoggerOwnInfo("Open file [%s] failed! errno = %d, desc = [%s]\n", 
                    filepath, errno, strerror(errno));
            }
            else
            {
                fwrite(curLoginfo, 1, strlen(curLoginfo), fp);
                fflush(fp);
                fclose(fp);
                fp = NULL;
            }
        }
        else
        {
            write(fileno(stdout), curLoginfo, sizeof(curLoginfo));
        }
    }
}

/*
    Free memory;
    reset all global info;
*/
int moLoggerUnInit(void)
{
    pthread_mutex_lock(&gLock);
    if(isInited())
    {
        gInitCnt--;
        //All user has been uninit yet, will release our resource
        if(0 == gInitCnt)
        {
            /* Release all info being parsed from config file. */
            moIniParser_UnInit(gpSecHeadNode);
            gpSecHeadNode = NULL;

            memset(gFileDir, 0x00, MO_LOGGER_MAX_DIRPATH_LEN);
                
            /* Reset the flag; */
            clearInitFlag();
        }
    }
    pthread_mutex_unlock(&gLock);
    
    return 0;
}

