#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.d>
#include <sys/types.h>
#include <time.h>

using namespace std;
#include <string>
#include <list>
#include <map>

#include "moCloudUtils.h"
#include "moCloudUtilsTypes.h"
#include "moCloudUtilsCrypt.h"
#include "moCloudUtilsCheck.h"
#include "fileMgr.h"
#include "cliCtrl.h"

#include "moLogger.h"
#include "moUtils.h"
#include "cliMgr.h"

static int startServer(const char * pConfFilepath)
{
    return 0;
}

int main(int argc, char ** argv)
{
    if(argc != 3)
    {
        fprintf(stderr, "Usage : %s dirPath confFilepath\n\n", argv[0]);
        return -1;
    }

    int ret = moLoggerInit("./");
    if(ret < 0)
    {
        fprintf(stderr, "moLoggerInit failed! ret=%d.\n", ret);
        return -2;
    }

    FileMgrSingleton::getInstance()->modifyDirpath(argv[1]);
    ret = FileMgrSingleton::getInstance()->refreshFileinfoTable();
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "refreshFileinfoTable failed! ret=%d\n", ret);
        moLoggerUnInit();
        return -3;
    }
    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, "refreshFileinfoTable succeed.\n");

    CliMgrSingleton::getInstance()->start();

    ret = startServer(argv[2]);
    if(ret < 0)
    {
        moLoggerError(MOCLOUD_MODULE_LOGGER_NAME, 
            "startServer failed! ret=%d\n", ret);
        
        CliMgrSingleton::getInstance()->stop();
        CliMgrSingleton::getInstance()->join();
        moLoggerUnInit();
        return -4;
    }
    
    moLoggerInfo(MOCLOUD_MODULE_LOGGER_NAME, "Main process will exit now...\n");
    moLoggerUnInit();
    
    return 0;
}
