#include <stdio.h>
#include <pthread.h>
#include <time.h>

#include "moLogger.h"

/*
    1.configDir donot exist;
    2.configDir is a file;
    3.configDir exist, but donot exist a config file in it;
    4.configDir exist, and configFile exist, but this configFile is a directory;
    5.configFile is not a valid ini file;
    6.configFile is an ini file, but donot has attribute "level";
    7.right format configFile:
        7.1.moLogger a log with invalid modulename;
        7.2.moLogger a log with valid modulename, with several levels;
        7.3.moLogger a log to a file;
    8.Multi-threads:
        8.1.output to stdout;
        8.2.output to the same file;
    9.Multi-processes:
        9.1.output to stdout;
        9.2.output to the same file;
*/

#define MODULE_EXIST_0  "moduleExist0"
#define MODULE_EXIST_1  "moduleExist1"
#define MODULE_NOT_EXIST_0  "moduleNotExist0"

static void tst_basicFunc(void)
{
    char fileDir[] = "./confFileDir";
    int ret = moLoggerInit(fileDir);
    if(0 != ret)
    {
        printf("wjl_test : moLoggerInit failed! ret = %d, fileDir = [%s]\n", 
            ret, fileDir);    
    }
    else
    {
        printf("wjl_test : moLoggerInit succeed.\n");

        printf("\nStart a test......\n");
        logger(MODULE_EXIST_0, moLoggerLevelDebug, "A log just for test, moduleName=[%s], Level=%d\n", 
            MODULE_EXIST_0, moLoggerLevelDebug);
        printf("end a test......\n");
        
        printf("\nStart a test......\n");
        logger(MODULE_EXIST_0, moLoggerLevelInfo, "A log just for test, moduleName=[%s], Level=%d\n", 
            MODULE_EXIST_0, moLoggerLevelInfo);
        printf("end a test......\n");
        
        printf("\nStart a test......\n");
        logger(MODULE_EXIST_0, moLoggerLevelWarn, "A log just for test, moduleName=[%s], Level=%d\n", 
            MODULE_EXIST_0, moLoggerLevelWarn);
        printf("end a test......\n");
        
        printf("\nStart a test......\n");
        logger(MODULE_EXIST_0, moLoggerLevelError, "A log just for test, moduleName=[%s], Level=%d\n", 
            MODULE_EXIST_0, moLoggerLevelError);
        printf("end a test......\n");
        
        printf("\nStart a test......\n");
        logger(MODULE_EXIST_0, moLoggerLevelFatal, "A log just for test, moduleName=[%s], Level=%d\n", 
            MODULE_EXIST_0, moLoggerLevelFatal);
        printf("end a test......\n");

        moLoggerUnInit();
    }
    
}

static void threadFunc0(void * args)
{
    while(1)
    {
        logger(MODULE_EXIST_0, moLoggerLevelError, "A log from thread 0. moduleName = [%s], level = %d\n", 
            MODULE_EXIST_0, moLoggerLevelError);
        int interval = rand() % 1000 + 1000;
        usleep(interval);
    }
}

static void threadFunc1(void * args)
{
    while(1)
    {
        logger(MODULE_EXIST_0, moLoggerLevelFatal, "A log from thread 1. moduleName = [%s], level = %d\n", 
            MODULE_EXIST_0, moLoggerLevelFatal);
        int interval = rand() % 1000 + 1000;
        usleep(interval);
    }
}

static void tst_multiThreads(void)
{
    char fileDir[] = "./confFileDir";
    int ret = moLoggerInit(fileDir);
    if(0 != ret)
    {
        printf("wjl_test : moLoggerInit failed! ret = %d, fileDir = [%s]\n", 
            ret, fileDir);    
    }
    else
    {
        pthread_t thId0;
        pthread_create(&thId0, NULL, threadFunc0, NULL);
        pthread_t thId1;
        pthread_create(&thId1, NULL, threadFunc1, NULL);
    
        while(1)
        {
            sleep(3);
            break;
        }

        moLoggerUnInit();
    }
}

int main(int argc, char **argv)
{
    srand((unsigned int )time(NULL));
    
//	printf("Hello world.\n");

//    tst_basicFunc();

    tst_multiThreads();


	return 0;
}
