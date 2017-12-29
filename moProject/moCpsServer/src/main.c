#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "moLogger.h"
#include "moCpsServer.h"
#include "moCpsUtils.h"

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        printf("Usage : %s confFilepath\n", argv[0]);
        return -1;
    }

    int ret = moLoggerInit("./");
    if(ret < 0)
    {
        printf("moLoggerInit failed! ret = %d\n", ret);
        return -2;
    }
    printf("moLoggerInit succeed.\n");

    ret = moCpsServ_init(argv[1]);
    if(ret < 0)
    {
        printf("moCpsServ_init failed! ret=%d\n", ret);
        moLoggerUnInit();
        return -3;
    }
    printf("moCpsServ_init succeed\n");

    ret = moCpsServ_startRunning();
    if(ret < 0)
    {
        printf("moCpsServ_startRunning failed! ret = %d\n", ret);
        moCpsServ_unInit();
        moLoggerUnInit();
        return -4;
    }

    sleep(6);

    ret = moCpsServ_stopRunning();
    if(ret < 0)
    {
        printf("moCpsServ_stopRunning failed! ret = %d\n", ret);
        moCpsServ_unInit();
        moLoggerUnInit();
        return -5;
    }
    
    moCpsServ_unInit();
    moLoggerUnInit();
    return 0;
}




































