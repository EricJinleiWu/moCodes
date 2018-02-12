#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "moCloudUtilsTypes.h"
#include "moCloudUtilsCheck.h"
#include "moCloudUtilsCrypt.h"
#include "moCloudClientLib.h"

/*
    1.read config file, get client ip(and port, not neccessary), get server ip and port;
    2.create socket;
    3.connect to server;
    4.do key agree;
    5.start heartbeat thread;
    6.get memory for ctrl orders, data must have another, but when we exec them, just do it then.
*/
int moCloudClient_init(const char * pCfgFilepath, int * pSockId)
{
    return 0;
}

/*
    1.stop heartbeat thread;
    2.say byebye to server;
    3.destroy socket;
    4.free memory;
*/
void moCloudClient_unInit()
{
    ;
}

/*
    Sign up to server;
*/
int moCloudClient_signUp(const char * pUsrName, const char * pPasswd)
{
    return 0;
}

/*
    Sign in to server;
*/
int moCloudClient_signIn(const char * pUsrName, const char * pPasswd)
{
    return 0;
}

/*
    These functions to get files info;
*/
int moCloudClient_getAllFileInfo(MOCLOUD_BASIC_FILEINFO * pAllFileInfo, int * pNum)
{
    return 0;
}

int moCloudClient_getOneTypeFileInfo(
    const MOCLOUD_FILETYPE type, MOCLOUD_BASIC_FILEINFO * pOneTypeFileInfo, int * pNum)
{
    return 0;
}

