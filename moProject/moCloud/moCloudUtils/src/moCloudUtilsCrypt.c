#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "moCloudUtilsTypes.h"
#include "moCloudUtilsCrypt.h"


int moCloudUtilsCrypt_getCipherTxtLen(
    const MOCLOUD_CRYPT_ALGO algoId,
    const int srcLen,
    int * pDstLen)
{
    //TODO, just a stub 

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "A stub for moCloudUtilsCrypt_getCipherTxtLen.\n");
    *pDstLen = srcLen;
    
    return 0;
}

int moCloudUtilsCrypt_doCrypt(
    const MOCRYPT_METHOD method,
    const MOCLOUD_CRYPT_INFO cryptInfo,
    const char * pSrc,
    const int srcLen,
    char * pDst,
    int * pDstLen)
{
    //TODO, just a stub

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "A stub for moCloudUtilsCrypt_doCrypt.\n");
    memcpy(pDst, pSrc, srcLen);
    *pDstLen = srcLen;
    
    return 0;    
}

