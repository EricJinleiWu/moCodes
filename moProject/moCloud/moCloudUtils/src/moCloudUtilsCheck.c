#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "moCloudUtilsTypes.h"
#include "moCloudUtilsCheck.h"

#define CHECKSUM_STUB_VALUE (0xa)
#define CRC_STUB_VALUE  (0xa)

int moCloudUtilsCheck_checksumGetValue(
    const char * pSrc,
    const int srcLen,
    unsigned char * pSum)
{
    //TODO, Just a stub

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "just a stub for moCloudUtilsCheck_checksumGetValue.\n");
    *pSum = CHECKSUM_STUB_VALUE;

    return 0;
}

int moCloudUtilsCheck_checksumCheckValue(
    const char * pSrc,
    const int srcLen,
    const unsigned char sum)
{
    //TODO, Just a stub

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "just a stub for moCloudUtilsCheck_checksumCheckValue.\n");
    return (sum == CHECKSUM_STUB_VALUE) ? 1 : 0;
}


int moCloudUtilsCheck_crcGetValue(
    const MOUTILS_CHECK_CRCMETHOD method,
    const char * pSrc,
    const int srcLen,
    MOUTILS_CHECK_CRCVALUE * pCrcValue)
{
    //TODO, Just a stub

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "just a stub for moCloudUtilsCheck_crcGetValue.\n");

    return 0;
}


int moCloudUtilsCheck_crcCheckValue(
    const MOUTILS_CHECK_CRCMETHOD method,
    const char * pSrc,
    const int srcLen,
    const MOUTILS_CHECK_CRCVALUE crcValue)
{
    //TODO, Just a stub

    moLoggerDebug(MOCLOUD_MODULE_LOGGER_NAME, 
        "just a stub for moCloudUtilsCheck_crcCheckValue.\n");
    return 0;
}

