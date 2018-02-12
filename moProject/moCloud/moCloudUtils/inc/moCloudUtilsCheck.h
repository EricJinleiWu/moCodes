#ifndef __MOCLOUD_UTILS_CHECK_H__
#define __MOCLOUD_UTILS_CHECK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "moUtils.h"

/*
    Use checkSum method to do check, this function to get checkSum value;

    @pSrc : The src content which will be do check;
    @srcLen : the length of @pSrc;
    @pSum : the checksum value;

    return :
        0 : succeed;
        0-: failed;
*/
int moCloudUtilsCheck_checksumGetValue(
    const char * pSrc,
    const int srcLen,
    unsigned char * pSum);

/*
    Use checkSum method to do check, this function to check value right or not;

    @pSrc : The src content which will be do check;
    @srcLen : the length of @pSrc;
    @sum : the checksum value;

    return :
        1 : check ok, @sum is the checksum value of @pSrc;
        0 : check failed, or param error, or other errors;
*/
int moCloudUtilsCheck_checksumCheckValue(
    const char * pSrc,
    const int srcLen,
    const unsigned char sum);

/*
    Use CRC method to do check, this function to get crc value;

    @method : which CRC method being used;
    @pSrc : The src content which will be do check;
    @srcLen : the length of @pSrc;
    @pCrcValue : the crc value;

    return :
        0 : succeed;
        0-: failed;
*/
int moCloudUtilsCheck_crcGetValue(
    const MOUTILS_CHECK_CRCMETHOD method,
    const char * pSrc,
    const int srcLen,
    MOUTILS_CHECK_CRCVALUE * pCrcValue);

/*
    Use CRC method to do check, this function to check value right or not;

    @method : which CRC method being used;
    @pSrc : The src content which will be do check;
    @srcLen : the length of @pSrc;
    @crcValue : the checksum value;

    return :
        1 : check ok, @crcValue is the checksum value of @pSrc;
        0 : check failed, or param error, or other errors;
*/
int moCloudUtilsCheck_crcCheckValue(
    const MOUTILS_CHECK_CRCMETHOD method,
    const char * pSrc,
    const int srcLen,
    const MOUTILS_CHECK_CRCVALUE crcValue);

#ifdef __cplusplus
}
#endif

#endif