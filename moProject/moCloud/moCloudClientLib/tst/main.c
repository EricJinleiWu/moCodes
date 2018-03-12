#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "moLogger.h"
#include "moUtils.h"
#include "moCrypt.h"

#include "moCloudUtilsTypes.h"
#include "moCloudUtilsCheck.h"
#include "moCloudUtilsCrypt.h"
#include "moCloudClientLib.h"

static void dumpAllFileInfo(MOCLOUD_BASIC_FILEINFO_NODE * pAllFileInfo)
{
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

//    sleep(20);

//    moCloudClient_unInit();

//    return 0;

    char username[MOCLOUD_USERNAME_MAXLEN] = "testtest";
    char passwd[MOCLOUD_PASSWD_MAXLEN] = "123456";
    ret = moCloudClient_signUp(username, passwd);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_signUp failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -2;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_signUp SUCCEED.\n");

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
    sleep(30);

    MOCLOUD_BASIC_FILEINFO_NODE * pAllFileInfo = NULL;
    ret = moCloudClient_getFilelist(pAllFileInfo, MOCLOUD_FILETYPE_ALL);
    if(ret != 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME,
            "moCloudClient_signUp failed! ret=%d\n", ret);
        moCloudClient_unInit();
        return -4;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "moCloudClient_getAllFileInfo SUCCEED.\n");
    dumpAllFileInfo(pAllFileInfo);

    moCloudClient_freeFilelist(pAllFileInfo);
    

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

    tst_All();

    moLoggerUnInit();
    
    return 0;
}
