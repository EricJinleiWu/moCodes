#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "moLogger.h"
#include "moUtils.h"
#include "moCrypt.h"

#include "moCloudUtilsTypes.h"
#include "moCloudUtilsCheck.h"
#include "moCloudUtilsCrypt.h"
#include "moCloudClientLib.h"

static void dumpAllFileInfo(MOCLOUD_BASIC_FILEINFO_NODE * pAllFileInfo)
{
    printf("dumpAllFileInfo now.\n");
    MOCLOUD_BASIC_FILEINFO_NODE * pCurNode = pAllFileInfo;
    while(pCurNode != NULL)
    {
        printf("Current file : name=[%s], size=%d, type=%d, state=%d\n",
            pCurNode->info.key.filename, pCurNode->info.filesize, pCurNode->info.key.filetype, pCurNode->info.state);
        
        pCurNode = pCurNode->next;
    }
}

static int tst_All(void)
{
    int ret = moCloudClient_init("./conf.ini");
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudClient_init failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_init SUCCEED.\n");

    char username[MOCLOUD_USERNAME_MAXLEN] = "testtest";
    char passwd[MOCLOUD_PASSWD_MAXLEN] = "123456";
#if 1
    ret = moCloudClient_signUp(username, passwd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_signUp failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_signUp SUCCEED.\n");
#endif

    ret = moCloudClient_logIn(username, passwd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_logIn failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_logIn SUCCEED.\n");

    //to check heartbeat ok or not;
    sleep(3);

    MOCLOUD_BASIC_FILEINFO_NODE * pAllFileInfo = NULL;
    ret = moCloudClient_getFilelist(&pAllFileInfo, MOCLOUD_FILETYPE_ALL);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_getAllFileInfo  failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_getAllFileInfo SUCCEED.\n");
    dumpAllFileInfo(pAllFileInfo);

    moCloudClient_freeFilelist(pAllFileInfo);


    sleep(3);

    ret = moCloudClient_logOut(username, passwd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_logOut failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_logOut SUCCEED.\n");
    
    moCloudClient_unInit();
    return 0;
}

static int tst_StartDwld()
{
    int ret = moCloudClient_init("./conf.ini");
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "moCloudClient_init failed! ret=%d\n", ret);
        return -1;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_init SUCCEED.\n");

    char username[MOCLOUD_USERNAME_MAXLEN] = "testtest";
    char passwd[MOCLOUD_PASSWD_MAXLEN] = "123456";
#if 0
    ret = moCloudClient_signUp(username, passwd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_signUp failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_signUp SUCCEED.\n");
#endif

    ret = moCloudClient_logIn(username, passwd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_logIn failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_logIn SUCCEED.\n");

    MOCLOUD_FILEINFO_KEY key;
    key.filetype = 1;   //3, 5, 7
    strcpy(key.filename , "2.vedio");   //3.vedio
    size_t fileSize = 1136; //136
    char localFilepath[MOCLOUD_FILEPATH_MAXLEN] = "./tstDwldFile_2.Vedio";  //./tstDwldFile_3.Vedio
    ret = moCloudClient_startDownloadFile(key, fileSize, localFilepath);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_startDownloadFile failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_startDownloadFile SUCCEED.\n");

    sleep(30);
    
    ret = moCloudClient_logOut(username, passwd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_logOut failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_logOut SUCCEED.\n");
    
    moCloudClient_unInit();
    return 0;
}

int main(int arc, char **argv)
{
    moLoggerInit("./");

//    tst_All();

    tst_StartDwld();

    moLoggerUnInit();
    
    return 0;
}
