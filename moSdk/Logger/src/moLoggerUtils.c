#include <time.h>
#include <dirent.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>

#include "moLoggerUtils.h"
#include "moLogger.h"

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

MO_LOGGER_BOOL isValidLevelValue(const char *str)
{
    if(NULL == str)
        return MO_LOGGER_FALSE;

    int i = moLoggerLevelDebug;
    for(; i <= moLoggerLevelFatal; i++)
    {
        char temp[4] = {0x00};
        sprintf(temp, "%d", i);
        if(0 == strcmp(str, temp))
        {
            //valid one
            return MO_LOGGER_TRUE;
        }
    }

    return MO_LOGGER_FALSE;
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



