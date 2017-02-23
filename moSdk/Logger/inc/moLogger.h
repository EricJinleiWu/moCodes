/* Wujl
 * Create at 2016-11-28
 * V1.0.0, support print logs to stdout and local file;
 * */

#ifndef __MO_LOGGER_H__
#define __MO_LOGGER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	moLoggerLevelDebug = 0,
	moLoggerLevelInfo = 1,
	moLoggerLevelWarn = 2,
	moLoggerLevelError = 3,
	moLoggerLevelFatal = 4,
}MO_LOGGER_LEVEL;

/* Do init for moLogger;
 * parse the moLoggerConf.ini file, get all modules infomations, and set them to memory;
 * @fileDir is the directory of moLoggerConf.ini;
 * return :
 * 		0 : succeed;
 * 		0-: failed;
 * */
int moLoggerInit(const char *fileDir);

/* Function to log;
 * Append on moduleName, moLogger will find the config file, get the level and
 * output destination info, then log out;
 * */
void logger(const char *moduleName, const MO_LOGGER_LEVEL level, const char *fmt, ...) \
		__attribute__((format(printf, 3, 4)));

/* API to caller;
 * */
#define moLogger(moduleName, level, fmt, args...) \
	do \
	{logger(moduleName, level, fmt, ##args);} \
	while(0)

/* Do uninit for moLogger;
 * Free the memory being malloced when init;
 * */
int moLoggerUnInit(void);

















#if 0
va_start(args, fmt);
doDebug(sysId, modId, level, NULL, fmt, args);
va_end(args);

static void doDebug(int sysId, int modId, int level, char *txt, const char *fmt, va_list args)
{
    char msgbuf[KIT_DEBUG_MSG_BUF_SIZE] = {0};
    char fmtbuf[KIT_DEBUG_MSG_BUF_SIZE] = {0};
    int output = STDERR_FILENO;
 
         
    if (KIT_DEBUG_LEVEL_FORCE != level)
    {
        if (level > debugCtrl[sysId][modId].level ||
            KIT_DEBUG_LEVEL_QUIET == debugCtrl[sysId][modId].level ||
            KIT_DEBUG_LEVEL_QUIET == level)
        {
            return;
        }
    }
    
    output = debugCtrl[sysId][modId].output;

    if (NULL == txt)
    {
        vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
    }
    else
    {
        snprintf(fmtbuf, sizeof(fmtbuf), "%s %s", txt, fmt);
        vsnprintf(msgbuf, sizeof(msgbuf), fmtbuf, args);
    }

    write(output, msgbuf, strlen(msgbuf));

    return;
}
#endif





#ifdef __cplusplus
}
#endif

#endif
