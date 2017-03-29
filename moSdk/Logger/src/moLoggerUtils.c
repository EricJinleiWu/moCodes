#include <time.h>
#include <dirent.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>

#include "moLoggerUtils.h"
#include "moLogger.h"

/*
    All levels being support;
*/
#define MO_LEVEL_NUM    5   //5 levels being supported
static const char gStrValue[MO_LEVEL_NUM][6] = {
    "debug", "info", "warn", "error", "fatal"
    };


/* 	moLoggerLevelDebug = 0,
	moLoggerLevelInfo = 1,
	moLoggerLevelWarn = 2,
	moLoggerLevelError = 3,
	moLoggerLevelFatal = 4,
 * */
MO_LOGGER_BOOL isValidLevel(const int level)
{
	return (level >= moLoggerLevelDebug && level <= moLoggerLevelFatal) ? MO_LOGGER_TRUE : MO_LOGGER_FALSE;
}

/* Get current time;
 * Then set current time to t in format like : 2016-08-31:19:46:35
 * */
void getCurTime(char *t)
{
	time_t curTime = time(NULL);

	struct tm *pLocalTime = NULL;
	pLocalTime = localtime(&curTime);

	sprintf(t, "%04d-%02d-%02d/%02d:%02d:%02d", pLocalTime->tm_year + 1900,
			pLocalTime->tm_mon + 1, pLocalTime->tm_mday, pLocalTime->tm_hour, pLocalTime->tm_min,
			pLocalTime->tm_sec);
}

/*
    @pLevel is a string of level, must be in ["debug", "info", "warn", "error", "fatal"]
    valid value has been defind in @gStrValue;
    level id will be returned if @pLevel is valid, or return 0-;
*/
int getLevelFromValue(const char *pLevel)
{
    if(NULL == pLevel)
        return -1;

    int ret = -2;
    int i = 0;
    for(; i < MO_LEVEL_NUM; i++)
    {
        if(0 == strcmp(pLevel, gStrValue[i]))
        {
            ret = i;
            break;
        }
    }

    return ret;
}

MO_LOGGER_BOOL isConfFileExist(const char *dir)
{
    if(NULL == dir)
    {
        return MO_LOGGER_FALSE;
    }

    DIR *pDir = opendir(dir);
    if(NULL == pDir)
    {
        moLoggerOwnInfo("Opendir [%s] failed! errno = %d, desc = [%s]\n", dir, errno, strerror(errno));
        return MO_LOGGER_FALSE;
    }

    struct dirent *pDirent = NULL;
    while((pDirent = readdir(pDir)) != NULL)
    {
        if(0 == strcmp(MO_LOGGER_CONF_FILENAME, pDirent->d_name))
        {
            closedir(pDir);
            pDir = NULL;
            return MO_LOGGER_TRUE;
        }
    }

    moLoggerOwnInfo("Donot find config file [%s] in dir [%s]!\n", MO_LOGGER_CONF_FILENAME, dir);            
    closedir(pDir);
    pDir = NULL;
    return MO_LOGGER_FALSE;
}



