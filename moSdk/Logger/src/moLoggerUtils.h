#ifndef __MO_LOGGER_UTILS_H__
#define __MO_LOGGER_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* This function to record the moLogger info;
 * When I need debug this library, will open this function;
 * */
#if 0
#define moLoggerOwnInfo(format, ...) printf("[%s, %s, %d] : "format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define moLoggerOwnInfo(format, ...) 
#endif

/* The config file name must be moLoggerConf.ini */
#define MO_LOGGER_CONF_FILENAME	"moLoggerConf.ini"

//The module name max length in bytes
#define MO_LOGGER_MODULENAME_MAXLEN		255

//The max length of a log
#define MO_LOGGER_MAX_LOG_LEN       1024

//The max length of a directory path
#define MO_LOGGER_MAX_DIRPATH_LEN   256

typedef enum
{
	MO_LOGGER_FALSE = 0,
	MO_LOGGER_TRUE
}MO_LOGGER_BOOL;

/* Check the @level is valid or not;
 * */
MO_LOGGER_BOOL isValidLevel(const int level);

/* Get curent time, and set it to @t;
 * time format is : 2016-08-31:19:46:35
 * The max length will be set to 20;
 *  */
#define MO_LOGGER_TIME_LEN	20
void getCurTime(char *t);

/* Check @str is valid level or not; */
MO_LOGGER_BOOL isValidLevelValue(const char *str);

/*
    check our config file exist in @dir or not;
*/
MO_LOGGER_BOOL isConfFileExist(const char *dir);

#ifdef __cplusplus
}
#endif

#endif
